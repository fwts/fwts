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
#include <acmacros.h>
#include <aclocal.h>
#include <acobject.h>
#include <acstruct.h>
#include <acutils.h>
#include <fwts_uefi.h>
#include <fwts_efi_module.h>

/* Standard AML IDs for an ECAM-capable PCI host bridge. */
#define CID_PCI			"PNP0A03"
#define HID_ECAM		"PNP0A08"
#define HID_CPU			"ACPI0007"
#define HID_CONTAINER		"ACPI0010"
/* ACPI Time and Alarm Device, same HID as src/acpi/devices/time/time.c. */
#define HID_TAD			"ACPI000E"

/* RISC-V FFH LPI entry method types (bits[63:60]). */
#define LPI_FFH_TYPE_WFI		0x0
#define LPI_FFH_TYPE_SBI_HSM		0x1

/* Indices into the _LPI package (ACPI 8.4.4.3). */
#define LPI_IDX_REVISION		0
#define LPI_IDX_LEVELID			1
#define LPI_IDX_COUNT			2
#define LPI_STATE_FIRST			3
#define LPI_STATE_MIN_ENT		7
#define LPI_ST_MIN_RESIDENCY		0
#define LPI_ST_WAKE_LATENCY		1
#define LPI_ST_FLAGS			2
#define LPI_ST_ARCH_FLAGS		3
#define LPI_ST_RES_FREQ			4
#define LPI_ST_PARENT_STATE		5
#define LPI_ST_ENTRY_METHOD		6
#define LPI_ST_RES_COUNTER		7
#define LPI_ST_USAGE_COUNTER		8
#define LPI_ST_NAME			9
#define LPI_ARCH_FLAGS_RESERVED_MASK	(~0x1ULL)

#define SBI_HSM_SUSP_NON_RET_BIT		0x80000000
#define SBI_HSM_SUSP_PLAT_BASE			0x10000000
#define SBI_HSM_SUSPEND_RET_DEFAULT		0x00000000
#define SBI_HSM_SUSPEND_RET_PLATFORM		SBI_HSM_SUSP_PLAT_BASE
#define SBI_HSM_SUSPEND_NON_RET_DEFAULT		SBI_HSM_SUSP_NON_RET_BIT
#define SBI_HSM_SUSPEND_NON_RET_PLATFORM	(SBI_HSM_SUSP_NON_RET_BIT | \
						 SBI_HSM_SUSP_PLAT_BASE)

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
static bool no_os_idle_states;
static bool no_os_perf_ctrl;
static bool no_osbus_rtc;

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

/*
 * Evaluate `name` under `handle`.
 * Optional `args` / `arg_count` are passed through (used by _SRT).
 *
 * On success:
 *   Integer                      -> *value = Integer.Value
 *   Buffer named "_GRT"          -> *value = Buffer.Length
 *   numeric String named "_UID"  -> *value = parsed number
 */
