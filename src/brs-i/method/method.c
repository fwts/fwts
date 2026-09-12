/*
 * Copyright (C) 2026 Xiang W <wangxiang@iscas.ac.cn>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI) && defined(FWTS_ARCH_RISCV)

#include "fwts_acpi_object_eval.h"

/* Standard AML IDs for an ECAM-capable PCI host bridge. */
#define CID_PCI			"PNP0A03"
#define HID_ECAM		"PNP0A08"
#define HID_CPU			"ACPI0007"

/* Generic Register Descriptor (ACPI 6.4.3.7): 0x82 + length word + 12-byte GAS. */
#define AML_GENERIC_REG_DESC		0x82
#define AML_GENERIC_REG_MIN_LEN		15
#define ACPI_ADR_SPACE_FFH		0x7F
#define CPPC_FFH_TYPE_SBI		0x1
#define CPPC_FFH_TYPE_CSR		0x2

#define CPPC_V2_REV			2
#define CPPC_V3_REV			3
#define CPPC_V4_REV			4
#define CPPC_V2_NUM_ENT			21
#define CPPC_V3_NUM_ENT			23
#define CPPC_V4_NUM_ENT			25

/* Indices into the _CPC package (ACPI 8.4.6.1). */
#define CPC_IDX_NUM_ENTRIES		0
#define CPC_IDX_REVISION		1
#define CPC_IDX_HIGHEST_PERF		2
#define CPC_IDX_NOMINAL_PERF		3
#define CPC_IDX_LOW_NONLINEAR_PERF	4
#define CPC_IDX_LOWEST_PERF		5
#define CPC_IDX_DESIRED_PERF		7
#define CPC_IDX_REF_CTR			13
#define CPC_IDX_DELIVERED_CTR		14

static fwts_acpi_table_info *mtable;
static bool acpi_initialized;
static bool no_os_perf_ctrl;

static int method_brsi_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}
	acpi_initialized = true;

	return FWTS_OK;
}

static int method_brsi_deinit(fwts_framework *fw)
{
	if (acpi_initialized)
		return fwts_acpi_deinit(fw);
	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	char device_path[128];
	int io_num;
} method_brsi_rc;

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	unsigned int walk_failed_counter;
	unsigned int has_io_counter;
} method_brsi_rc_ctx;

static ACPI_STATUS method_brsi_crs_resource(ACPI_RESOURCE *resource, void *context)
{
	method_brsi_rc *rc = context;
	fwts_framework *fw = rc->fw;
	const char *desc = NULL;
	uint64_t min;
	uint64_t length;

	switch (resource->Type) {
	case ACPI_RESOURCE_TYPE_IO:
		desc = "IO";
		min = resource->Data.Io.Minimum;
		length = resource->Data.Io.AddressLength;
		break;
	case ACPI_RESOURCE_TYPE_FIXED_IO:
		desc = "FixedIO";
		min = resource->Data.FixedIo.Address;
		length = resource->Data.FixedIo.AddressLength;
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS16:
		if (resource->Data.Address16.ResourceType == ACPI_IO_RANGE) {
			desc = "WordIO";
			min = resource->Data.Address16.Address.Minimum;
			length = resource->Data.Address16.Address.AddressLength;
		}
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS32:
		if (resource->Data.Address32.ResourceType == ACPI_IO_RANGE) {
			desc = "DWordIO";
			min = resource->Data.Address32.Address.Minimum;
			length = resource->Data.Address32.Address.AddressLength;
		}
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS64:
		if (resource->Data.Address64.ResourceType == ACPI_IO_RANGE) {
			desc = "QWordIO";
			min = resource->Data.Address64.Address.Minimum;
			length = resource->Data.Address64.Address.AddressLength;
		}
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
		if (resource->Data.ExtAddress64.ResourceType == ACPI_IO_RANGE) {
			desc = "ExtendedIO";
			min = resource->Data.ExtAddress64.Address.Minimum;
			length = resource->Data.ExtAddress64.Address.AddressLength;
		}
		break;
	default:
		break;
	}

	if (desc) {
		rc->io_num++;
		fwts_log_info(fw,
			"PCIe Root Complex %s _CRS has I/O range descriptor %s "
			"(min=0x%" PRIx64 ", length=0x%" PRIx64 ").",
			rc->device_path[0] ? rc->device_path : "(unknown)",
			desc, min, length);
	}

	return AE_OK;
}