static bool method_brsi_eval(
	ACPI_HANDLE handle,
	char *name,
	ACPI_OBJECT *args,
	UINT32 arg_count,
	uint64_t *value)
{
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT_LIST arg_list;
	ACPI_OBJECT *obj;
	ACPI_STATUS status;
	bool rc = false;

	if (args && arg_count) {
		arg_list.Count = arg_count;
		arg_list.Pointer = args;
		status = AcpiEvaluateObject(handle, name, &arg_list, &buf);
	} else {
		status = AcpiEvaluateObject(handle, name, NULL, &buf);
	}
	if (ACPI_FAILURE(status) || buf.Pointer == NULL)
		return false;

	obj = buf.Pointer;
	if (obj->Type == ACPI_TYPE_INTEGER) {
		rc = true;
		if (value)
			*value = obj->Integer.Value;
	} else if (obj->Type == ACPI_TYPE_BUFFER && !strcmp(name, "_GRT")) {
		rc = true;
		if (value)
			*value = obj->Buffer.Length;
	} else if(obj->Type == ACPI_TYPE_STRING && !strcmp(name, "_UID")) {
		char *end = NULL;
		unsigned long long v;
		v = strtoull(obj->String.Pointer, &end, 0);
		if (end != obj->String.Pointer && end != NULL && *end == '\0') {
			rc = true;
			if(value)
				*value = v;
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

	if (!method_brsi_eval(handle, "_UID", NULL, 0, &uid)) {
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

/* OSC_SB_CPCV2_SUPPORT / OSC_SB_PCLPI_SUPPORT from Linux include/linux/acpi.h */
#define OSC_SB_CPCV2_SUPPORT	0x00000040
#define OSC_SB_PCLPI_SUPPORT	0x00000080

/* System Bus _OSC UUID: 0811B06E-4A27-44F9-8D60-3CBBC22E7B48 */
static uint8_t sb_osc_uuid[16] = {
	0x6e, 0xb0, 0x11, 0x08, 0x27, 0x4a, 0xf9, 0x44,
	0x8d, 0x60, 0x3c, 0xbb, 0xc2, 0x2e, 0x7b, 0x48
};

/*
 * Optional check: Linux only uses _CPC / _LPI after
 * \_SB._OSC acknowledges the corresponding capability bit.
 * Missing \_SB._OSC is common and not a failure.
 *
 * bit:        OSC_SB_CPCV2_SUPPORT or OSC_SB_PCLPI_SUPPORT
 * has_object: true if usable _CPC / _LPI was found
 * tag:        "AML_040" or "AML_050"
 * what:       "CPPC v2" or "Processor Container LPI"
 */
static void method_brsi_check_sb_osc(
	fwts_framework *fw,
	uint32_t bit,
	bool has_object,
	const char *tag,
	const char *what)
{
	ACPI_STATUS status;
	ACPI_OBJECT args[4];
	ACPI_OBJECT_LIST arg_list = { 4, args };
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *ret;
	uint32_t caps = 0;
	bool acked = false;
	uint32_t cap_in[2] = { 0x1, bit };	/* Query flag + requested bit */

	args[0].Type = ACPI_TYPE_BUFFER;
	args[0].Buffer.Length = 16;
	args[0].Buffer.Pointer = (uint8_t *)sb_osc_uuid;

	args[1].Type = ACPI_TYPE_INTEGER;
	args[1].Integer.Value = 1;		/* Revision */

	args[2].Type = ACPI_TYPE_INTEGER;
	args[2].Integer.Value = 2;		/* Count of capabilities DWORDs */

	args[3].Type = ACPI_TYPE_BUFFER;
	args[3].Buffer.Length = sizeof(cap_in);
	args[3].Buffer.Pointer = (uint8_t *)cap_in;

	status = AcpiEvaluateObject(NULL, "\\_SB._OSC", &arg_list, &buf);
	if (ACPI_FAILURE(status) || !buf.Pointer) {
		fwts_log_error(fw,
			"%s: \\_SB._OSC evaluation failed (%s); "
			"skipping %s acknowledgement check.",
			tag, AcpiFormatException(status), what);
		return;
	}

	ret = buf.Pointer;
	if (ret->Type == ACPI_TYPE_BUFFER &&
	    ret->Buffer.Length >= 8 &&
	    ret->Buffer.Pointer) {
		caps = ((uint32_t *)ret->Buffer.Pointer)[1];
		acked = !!(caps & bit);
	}
	free(buf.Pointer);

	if (acked) {
		fwts_log_info(fw,
			"%s: \\_SB._OSC acknowledged %s (0x%x).",
			tag, what, bit);
	} else if (has_object) {
		fwts_warning(fw,
			"%s: %s objects are present but \\_SB._OSC did not "
			"acknowledge the corresponding capability bit (0x%x). "
			"Linux may ignore them.",
			tag, what, bit);
		fwts_advice(fw,
			"Set the %s bit in the capabilities returned by "
			"\\_SB._OSC if the platform intends the OS to use it.",
			what);
	} else {
		fwts_log_info(fw,
			"%s: \\_SB._OSC present but %s not set "
			"(no corresponding objects found either).",
			tag, what);
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

	method_brsi_check_sb_osc(fw, OSC_SB_CPCV2_SUPPORT,
		ctx.with_cpc > 0 && !ctx.failed,
		"AML_040", "CPPC v2");

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

typedef struct {
	fwts_framework *fw;
	unsigned int harts;
	unsigned int containers;
	unsigned int with_lpi;
	unsigned int absent;
	unsigned int failed;
} method_brsi_lpi_ctx;

static bool method_brsi_sbi_suspend_state_is_valid(uint32_t state)
{
	if (state > SBI_HSM_SUSPEND_RET_DEFAULT &&
	    state < SBI_HSM_SUSPEND_RET_PLATFORM)
		return false;

	if (state > SBI_HSM_SUSPEND_NON_RET_DEFAULT &&
	    state < SBI_HSM_SUSPEND_NON_RET_PLATFORM)
		return false;

	return true;
}

static bool method_brsi_lpi_entry_ok(
	fwts_framework *fw,
	const char *path,
	unsigned int state,
	const ACPI_OBJECT *obj)
{
	fwts_acpi_gas gas;
	bool is_null = true;
	uint64_t addr;
	uint8_t type;
	uint32_t mid;
	uint32_t suspend_type;

	if (!method_brsi_cpc_parse_reg(obj, &gas, &is_null) || is_null) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u Entry Method is missing, "
			"malformed or a NULL register.",
			path, state);
		return false;
	}

	fwts_log_info(fw,
		"AML_050: %s._LPI state %u Entry Method space=0x%" PRIx8
		" width=%u addr=0x%" PRIx64 ".",
		path, state, gas.address_space_id,
		gas.register_bit_width, (uint64_t)gas.address);

	if (gas.address_space_id != ACPI_ADR_SPACE_FFH)
		return false;

	addr = gas.address;
	type = (addr >> 60) & 0xf;
	mid = (addr >> 32) & 0xfffffff;
	suspend_type = addr & 0xffffffff;

	if (gas.register_bit_width != 64 ||
	    gas.register_bit_offset != 0 ||
	    gas.access_width != 4) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u FFixedHW Entry Method "
			"must be 64-bit, offset 0, AccessSize QWord.",
			path, state);
		return false;
	}
	if (mid != 0) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u FFixedHW bits[59:32] "
			"must be 0.",
			path, state);
		return false;
	}
	if (type != LPI_FFH_TYPE_SBI_HSM) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u FFixedHW type 0x%x is "
			"reserved (want 0x1 SBI HSM).",
			path, state, type);
		return false;
	}

	if (!method_brsi_sbi_suspend_state_is_valid(suspend_type)) {
		fwts_log_error(fw,
			"AML_050: SBI suspend type 0x%" PRIx32
			" is reserved or invalid.", suspend_type);
		return false;
	}

	return true;
}