static void method_brsi_acpi_fullname(ACPI_HANDLE handle, char *out, size_t out_size)
{
	ACPI_BUFFER buf;

	if (!out || !out_size)
		return;

	buf.Pointer = out;
	buf.Length = out_size;
	out[0] = '\0';
	if (ACPI_FAILURE(AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf)))
		strncpy(out, "(unknown)", out_size - 1);
}

static ACPI_STATUS method_brsi_pci_host_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_rc_ctx *ctx = context;
	fwts_framework *fw = ctx->fw;
	method_brsi_rc rc;
	ACPI_STATUS status;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	memset(&rc, 0, sizeof(rc));
	rc.fw = fw;
	method_brsi_acpi_fullname(handle, rc.device_path, sizeof(rc.device_path));

	fwts_log_info(fw,
		"Checking _CRS of PCIe Root Complex %s (HID %s / CID %s).",
		rc.device_path, HID_ECAM, CID_PCI);

	status = AcpiWalkResources(handle, METHOD_NAME__CRS,
		method_brsi_crs_resource, &rc);
	if (ACPI_FAILURE(status)) {
		ctx->walk_failed_counter++;
		fwts_log_error(fw,
			"Failed to walk _CRS of %s: %s.",
			rc.device_path, AcpiFormatException(status));
	}

	if (rc.io_num)
		ctx->has_io_counter++;
	ctx->found++;

	return AE_OK;
}

static int method_brsi_aml010(fwts_framework *fw)
{
	method_brsi_rc_ctx ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	/*
	 * AcpiGetDevices() matches the given ID against both _HID and _CID,
	 * so PNP0A08 covers ECAM PCI Express host bridges.
	 */
	AcpiGetDevices(HID_ECAM, method_brsi_pci_host_walk, &ctx, NULL);

	if (ctx.found == 0) {
		fwts_skipped(fw,
			"AML_010: no PCIe Root Complex with _HID/_CID %s found; "
			"skipping I/O range check.",
			HID_ECAM);
		return FWTS_OK;
	}
	if (ctx.has_io_counter > 0) {
		fwts_warning(fw,
			"AML_010: %u of %u PCIe Root Complex _CRS return I/O "
			"range descriptors. BRS-I says _CRS SHOULD NOT return "
			"I/O ranges (WordIO, DWordIO, QWordIO, IO, FixedIO or "
			"ExtendedIO).",
			ctx.has_io_counter, ctx.found);
		fwts_advice(fw,
			"Legacy PCI I/O BARs are uncommon on modern PCIe devices "
			"and describing I/O space can complicate Root Complex "
			"configuration. Remove I/O descriptors from the Root "
			"Complex _CRS unless a specific device requires them.");
	}
	if (ctx.walk_failed_counter > 0) {
		fwts_warning(fw,
			"AML_010: failed to inspect _CRS of %u of %u PCIe "
			"Root Complex device(s); not treated as "
			"\"does not return I/O range descriptors\".",
			ctx.walk_failed_counter, ctx.found);
	}
	if (ctx.has_io_counter == 0 && ctx.walk_failed_counter == 0) {
		fwts_passed(fw,
			"AML_010: _CRS of %u PCIe Root Complex device(s) "
			"does not return I/O range descriptors.",
			ctx.found);
	}

	return FWTS_OK;
}

static int method_brsi_aml020(fwts_framework *fw)
{
	fwts_list *methods;
	fwts_list_link *item;
	bool found = false;
	bool lookup_failed = false;
	unsigned int candidates = 0;

	if ((methods = fwts_acpi_object_get_names()) != NULL) {
		fwts_list_foreach(item, methods) {
			char *name = fwts_list_data(char *, item);
			size_t len;
			ACPI_HANDLE handle;
			ACPI_OBJECT_TYPE type;
			ACPI_STATUS status;
			const char *which;

			if (name == NULL)
				continue;

			len = strlen(name);
			if (len < 4)
				continue;

			if (strncmp(name + len - 4, "_PRS", 4) == 0)
				which = "_PRS";
			else if (strncmp(name + len - 4, "_SRS", 4) == 0)
				which = "_SRS";
			else
				continue;

			status = AcpiGetHandle(NULL, name, &handle);
			if (ACPI_FAILURE(status)) {
				lookup_failed = true;
				candidates++;
				fwts_log_warning(fw, "AML_020: AcpiGetHandle(%s) failed: %s.",
					name, AcpiFormatException(status));
				continue;
			}

			status = AcpiGetType(handle, &type);
			if (ACPI_FAILURE(status)) {
				lookup_failed = true;
				candidates++;
				fwts_log_warning(fw, "AML_020: AcpiGetType(%s) failed: %s.",
					name, AcpiFormatException(status));
				continue;
			}

			/* Skip namespace scopes that happen to end in the same suffix. */
			if (type == ACPI_TYPE_LOCAL_SCOPE)
				continue;

			found = true;
			fwts_log_info(fw, "AML_020: found %s method %s.", which, name);
		}
	}

	if (found) {
		fwts_warning(fw,
			"AML_020: _PRS and/or _SRS methods are implemented. "
			"BRS-I says these methods SHOULD NOT be implemented.");
		fwts_advice(fw,
			"ACPI resource descriptors are typically used for "
			"devices with fixed resource ranges. Flexible resource "
			"assignment via _PRS/_SRS is not supported by most "
			"modern ACPI operating systems. Remove these methods "
			"unless a device truly requires runtime rebalancing. "
			"Note that PCI Interrupt Link devices (PNP0C0F) require "
			"_PRS/_SRS by definition; the usual fix is to use static "
			"GSIs in the _PRT.");
	} else if (lookup_failed) {
		fwts_warning(fw,
			"AML_020: found %u name(s) ending in _PRS/_SRS, but "
			"AcpiGetHandle/AcpiGetType failed; not treated as "
			"\"no methods are implemented\".", candidates);
	} else {
		fwts_passed(fw,
			"AML_020: no _PRS or _SRS methods are implemented.");
	}

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	unsigned int under_sb;
	unsigned int under_pr;
	unsigned int elsewhere;
	unsigned int uid_mismatch;
} method_brsi_cpu_ctx;

static fwts_list madt_rintc_uids;

static int method_brsi_collect_rintc_uids(fwts_framework *fw)
{
	uint8_t *data;
	ssize_t length;
	uint32_t *uid;
	fwts_acpi_madt_rintc *rintc;

	data = mtable->data;
	length = mtable->length;
	if (length < (ssize_t)sizeof(fwts_acpi_table_madt)) {
		fwts_log_error(fw, "AML_030: MADT is truncated.");
		return FWTS_ERROR;
	}

	data += sizeof(fwts_acpi_table_madt);
	length -= sizeof(fwts_acpi_table_madt);

	while (length > (ssize_t)sizeof(fwts_acpi_madt_sub_table_header)) {
		fwts_acpi_madt_sub_table_header *hdr =
			(fwts_acpi_madt_sub_table_header *)data;
		ssize_t body;

		if (hdr->length < sizeof(*hdr) || hdr->length > length) {
			fwts_log_error(fw,
				"AML_030: The MADT subtable header Length is incorrect.");
			return FWTS_ERROR;
		}

		data += sizeof(*hdr);
		length -= sizeof(*hdr);
		body = hdr->length - sizeof(*hdr);

		if (hdr->type == FWTS_MADT_RINTC) {
			if (body < (ssize_t) sizeof(fwts_acpi_madt_rintc)) {
				fwts_log_error(fw,
					"AML_030: The MADT RINTC subtable header Length is too small.");
				return FWTS_ERROR;
			}
			rintc = (fwts_acpi_madt_rintc* )data;
			uid = malloc(sizeof(*uid));
			if (!uid) {
				fwts_log_error(fw, "AML_030: failed to allocate memory for RINTC UID.");
				return FWTS_OUT_OF_MEMORY;
			}
			*uid = rintc->uid;
			fwts_list_append(&madt_rintc_uids, uid);
		}
		data += body;
		length -= body;
	}
	return FWTS_OK;
}

static bool method_brsi_eval_uid(ACPI_HANDLE handle, uint64_t *out)
{
	bool rc = false;
	ACPI_STATUS status;
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;

	status = AcpiEvaluateObject(handle, "_UID", NULL, &buf);
	if (ACPI_FAILURE(status) || buf.Pointer == NULL)
		return false;

	obj = buf.Pointer;
	if (obj->Type == ACPI_TYPE_INTEGER) {
		*out = obj->Integer.Value;
		rc = true;
	} else if(obj->Type == ACPI_TYPE_STRING) {
		char *end = NULL;
		unsigned long long v;
		v = strtoull(obj->String.Pointer, &end, 0);
		if (end != obj->String.Pointer && end != NULL && *end == '\0') {
			*out = v;
			rc = true;
		}
	}

	free(buf.Pointer);
	return rc;
}