static bool method_brsi_lpi_state_ok(
	fwts_framework *fw,
	const char *path,
	unsigned int state,
	const ACPI_OBJECT *pkg)
{
	char *name;
	uint64_t arch, minresidency, wakelatency, flag;

	if (pkg->Type != ACPI_TYPE_PACKAGE ||
	    pkg->Package.Count < LPI_ST_NAME) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u is not a usable Package "
			"(type %u, count=%u, need >= %u).",
			path, state, pkg->Type,
			pkg->Type == ACPI_TYPE_PACKAGE ? pkg->Package.Count : 0,
			LPI_ST_NAME);
		return false;
	}

	if (pkg->Package.Elements[LPI_ST_ARCH_FLAGS].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u Arch. Context Lost Flags "
			"is not an Integer (type %u).",
			path, state,
			pkg->Package.Elements[LPI_ST_ARCH_FLAGS].Type);
		return false;
	}
	if (pkg->Package.Elements[LPI_ST_MIN_RESIDENCY].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u MinResidency is not an "
			"Integer (type %u).",
			path, state,
			pkg->Package.Elements[LPI_ST_MIN_RESIDENCY].Type);
		return false;
	}
	if (pkg->Package.Elements[LPI_ST_WAKE_LATENCY].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u WakeLatency is not an "
			"Integer (type %u).",
			path, state,
			pkg->Package.Elements[LPI_ST_WAKE_LATENCY].Type);
		return false;
	}
	if (pkg->Package.Elements[LPI_ST_FLAGS].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u Flags is not an Integer "
			"(type %u).",
			path, state,
			pkg->Package.Elements[LPI_ST_FLAGS].Type);
		return false;
	}
	if (pkg->Package.Elements[LPI_ST_NAME].Type != ACPI_TYPE_STRING || pkg->Package.Elements[LPI_ST_NAME].String.Pointer == NULL) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u Name is not a valid String "
			"(type %u).",
			path, state,
			pkg->Package.Elements[LPI_ST_NAME].Type);
		return false;
	}

	arch = pkg->Package.Elements[LPI_ST_ARCH_FLAGS].Integer.Value;
	minresidency = pkg->Package.Elements[LPI_ST_MIN_RESIDENCY].Integer.Value;
	wakelatency = pkg->Package.Elements[LPI_ST_WAKE_LATENCY].Integer.Value;
	flag = pkg->Package.Elements[LPI_ST_FLAGS].Integer.Value;
	name = pkg->Package.Elements[LPI_ST_NAME].String.Pointer;

	fwts_log_info(fw,
		"AML_050: %s._LPI state %u Name=%s MinResidency=%" PRIu64" WakeLatency=%"
		PRIu64 " Flags=0x%" PRIx64 " ArchFlags=0x%" PRIx64 ".",
		path, state, name, minresidency, wakelatency, flag, arch);

	if (arch & LPI_ARCH_FLAGS_RESERVED_MASK) {
		fwts_log_error(fw,
			"AML_050: %s._LPI state %u Arch. Context Lost Flags 0x%" PRIx64
			" has reserved bits set (RISC-V FFH only allows bit 0).",
			path, state, arch);
		return false;
	}

	return method_brsi_lpi_entry_ok(fw, path, state,
			&pkg->Package.Elements[LPI_ST_ENTRY_METHOD]);
}