static ACPI_STATUS method_brsi_cpu_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_cpu_ctx *ctx = context;
	uint64_t uid;
	char path[128];

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	method_brsi_acpi_fullname(handle, path, sizeof(path));
	fwts_log_info(ctx->fw, "AML_030: found %s at %s.", HID_CPU, path);

	if (strncmp(path, "\\_SB", 4) == 0)
		ctx->under_sb++;
	else if (strncmp(path, "\\_PR", 4) == 0)
		ctx->under_pr++;
	else
		ctx->elsewhere++;

	if (!method_brsi_eval_uid(handle, &uid)) {
		fwts_log_error(ctx->fw,
			"AML_030: failed to evaluate _UID for per-hart object at %s.",
			path);
	} else {
		bool found = false;
		fwts_list_link *item;
		fwts_list_foreach(item, &madt_rintc_uids) {
			if (*(uint32_t*)item->data == uid) {
				found = true;
				break;
			}
		}
		if (!found) {
			fwts_log_error(ctx->fw,
				"AML_030: No matching RINTC found for %s(%s) in the MADT table",
				HID_CPU, path);
			ctx->uid_mismatch++;
		}
	}

	return AE_OK;
}

static int method_brsi_aml030(fwts_framework *fw)
{
	int rintc_count;
	method_brsi_cpu_ctx ctx;

	if (fwts_acpi_find_table(fw, "APIC", 0, &mtable) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_030",
			"Cannot find ACPI MADT tables.");
		return FWTS_OK;
	}

	if (!mtable || mtable->length == 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_030",
			"Required ACPI MADT (APIC) table not found");
		return FWTS_OK;
	}

	fwts_list_init(&madt_rintc_uids);
	if (method_brsi_collect_rintc_uids(fw) != FWTS_OK) {
		fwts_list_free_items(&madt_rintc_uids, free);
		return FWTS_OK;
	}
	rintc_count = fwts_list_len(&madt_rintc_uids);

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_CPU, method_brsi_cpu_walk, &ctx, NULL);
	AcpiWalkNamespace(ACPI_TYPE_PROCESSOR, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
		method_brsi_cpu_walk, NULL, &ctx, NULL);

	if (ctx.under_pr || ctx.elsewhere) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_030",
			"Per-hart %s objects must be under \\_SB, "
			"not deprecated \\_PR (%u under \\_PR, %u elsewhere, "
			"%u under \\_SB).",
			HID_CPU, ctx.under_pr, ctx.elsewhere, ctx.under_sb);
	} else if (ctx.under_sb == 0 && rintc_count > 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_030",
			"MADT has %u RINTC entr%s but no %s per-hart objects "
			"found under \\_SB.", rintc_count,
			rintc_count == 1 ? "y" : "ies", HID_CPU);
	} else if (ctx.uid_mismatch || ctx.under_sb != rintc_count) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_030",
			"Per-hart objects under \\_SB (%u) do not match MADT RINTC "
			"entries (%u): %u _UID mismatch%s.", ctx.under_sb, rintc_count,
			ctx.uid_mismatch,ctx.uid_mismatch == 1 ? "" : "es");
	} else if (ctx.under_sb) {
		fwts_passed(fw,
			"AML_030: %s per-hart objects are under \\_SB.",
			HID_CPU);
	} else {
		fwts_skipped(fw,
			"AML_030: no %s per-hart objects found.",
			HID_CPU);
	}

	fwts_list_free_items(&madt_rintc_uids, free);
	return FWTS_OK;
}


typedef struct {
	fwts_framework *fw;
	unsigned int harts;
	unsigned int with_cpc;
	unsigned int absent;
	unsigned int failed;
} method_brsi_cpc_ctx;

/*
 * Parse an AML Generic Register Descriptor Buffer into a GAS.
 * A NULL register is SystemMemory with width/offset/access/address all 0.
 */