static void method_brsi_lpi_eval(
	method_brsi_lpi_ctx *ctx,
	ACPI_HANDLE handle,
	const char *path)
{
	fwts_framework *fw = ctx->fw;
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;
	ACPI_STATUS status;
	uint64_t rev, count,levelid;
	unsigned int i;

	status = AcpiEvaluateObject(handle, "_LPI", NULL, &buf);
	if (ACPI_FAILURE(status) || buf.Pointer == NULL) {
		fwts_log_warning(fw,
			"AML_050: %s._LPI is missing or failed to evaluate (%s).",
			path, AcpiFormatException(status));
		ctx->absent++;
		return;
	}

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE ||
	    obj->Package.Count < LPI_STATE_FIRST + 1) {
		fwts_log_error(fw,
			"AML_050: %s._LPI is not a usable Package; see the "
			"generic method _LPI test.",
			path);
		goto FAILED;
	}

	if (obj->Package.Elements[LPI_IDX_REVISION].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI Revision is not an Integer (type %u).",
			path, obj->Package.Elements[LPI_IDX_REVISION].Type);
		goto FAILED;
	}
	if (obj->Package.Elements[LPI_IDX_COUNT].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI Count is not an Integer (type %u).",
			path, obj->Package.Elements[LPI_IDX_REVISION].Type);
		goto FAILED;
	}

	if (obj->Package.Elements[LPI_IDX_LEVELID].Type != ACPI_TYPE_INTEGER) {
		fwts_log_error(fw,
			"AML_050: %s._LPI LevelID is not an Integer (type %u).",
			path, obj->Package.Elements[LPI_IDX_REVISION].Type);
		goto FAILED;
	}

	rev = obj->Package.Elements[LPI_IDX_REVISION].Integer.Value;
	count = obj->Package.Elements[LPI_IDX_COUNT].Integer.Value;
	levelid = obj->Package.Elements[LPI_IDX_LEVELID].Integer.Value;

	if (rev != 0) {
		fwts_log_error(fw,
			"AML_050: %s._LPI revision %" PRIu64
			" is not 0 (BRS requires revision 0).",
			path, rev);
		goto FAILED;
	}

	if (count != obj->Package.Count - LPI_STATE_FIRST) {
		fwts_log_error(fw,
			"AML_050: %s._LPI Count (%" PRIu64
			") does not match the number of state packages (%u).",
			path, count,
			obj->Package.Count - LPI_STATE_FIRST);
		goto FAILED;
	}

	fwts_log_info(fw,
		"AML_050: %s._LPI revision=%" PRIu64 " LevelID=0x%" PRIx64
		" Count=%" PRIu64 " (package count %u).",
		path, rev, levelid, count, obj->Package.Count);

	for (i = LPI_STATE_FIRST; i < obj->Package.Count; i++) {
		if (!method_brsi_lpi_state_ok(fw, path,
				i - LPI_STATE_FIRST + 1,
				&obj->Package.Elements[i]))
			goto FAILED;
	}
	ctx->with_lpi++;
	free(buf.Pointer);
	return;