static bool method_brsi_cpc_parse_reg(
	const ACPI_OBJECT *obj,
	fwts_acpi_gas *gas,
	bool *is_null)
{
	uint8_t *p;

	if (obj == NULL || obj->Type != ACPI_TYPE_BUFFER ||
	    obj->Buffer.Pointer == NULL ||
	    obj->Buffer.Length < AML_GENERIC_REG_MIN_LEN)
		return false;

	p = obj->Buffer.Pointer;
	if (p[0] != AML_GENERIC_REG_DESC)
		return false;

	memcpy(gas, p + 3, sizeof(*gas));
	*is_null = (gas->address_space_id == 0 &&
		    gas->register_bit_width == 0 &&
		    gas->register_bit_offset == 0 &&
		    gas->access_width == 0 &&
		    gas->address == 0);
	return true;
}

static bool method_brsi_cpc_perf_usable(
	fwts_framework *fw,
	const char *path,
	const char *field,
	const ACPI_OBJECT *obj)
{
	fwts_acpi_gas gas;
	bool is_null = true;

	if (obj->Type == ACPI_TYPE_INTEGER) {
		if (obj->Integer.Value == 0) {
			fwts_log_error(fw,
				"AML_040: %s.%s is Integer 0 (unsupported).",
				path, field);
			return false;
		}
		fwts_log_info(fw,
			"AML_040: %s.%s = Integer 0x%" PRIx64 ".",
			path, field, (uint64_t)obj->Integer.Value);
		return true;
	}

	if (!method_brsi_cpc_parse_reg(obj, &gas, &is_null) || is_null) {
		fwts_log_error(fw,
			"AML_040: %s.%s is missing, malformed or a NULL "
			"register.",
			path, field);
		return false;
	}

	fwts_log_info(fw,
		"AML_040: %s.%s register space=0x%" PRIx8
		" width=%u addr=0x%" PRIx64 ".",
		path, field, gas.address_space_id,
		gas.register_bit_width, (uint64_t)gas.address);
	return true;
}

static bool method_brsi_cpc_reg_usable(
	fwts_framework *fw,
	const char *path,
	const char *field,
	const ACPI_OBJECT *obj)
{
	fwts_acpi_gas gas;
	bool is_null = true;

	if (!method_brsi_cpc_parse_reg(obj, &gas, &is_null)) {
		fwts_log_error(fw,
			"AML_040: %s.%s is not a Generic Register Descriptor.",
			path, field);
		return false;
	}
	if (is_null) {
		fwts_log_error(fw,
			"AML_040: %s.%s is a NULL register; OSPM cannot use it.",
			path, field);
		return false;
	}

	fwts_log_info(fw,
		"AML_040: %s.%s register space=0x%" PRIx8
		" width=%u addr=0x%" PRIx64 ".",
		path, field, gas.address_space_id,
		gas.register_bit_width, (uint64_t)gas.address);

	/*
	 * RISC-V FFH: FFixedHW descriptors must use Table 5/6 encoding
	 * (SBI CPPC type 0x1 or CSR type 0x2). Other spaces (SystemMemory)
	 * are allowed by ACPI and not forbidden by BRS AML_040.
	 */
	if (gas.address_space_id == ACPI_ADR_SPACE_FFH) {
		uint64_t addr = gas.address;
		uint8_t type = (addr >> 60) & 0xf;
		uint32_t mid = (addr >> 32) & 0xfffffff;

		if (gas.register_bit_width != 64 ||
		    gas.register_bit_offset != 0 ||
		    gas.access_width != 4) {
			fwts_log_error(fw,
				"AML_040: %s.%s FFixedHW descriptor must be "
				"64-bit, offset 0, AccessSize QWord.",
				path, field);
			return false;
		}
		if (mid != 0) {
			fwts_log_error(fw,
				"AML_040: %s.%s FFixedHW bits[59:32] must be 0.",
				path, field);
			return false;
		}
		if (type != CPPC_FFH_TYPE_SBI && type != CPPC_FFH_TYPE_CSR) {
			fwts_log_error(fw,
				"AML_040: %s.%s FFixedHW type 0x%x is reserved "
				"(want 0x1 SBI CPPC or 0x2 CSR).",
				path, field, type);
			return false;
		}
		if (type == CPPC_FFH_TYPE_CSR && ((addr >> 12) & 0xfffff) != 0) {
			fwts_log_error(fw,
				"AML_040: %s.%s CSR FFixedHW bits[31:12] "
				"must be 0.",
				path, field);
			return false;
		}
	}

	return true;
}