FAILED:
	ctx->failed++;
	free(buf.Pointer);
	return;
}

static ACPI_STATUS method_brsi_lpi_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_lpi_ctx *ctx = context;
	char device_path[128];

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->harts++;
	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_050: found per-hart device %s.",
		device_path);

	method_brsi_lpi_eval(ctx, handle, device_path);

	return AE_OK;
}

static unsigned int method_brsi_count_cstate(fwts_framework *fw)
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
		if (strncmp(name + len - 4, "_CST", 4) != 0 &&
		    strncmp(name + len - 4, "_CSD", 4) != 0)
			continue;

		fwts_log_info(fw,
			"AML_050: found legacy C-state method %s.", name);
		n++;
	}

	return n;
}

static int method_brsi_aml050(fwts_framework *fw)
{
	method_brsi_lpi_ctx ctx;
	unsigned int cstate;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_CPU, method_brsi_lpi_walk, &ctx, NULL);
	AcpiWalkNamespace(ACPI_TYPE_PROCESSOR, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
		method_brsi_lpi_walk, NULL, &ctx, NULL);

	method_brsi_check_sb_osc(fw, OSC_SB_PCLPI_SUPPORT,
		ctx.with_lpi > 0 && !ctx.failed,
		"AML_050", "LPI");

	cstate = method_brsi_count_cstate(fw);

	if (ctx.harts == 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_050",
			"No per-hart "HID_CPU" or Processor() objects found. "
			"This should not happen on a valid RISC-V ACPI system.");
		return FWTS_OK;
	}

	if (ctx.failed) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_050",
			"%u of %u per-hart object(s) have a malformed or "
			"invalid _LPI implementation.",
			ctx.failed, ctx.harts);
		return FWTS_OK;
	}

	if (cstate) {
		if (ctx.absent)
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_050",
				"Found legacy C-state methods (OS-directed "
				"idle is supported), but %u of %u per-hart "
				"object(s) are missing _LPI.",
				ctx.absent, ctx.harts);
		else
			fwts_passed(fw,
				"AML_050: %u per-hart object(s) describe idle "
				"states via usable _LPI.",
				ctx.with_lpi);
	} else {
		if (no_os_idle_states)
			fwts_skipped(fw,
				"AML_050: --brs-i-no-os-idle-states specified; "
				"platform does not describe OS-directed "
				"hart idle states.");
		else if (ctx.with_lpi == 0)
			fwts_warning(fw,
				"AML_050: no usable _LPI and no legacy C-states "
				"found. If the platform does not support "
				"OS-directed idle states, re-run with "
				"--brs-i-no-os-idle-states.");
		else if (ctx.absent)
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_050",
				"%u of %u per-hart object(s) expose usable "
				"_LPI, but %u are missing it. "
				"Incomplete _LPI implementation.",
				ctx.with_lpi, ctx.harts, ctx.absent);
		else
			fwts_passed(fw,
				"AML_050: %u per-hart object(s) describe idle "
				"states via usable _LPI.",
				ctx.with_lpi);
	}

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	unsigned int failed;
} method_brsi_tad_ctx;