static void method_brsi_cpc_check(
	method_brsi_cpc_ctx *ctx,
	ACPI_HANDLE handle,
	const char *path)
{
	fwts_framework *fw = ctx->fw;
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;
	ACPI_STATUS status;
	uint64_t nent, rev, expected = 0;

	status = AcpiEvaluateObject(handle, "_CPC", NULL, &buf);
	if (ACPI_FAILURE(status) || buf.Pointer == NULL) {
		fwts_log_warning(fw,
			"AML_040: %s._CPC is missing or failed to evaluate "
			"(%s).",
			path, AcpiFormatException(status));
		ctx->absent++;
		return;
	}

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE || obj->Package.Count < 2) {
		fwts_log_error(fw,
			"AML_040: %s._CPC is not a usable Package (type %u, "
			"count=%u); see the generic method _CPC test.",
			path, obj->Type, obj->Type == ACPI_TYPE_PACKAGE ?
				obj->Package.Count : 0);
		goto FAILED;
	}

	if (obj->Package.Elements[CPC_IDX_NUM_ENTRIES].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_040: %s._CPC NumEntries is not an Integer (type %u).",
			path, obj->Package.Elements[CPC_IDX_NUM_ENTRIES].Type);
		goto FAILED;
	}

	if (obj->Package.Elements[CPC_IDX_REVISION].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_040: %s._CPC Revision is not an Integer (type %u).",
			path, obj->Package.Elements[CPC_IDX_REVISION].Type);
		goto FAILED;
	}

	nent = obj->Package.Elements[CPC_IDX_NUM_ENTRIES].Integer.Value;
	rev = obj->Package.Elements[CPC_IDX_REVISION].Integer.Value ;

	fwts_log_info(fw,
		"AML_040: %s._CPC revision=%" PRIu64 " entries=%" PRIu64
		" (package count %u).",
		path, rev, nent, obj->Package.Count);

	if (rev == CPPC_V2_REV)
		expected = CPPC_V2_NUM_ENT;
	else if (rev == CPPC_V3_REV)
		expected = CPPC_V3_NUM_ENT;
	else if (rev == CPPC_V4_REV)
		expected = CPPC_V4_NUM_ENT;
	else {
		/* BRS AML_040 requires CPPC; ACPI allows revision 1, BRS does not. */
		fwts_log_error(fw,
			"AML_040: %s._CPC revision %" PRIu64
			" is not 2, 3 or 4.",
			path, rev);
		goto FAILED;
	}

	if (expected && (nent != expected || obj->Package.Count < nent)) {
		fwts_log_error(fw,
			"AML_040: %s._CPC NumEntries (%" PRIu64 ") or package count (%u) "
			"does not match revision %" PRIu64 " (expected %" PRIu64 ").",
			path, nent, obj->Package.Count, rev, expected);
		goto FAILED;
	}

	/* Required fields go up to the Delivered counter (index 14). */
	if (obj->Package.Count < CPC_IDX_DELIVERED_CTR) {
		fwts_log_error(fw,
			"AML_040: %s._CPC package too short (count=%u, need >%u) "
			"to contain Highest/Nominal/LowestNonlinear/LowestPerformance "
			"and the Reference/Delivered counter registers.",
			path, obj->Package.Count, CPC_IDX_DELIVERED_CTR);
		goto FAILED;
	}

	if (!method_brsi_cpc_perf_usable(fw, path, "HighestPerformance",
		&obj->Package.Elements[CPC_IDX_HIGHEST_PERF]))
		goto FAILED;
	if (!method_brsi_cpc_perf_usable(fw, path, "NominalPerformance",
		&obj->Package.Elements[CPC_IDX_NOMINAL_PERF]))
		goto FAILED;
	if (!method_brsi_cpc_perf_usable(fw, path, "LowestNonlinearPerformance",
		&obj->Package.Elements[CPC_IDX_LOW_NONLINEAR_PERF]))
		goto FAILED;
	if (!method_brsi_cpc_perf_usable(fw, path, "LowestPerformance",
		&obj->Package.Elements[CPC_IDX_LOWEST_PERF]))
		goto FAILED;
	if (!method_brsi_cpc_reg_usable(fw, path,
		"ReferencePerformanceCounterRegister",
		&obj->Package.Elements[CPC_IDX_REF_CTR]))
		goto FAILED;
	if (!method_brsi_cpc_reg_usable(fw, path,
		"DeliveredPerformanceCounterRegister",
		&obj->Package.Elements[CPC_IDX_DELIVERED_CTR]))
		goto FAILED;

	ctx->with_cpc++;
	free(buf.Pointer);
	return;