static ACPI_STATUS method_brsi_aml060_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_tad_ctx *ctx = context;
	char device_path[128];
	fwts_acpi_time_buffer real_time;
	ACPI_OBJECT arg0;
	uint64_t gcp = 0;
	uint64_t grt_len = 0;
	uint64_t srt = 0;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;

	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_060: found TAD %s (HID %s).",
		device_path, HID_TAD);

	if (!method_brsi_eval(handle, "_GCP", NULL, 0, &gcp)) {
		fwts_log_error(ctx->fw,
			"AML_060: %s._GCP is mandatory but missing, failed "
			"to evaluate, or did not return an Integer.",
			device_path);
			goto FAILED;
	} else {
		fwts_log_info(ctx->fw, "AML_060: %s._GCP returned 0x%" PRIx64 ".",
			device_path, gcp);
		if (gcp & ~0x1ff) {
			fwts_log_error(ctx->fw,
				"AML_060: %s._GCP reserved bits 9..31 are set.",
				device_path);
			goto FAILED;
		} else if (!(gcp & 0x4)) {
			fwts_log_error(ctx->fw,
				"AML_060: %s._GCP bit 2 (get/set real time) is not set.",
				device_path);
			goto FAILED;
		}
	}

	if (!method_brsi_eval(handle, "_GRT", NULL, 0, &grt_len)) {
		fwts_log_error(ctx->fw,
			"AML_060: %s._GRT is mandatory but missing, failed "
			"to evaluate, or did not return a Buffer.",
			device_path);
		goto FAILED;
	} else if (grt_len != sizeof(fwts_acpi_time_buffer)) {
		fwts_log_error(ctx->fw,
			"AML_060: %s._GRT returned a Buffer of %" PRIu64
			" bytes, expected %zu.",
			device_path, grt_len,
			sizeof(fwts_acpi_time_buffer));
		goto FAILED;
	} else {
		fwts_log_info(ctx->fw,
			"AML_060: %s._GRT returned a %" PRIu64 "-byte time buffer.",
			device_path, grt_len);
	}

	memset(&real_time, 0, sizeof(real_time));
	real_time.year = 2000;
	real_time.month = 1;
	real_time.day = 1;
	real_time.hour = 0;
	real_time.minute = 0;
	real_time.milliseconds = 1;
	real_time.timezone = 0;

	arg0.Type = ACPI_TYPE_BUFFER;
	arg0.Buffer.Length = sizeof(real_time);
	arg0.Buffer.Pointer = (void *)&real_time;

	if (!method_brsi_eval(handle, "_SRT", &arg0, 1, &srt)) {
		fwts_log_error(ctx->fw,
			"AML_060: %s._SRT is mandatory but missing, failed "
			"to evaluate, or did not return an Integer.",
			device_path);
		goto FAILED;
	} else {
		fwts_log_info(ctx->fw, "AML_060: %s._SRT returned 0x%" PRIx64 ".",
			device_path, srt);
		if (srt != 0) {
			fwts_log_error(ctx->fw,
				"AML_060: %s._SRT returned non-zero (0x%" PRIx64
				"), expected 0 (success).",
				device_path, srt);
			goto FAILED;
		}
	}
	return AE_OK;

FAILED:
	ctx->failed++;

	return AE_OK;
}

static int method_brsi_aml060(fwts_framework *fw)
{
	int fd;
	uint32_t runtimeservicessupported;
	uint32_t runtimeservicesexpect;
	method_brsi_tad_ctx ctx;

	if (fwts_lib_efi_runtime_module_init(fw, &fd) == FWTS_OK) {
		fwts_uefi_rt_support_status_get(fd, &runtimeservicessupported);
		runtimeservicesexpect = EFI_RT_SUPPORTED_GET_TIME | EFI_RT_SUPPORTED_SET_TIME;
		if ((runtimeservicessupported & runtimeservicesexpect) == runtimeservicesexpect) {
			fwts_skipped(fw,
				"AML_060: UEFI Runtime Services GetTime and "
				"SetTime are supported. The RTC is not on an "
				"OS-managed bus, so Time and Alarm Device is "
				"not required.");
			return FWTS_OK;
		}
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_TAD, method_brsi_aml060_walk, &ctx, NULL);

	if (ctx.failed) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_060",
			"%u of %u Time and Alarm Device(s) have a malformed "
			"or non-functional _GCP (bit 2), _GRT or _SRT.",
			ctx.failed, ctx.found);
		return FWTS_OK;
	}

	if (ctx.found == 0) {
		if (no_osbus_rtc)
			fwts_skipped(fw,
				"AML_060: --brs-i-no-osbus-rtc specified; no RTC "
				"on an OS-managed bus, Time and Alarm Device is "
				"not required.");
		else
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_060",
				"No Time and Alarm Device (HID %s) found. "
				"Systems with an RTC on an OS-managed bus MUST "
				"implement a TAD with functioning _GCP (bit 2), "
				"_GRT and _SRT. Re-run with --brs-i-no-osbus-rtc "
				"if this system has no such RTC.",
				HID_TAD);
	} else {
		fwts_passed(fw,
			"AML_060: %u Time and Alarm Device(s) implement "
			"functioning _GCP (bit 2 set), _GRT and _SRT "
			"(AML returned expected types and values; "
			"fwts simulates all address spaces).",
			ctx.found);
	}

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	char name[128];
	bool has_gsbus;
	bool has_reg;
	unsigned int regions;
} method_brsi_region_info;

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	fwts_list list;
} method_brsi_aml070_ctx;

static ACPI_STATUS method_brsi_region_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_region_info *info = context;
	ACPI_NAMESPACE_NODE *node;
	ACPI_OPERAND_OBJECT *obj;
	UINT8 space_id;
	const char *space;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	node = ACPI_CAST_PTR(ACPI_NAMESPACE_NODE, handle);
	if (node == NULL || node->Object == NULL)
		return AE_OK;

	obj = node->Object;
	space_id = obj->Region.SpaceId;
	space = AcpiUtGetRegionName(space_id);
	info->regions++;

	switch (space_id) {
	case ACPI_ADR_SPACE_GSBUS:
		fwts_log_warning(info->fw, "AML_070: %s parent has %s OperationRegion",
			info->name, space);
		info->has_gsbus = true;
		break;
	default:
		fwts_log_info(info->fw, "AML_070: %s parent has %s OperationRegion",
			info->name, space);
		break;
	}

	return AE_OK;
}