FAILED:
	ctx->failed++;
	free(buf.Pointer);
	return;
}

static ACPI_STATUS method_brsi_cpc_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_cpc_ctx *ctx = context;
	char device_path[128];

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->harts++;
	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_040: found per-hart device %s.",
		device_path);

	method_brsi_cpc_check(ctx, handle, device_path);

	return AE_OK;
}

static unsigned int method_brsi_count_pstate(fwts_framework *fw)
{
	fwts_list *methods;
	fwts_list_link *item;
	unsigned int n = 0;

	methods = fwts_acpi_object_get_names();
	if (methods == NULL)
		return 0;

	fwts_list_foreach(item, methods) {
		char *name = fwts_list_data(char *, item);
		size_t len;

		if (name == NULL)
			continue;
		len = strlen(name);
		if (len < 4)
			continue;
		if (strncmp(name + len - 4, "_PCT", 4) != 0 &&
		    strncmp(name + len - 4, "_PSS", 4) != 0 &&
		    strncmp(name + len - 4, "_PPC", 4) != 0)
			continue;

		fwts_log_info(fw,
			"AML_040: found legacy P-state method %s.", name);
		n++;
	}

	return n;
}

/* OSC_SB_CPCV2_SUPPORT from Linux include/linux/acpi.h */
#define OSC_SB_CPCV2_SUPPORT	0x00000040

/* System Bus _OSC UUID: 0811B06E-4A27-44F9-8D60-3CBBC22E7B48 */
static uint8_t sb_osc_uuid[16] = {
	0x6e, 0xb0, 0x11, 0x08, 0x27, 0x4a, 0xf9, 0x44,
	0x8d, 0x60, 0x3c, 0xbb, 0xc2, 0x2e, 0x7b, 0x48
};

/*
 * Optional AML_040 check: Linux only uses _CPC after \_SB._OSC
 * acknowledges OSC_SB_CPCV2_SUPPORT (unless cpc_supported_by_cpu()).
 * Missing \_SB._OSC is common and not a failure.
 */
static void method_brsi_check_sb_osc_cppc2(fwts_framework *fw, bool has_cpc)
{
	ACPI_STATUS status;
	ACPI_OBJECT args[4];
	ACPI_OBJECT_LIST arg_list = { 4, args };
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *ret;
	uint32_t caps = 0;
	bool acked = false;

	/* Build a minimal query _OSC call */
	args[0].Type = ACPI_TYPE_BUFFER;
	args[0].Buffer.Length = 16;
	args[0].Buffer.Pointer = (uint8_t *)sb_osc_uuid;

	args[1].Type = ACPI_TYPE_INTEGER;
	args[1].Integer.Value = 1;		/* Revision */

	args[2].Type = ACPI_TYPE_INTEGER;
	args[2].Integer.Value = 2;		/* Count of capabilities DWORDs */

	/* Query flag set, ask for CPC v2 */
	uint32_t cap_in[2] = { 0x1, OSC_SB_CPCV2_SUPPORT };
	args[3].Type = ACPI_TYPE_BUFFER;
	args[3].Buffer.Length = sizeof(cap_in);
	args[3].Buffer.Pointer = (uint8_t *)cap_in;

	status = AcpiEvaluateObject(NULL, "\\_SB._OSC", &arg_list, &buf);
	if (ACPI_FAILURE(status) || !buf.Pointer) {
		fwts_log_error(fw,
			"AML_040: \\_SB._OSC evaluation failed (%s); "
			"skipping CPPC v2 acknowledgement check.",
			AcpiFormatException(status));
		return;
	}

	ret = buf.Pointer;
	if (ret->Type == ACPI_TYPE_BUFFER &&
	    ret->Buffer.Length >= 8 &&
	    ret->Buffer.Pointer) {
		/* Capabilities DWORD is the second DWORD (index 1) */
		caps = ((uint32_t *)ret->Buffer.Pointer)[1];
		acked = !!(caps & OSC_SB_CPCV2_SUPPORT);
	}

	free(buf.Pointer);

	if (acked) {
		fwts_log_info(fw,
			"AML_040: \\_SB._OSC acknowledged OSC_SB_CPCV2_SUPPORT.");
	} else if (has_cpc) {
		fwts_warning(fw,
			"AML_040: _CPC is present but \\_SB._OSC did not "
			"acknowledge OSC_SB_CPCV2_SUPPORT (0x40). "
			"Linux may ignore these _CPC objects unless the CPU "
			"reports support via cpc_supported_by_cpu().");
		fwts_advice(fw,
			"Set the CPPC v2 bit in the capabilities returned by "
			"\\_SB._OSC if the platform intends the OS to use CPPC.");
	} else {
		fwts_log_info(fw,
			"AML_040: \\_SB._OSC present but OSC_SB_CPCV2_SUPPORT "
			"not set (no _CPC found either).");
	}
}