static ACPI_STATUS method_brsi_aml070_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_aml070_ctx *ctx = context;
	method_brsi_region_info *info;
	ACPI_HANDLE parent = NULL;
	ACPI_HANDLE reg_handle = NULL;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;

	info = calloc(1, sizeof(*info));
	if (info == NULL)
		return FWTS_OUT_OF_MEMORY;

	method_brsi_acpi_fullname(handle, info->name, sizeof(info->name));

	fwts_log_info(ctx->fw, "AML_070: found TAD %s (HID %s).",
		info->name, HID_TAD);

	info->fw = ctx->fw;
	if (ACPI_SUCCESS(AcpiGetParent(handle, &parent)) && parent) {
		if (ACPI_SUCCESS(AcpiGetHandle(parent, "_REG", &reg_handle) && reg_handle)) {
			info->has_reg = true;
			fwts_log_warning(ctx->fw, "AML_070: %s parent has _REG", info->name);
		}
	}


	AcpiWalkNamespace(ACPI_TYPE_REGION, parent ? parent : handle, ACPI_UINT32_MAX,
		method_brsi_region_walk, NULL, info, NULL);
	if (info->regions == 0)
		fwts_log_warning(ctx->fw, "AML_070: %s parent do not have any OperationRegion", info->name);

	fwts_list_append(&ctx->list, info);

	return AE_OK;
}

static int method_brsi_aml070(fwts_framework *fw)
{
	method_brsi_aml070_ctx ctx;
	fwts_list_link *item;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;
	fwts_list_init(&ctx.list);

	AcpiGetDevices(HID_TAD, method_brsi_aml070_walk, &ctx, NULL);

	if (ctx.found == 0) {
		fwts_skipped(fw,
			"AML_070: no Time and Alarm Device (HID %s) found; "
			"requirement applies only when a TAD is implemented.",
			HID_TAD);
		goto done;
	}

	fwts_list_foreach(item, &ctx.list) {
	method_brsi_region_info *info =
		fwts_list_data(method_brsi_region_info *, item);

		if (info->regions == 0) {
			fwts_warning(fw,
				"AML_070: %s parent has no OperationRegion. "
				"Cannot confirm compliance.",
				info->name);
		} else if (info->has_gsbus || info->has_reg) {
			fwts_warning(fw,
				"AML_070: %s may depend on an OS driver "
				"(parent has %s%s%s). Cannot confirm compliance.",
				info->name,
				info->has_gsbus ? "GenericSerialBus" : "",
				(info->has_gsbus && info->has_reg) ? " and " : "",
				info->has_reg ? "_REG" : "");
		} else {
			fwts_warning(fw,
				"AML_070: %s parent has no GenericSerialBus and no _REG. "
				"No strong signal of driver dependency, but compliance "
				"is not confirmed.",
				info->name);
		}
	}

	fwts_advice(fw,
		"AML_070 requires TADs to function without vendor-specific "
		"OS drivers. This check only inspects OperationRegions and "
		"_REG under the parent node. It cannot prove which regions "
		"_GRT/_SRT/_GCP actually use.");

done:
	fwts_list_free_items(&ctx.list, free);
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
		case 1:	/* --brs-i-no-os-idle-states */
			no_os_idle_states = true;
			break;
		case 2:	/* --brs-i-no-osbus-rtc */
			no_osbus_rtc = true;
			break;

		}
	}
	return FWTS_OK;
}

static fwts_option options[] = {
	{ "brs-i-no-os-perf-ctrl", "", 0,
	  "Platform has no OS-directed hart performance control (skip AML_040)" },
	{ "brs-i-no-os-idle-states", "", 0,
	  "Platform has no OS-directed hart idle states (skip AML_050)" },
	{ "brs-i-no-osbus-rtc", "", 0,
	  "Platform has no RTC on an OS-managed bus (skip AML_060)" },
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
	{ method_brsi_aml050,
	  "AML_050: processor idle states MUST be described using _LPI." },
	{ method_brsi_aml060,
	  "AML_060: TAD with _GCP bit 2, _GRT and _SRT if RTC is on an OS-managed bus." },
	{ method_brsi_aml070,
	  "AML_070: TAD MUST work in fwts ACPICA without kernel bus drivers." },
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