static int method_brsi_aml040(fwts_framework *fw)
{
	method_brsi_cpc_ctx ctx;
	unsigned int pstate;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_CPU, method_brsi_cpc_walk, &ctx, NULL);
	AcpiWalkNamespace(ACPI_TYPE_PROCESSOR, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                  method_brsi_cpc_walk, NULL, &ctx, NULL);

	method_brsi_check_sb_osc_cppc2(fw, ctx.with_cpc > 0 && !ctx.failed);

	pstate = method_brsi_count_pstate(fw);

	if (ctx.harts == 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_040",
			"No per-hart "HID_CPU" or Processor() objects found. "
			"This should not happen on a valid RISC-V ACPI system.");
		return FWTS_OK;
	}

	if (ctx.failed) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_040",
			"%u of %u per-hart object(s) have a malformed or "
			"invalid _CPC implementation.",
			ctx.failed, ctx.harts);
		return FWTS_OK;
	}

	if (pstate) {
		if (ctx.absent)
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_040",
				"Found legacy P-state methods (OS-directed "
				"performance control is supported), but %u of "
				"%u per-hart object(s) are missing _CPC.",
				ctx.absent, ctx.harts);
		else
			fwts_passed(fw,
				"AML_040: %u per-hart object(s) expose usable "
				"CPPC (_CPC).",
				ctx.with_cpc);
	} else {
		if (no_os_perf_ctrl)
			fwts_skipped(fw,
				"AML_040: --brs-i-no-os-perf-ctrl specified; "
				"platform does not support OS-directed "
				"performance control.");
		else if (ctx.with_cpc == 0)
			fwts_warning(fw,
				"AML_040: no usable _CPC and no legacy P-states "
				"found. If the platform does not support "
				"OS-directed performance control, re-run with "
				"--brs-i-no-os-perf-ctrl.");
		else if (ctx.absent)
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_040",
				"%u of %u per-hart object(s) expose usable "
				"_CPC, but %u are missing it. "
				"Incomplete CPPC implementation.",
				ctx.with_cpc, ctx.harts, ctx.absent);
		else
			fwts_passed(fw,
				"AML_040: %u per-hart object(s) expose usable "
				"CPPC (_CPC).",
				ctx.with_cpc);
	}

	return FWTS_OK;
}

static int options_handler(
	fwts_framework *fw,
	int argc,
	char * const argv[],
	int option_char,
	int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);

	if (option_char == 0) {
		switch (long_index) {
		case 0:	/* --brs-i-no-os-perf-ctrl */
			no_os_perf_ctrl = true;
			break;
		}
	}
	return FWTS_OK;
}

static fwts_option options[] = {
	{ "brs-i-no-os-perf-ctrl", "", 0,
	  "Platform has no OS-directed hart performance control (skip AML_040)" },
	{ NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test method_brsi_tests[] = {
	{ method_brsi_aml010,
	  "AML_010: PCIe Root Complex _CRS SHOULD NOT return I/O ranges." },
	{ method_brsi_aml020,
	  "AML_020: _PRS and _SRS methods SHOULD NOT be implemented." },
	{ method_brsi_aml030,
	  "AML_030: per-hart devices MUST be under \\_SB, not \\_PR." },
	{ method_brsi_aml040,
	  "AML_040: OS-directed hart performance control MUST use CPPC (_CPC)." },
	{ NULL, NULL }
};

static fwts_framework_ops method_brsi_ops = {
	.description     = "RISC-V BRS-I ACPI Methods and Objects test.",
	.init            = method_brsi_init,
	.deinit          = method_brsi_deinit,
	.minor_tests     = method_brsi_tests,
	.options         = options,
	.options_handler = options_handler
};

FWTS_REGISTER("method_brsi", &method_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
