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

#if defined(FWTS_HAS_ACPI) && (FWTS_ARCH_RISCV)
#include "fwts_acpi_object_eval.h"
#include <acmacros.h>
#include <aclocal.h>
#include <acobject.h>
#include <acstruct.h>
#include <acutils.h>

/* Standard AML IDs for an ECAM-capable PCI host bridge. */
#define CID_PCI			"PNP0A03"
#define HID_ECAM		"PNP0A08"
#define HID_CPU			"ACPI0007"
#define HID_CONTAINER		"ACPI0010"
/* ACPI Time and Alarm Device, same HID as src/acpi/devices/time/time.c. */
#define HID_TAD			"ACPI000E"
#define HID_PLIC		"RSCV0001"
#define HID_APLIC		"RSCV0002"
#define HID_UART		"RSCV0003"

static fwts_acpi_table_info *mtable;

/* Device Properties UUID: daffd814-6eba-4d8c-8a91-bc9bbf4aa301 */
static const uint8_t dsd_devprop_uuid[16] = {
	0x14, 0xd8, 0xff, 0xda, 0xba, 0x6e, 0x8c, 0x4d,
	0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01
};

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

static bool no_os_idle_states;
static bool no_os_perf_ctrl;
static bool no_osbus_rtc;

static int method_brsi_init(fwts_framework *fw)
{
	if (fwts_acpi_find_table(fw, "APIC", 0, &mtable) != FWTS_OK) {
		fwts_log_error(fw, "Cannot find ACPI MADT tables.");
		return FWTS_ERROR;
	}
	if (!mtable || mtable->length == 0) {
		fwts_log_error(fw, "Required ACPI MADT (APIC) table not found");
		return FWTS_ERROR;
	}

	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int method_brsi_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

typedef struct {
	fwts_framework *fw;
	char device_path[128];
	int io_num;
	bool walk_failed;
} method_brsi_rc;

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	fwts_list rc_list;
} method_brsi_rc_ctx;

static ACPI_STATUS method_brsi_crs_resource(ACPI_RESOURCE *resource, void *context)
{
	method_brsi_rc *rc = context;
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
		fwts_log_info(rc->fw,
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
	method_brsi_rc *rc;
	ACPI_STATUS status;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	rc = calloc(1, sizeof(*rc));
	if (rc == NULL)
		return AE_NO_MEMORY;
	rc->fw = ctx->fw;
	method_brsi_acpi_fullname(handle, rc->device_path, sizeof(rc->device_path));

	fwts_log_info(ctx->fw,
		"Checking _CRS of PCIe Root Complex %s (HID %s / CID %s).",
		rc->device_path, HID_ECAM, CID_PCI);

	status = AcpiWalkResources(handle, METHOD_NAME__CRS,
		method_brsi_crs_resource, rc);
	if (ACPI_FAILURE(status)) {
		rc->walk_failed = true;
		fwts_log_warning(ctx->fw,
			"Failed to walk _CRS of %s: %s.",
			rc->device_path, AcpiFormatException(status));
	}
	fwts_list_append(&ctx->rc_list, rc);
	ctx->found++;

	return AE_OK;
}

static int method_brsi_aml010(fwts_framework *fw)
{
	fwts_list_link *item;
	method_brsi_rc_ctx ctx;
	int walk_failed = 0;
	int has_io = 0;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;
	fwts_list_init(&ctx.rc_list);

	/*
	 * AcpiGetDevices() matches the given ID against both _HID and _CID,
	 * so PNP0A08 covers ECAM PCI Express host bridges.
	 */
	AcpiGetDevices(HID_ECAM, method_brsi_pci_host_walk, &ctx, NULL);

	fwts_list_foreach(item, &ctx.rc_list) {
		method_brsi_rc *rc = fwts_list_data(method_brsi_rc *, item);
		if (rc->walk_failed)
			walk_failed++;
		else if (rc->io_num)
			has_io++;
	}

	if (ctx.found == 0) {
		fwts_skipped(fw,
			"AML_010: no PCIe Root Complex with _HID/_CID %s found; "
			"skipping I/O range check.",
			HID_ECAM);
	} else if (has_io) {
		fwts_warning(fw,
			"AML_010: %u of %u PCIe Root Complex _CRS return I/O "
			"range descriptors. BRS-I says _CRS SHOULD NOT return "
			"I/O ranges (WordIO, DWordIO, QWordIO, IO, FixedIO or "
			"ExtendedIO).",
			has_io, ctx.found);
		fwts_advice(fw,
			"Legacy PCI I/O BARs are uncommon on modern PCIe devices "
			"and describing I/O space can complicate Root Complex "
			"configuration. Remove I/O descriptors from the Root "
			"Complex _CRS unless a specific device requires them.");
	} else if (walk_failed) {
		fwts_warning(fw,
			"AML_010: failed to inspect _CRS of %u of %u PCIe "
			"Root Complex device(s); not treated as "
			"\"does not return I/O range descriptors\".",
			walk_failed, ctx.found);
	} else {
		fwts_passed(fw,
			"AML_010: _CRS of %u PCIe Root Complex device(s) "
			"does not return I/O range descriptors.",
			ctx.found);
	}

	fwts_list_free_items(&ctx.rc_list, free);
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
			"unless a device truly requires runtime rebalancing.");
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
} method_brsi_cpu_ctx;

static ACPI_STATUS method_brsi_cpu_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_cpu_ctx *ctx = context;
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

	return AE_OK;
}

static int method_brsi_aml030(fwts_framework *fw)
{
	method_brsi_cpu_ctx ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_CPU, method_brsi_cpu_walk, &ctx, NULL);

	if (ctx.under_pr || ctx.elsewhere) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_030",
			"Per-hart %s objects must be under \\_SB, "
			"not deprecated \\_PR (%u under \\_PR, %u elsewhere, "
			"%u under \\_SB).",
			HID_CPU, ctx.under_pr, ctx.elsewhere, ctx.under_sb);
	} else if (ctx.under_sb) {
		fwts_passed(fw,
			"AML_030: %s per-hart objects are under \\_SB.",
			HID_CPU);
	} else {
		fwts_skipped(fw,
			"AML_030: no %s per-hart objects found.",
			HID_CPU);
	}

	return FWTS_OK;
}


typedef struct {
	fwts_framework *fw;
	unsigned int harts;
	unsigned int with_cpc;
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
			fwts_log_info(fw,
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
		fwts_log_info(fw,
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
		fwts_log_info(fw,
			"AML_040: %s.%s is not a Generic Register Descriptor.",
			path, field);
		return false;
	}
	if (is_null) {
		fwts_log_info(fw,
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
			fwts_log_info(fw,
				"AML_040: %s.%s FFixedHW descriptor must be "
				"64-bit, offset 0, AccessSize QWord.",
				path, field);
			return false;
		}
		if (mid != 0) {
			fwts_log_info(fw,
				"AML_040: %s.%s FFixedHW bits[59:32] must be 0.",
				path, field);
			return false;
		}
		if (type != CPPC_FFH_TYPE_SBI && type != CPPC_FFH_TYPE_CSR) {
			fwts_log_info(fw,
				"AML_040: %s.%s FFixedHW type 0x%x is reserved "
				"(want 0x1 SBI CPPC or 0x2 CSR).",
				path, field, type);
			return false;
		}
		if (type == CPPC_FFH_TYPE_CSR && ((addr >> 12) & 0xfffff) != 0) {
			fwts_log_info(fw,
				"AML_040: %s.%s CSR FFixedHW bits[31:12] "
				"must be 0.",
				path, field);
			return false;
		}
	}

	return true;
}

static bool method_brsi_cpc_check(
	fwts_framework *fw,
	ACPI_HANDLE handle,
	const char *path)
{
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;
	ACPI_STATUS status;
	uint64_t nent, rev;
	bool rc = true;

	status = AcpiEvaluateObject(handle, "_CPC", NULL, &buf);
	if (ACPI_FAILURE(status) || buf.Pointer == NULL) {
		fwts_log_info(fw,
			"AML_040: %s._CPC is missing or failed to evaluate "
			"(%s).",
			path, AcpiFormatException(status));
		return false;
	}

	obj = buf.Pointer;
	/*
	 * Package type, Revision/NumEntries types, and per-field Integer/Buffer
	 * types are already validated by src/acpi/method/method.c
	 * (method_test_CPC_return). Only BRS-I semantic checks remain here.
	 */
	if (obj->Type != ACPI_TYPE_PACKAGE || obj->Package.Count < 2) {
		fwts_log_info(fw,
			"AML_040: %s._CPC is not a usable Package (type %u, "
			"count=%u); see the generic method _CPC test.",
			path, obj->Type, obj->Type == ACPI_TYPE_PACKAGE ?
				obj->Package.Count : 0);
		free(buf.Pointer);
		return false;
	}

	nent = (obj->Package.Elements[CPC_IDX_NUM_ENTRIES].Type ==
		ACPI_TYPE_INTEGER) ?
		obj->Package.Elements[CPC_IDX_NUM_ENTRIES].Integer.Value : 0;
	rev = (obj->Package.Elements[CPC_IDX_REVISION].Type ==
		ACPI_TYPE_INTEGER) ?
		obj->Package.Elements[CPC_IDX_REVISION].Integer.Value : 0;

	fwts_log_info(fw,
		"AML_040: %s._CPC revision=%" PRIu64 " entries=%" PRIu64
		" (package count %u).",
		path, rev, nent, obj->Package.Count);

	/* BRS AML_040 requires CPPC; ACPI allows revision 1, BRS does not. */
	if (rev != CPPC_V2_REV && rev != CPPC_V3_REV && rev != CPPC_V4_REV) {
		fwts_log_info(fw,
			"AML_040: %s._CPC revision %" PRIu64
			" is not 2, 3 or 4.",
			path, rev);
		rc = false;
	}

	if (obj->Package.Count > CPC_IDX_LOWEST_PERF) {
		if (!method_brsi_cpc_perf_usable(fw, path, "HighestPerformance",
				&obj->Package.Elements[CPC_IDX_HIGHEST_PERF]))
			rc = false;
		if (!method_brsi_cpc_perf_usable(fw, path, "NominalPerformance",
				&obj->Package.Elements[CPC_IDX_NOMINAL_PERF]))
			rc = false;
		if (!method_brsi_cpc_perf_usable(fw, path,
				"LowestNonlinearPerformance",
				&obj->Package.Elements[CPC_IDX_LOW_NONLINEAR_PERF]))
			rc = false;
		if (!method_brsi_cpc_perf_usable(fw, path, "LowestPerformance",
				&obj->Package.Elements[CPC_IDX_LOWEST_PERF]))
			rc = false;
	} else {
		rc = false;
	}

	if (obj->Package.Count > CPC_IDX_DELIVERED_CTR) {
		if (!method_brsi_cpc_reg_usable(fw, path,
				"DesiredPerformanceRegister",
				&obj->Package.Elements[CPC_IDX_DESIRED_PERF]))
			rc = false;
		if (!method_brsi_cpc_reg_usable(fw, path,
				"ReferencePerformanceCounterRegister",
				&obj->Package.Elements[CPC_IDX_REF_CTR]))
			rc = false;
		if (!method_brsi_cpc_reg_usable(fw, path,
				"DeliveredPerformanceCounterRegister",
				&obj->Package.Elements[CPC_IDX_DELIVERED_CTR]))
			rc = false;
	} else {
		rc = false;
	}

	free(buf.Pointer);
	return rc;
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

	if (method_brsi_cpc_check(ctx->fw, handle, device_path))
		ctx->with_cpc++;
	else
		ctx->failed++;

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

static int method_brsi_aml040(fwts_framework *fw)
{
	method_brsi_cpc_ctx ctx;
	unsigned int pstate;

	/*
	 * AML_040: systems that support OS-directed hart performance
	 * control and power management MUST expose it via CPPC (_CPC).
	 * Whether the platform supports that cannot be discovered from
	 * ACPI, so --brs-i-no-os-perf-ctrl declares that CPPC is not
	 * required.
	 */
	if (no_os_perf_ctrl) {
		fwts_skipped(fw,
			"AML_040: --brs-i-no-os-perf-ctrl specified; "
			"platform does not support OS-directed hart "
			"performance control, CPPC is not required.");
		return FWTS_OK;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_CPU, method_brsi_cpc_walk, &ctx, NULL);

	pstate = method_brsi_count_pstate(fw);

	if (ctx.harts == 0) {
		if (pstate) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_040",
				"Found %u legacy P-state method(s) (_PCT/"
				"_PSS/_PPC) but no per-hart "HID_CPU" or "
				"Processor() objects with CPPC. BRS-I "
				"requires OS-directed performance control "
				"via CPPC, not P-states.",
				pstate);
		} else {
			fwts_skipped(fw,
				"AML_040: no "HID_CPU" or Processor() per-hart "
				"objects found; skipping CPPC check. Re-run "
				"with --brs-i-no-os-perf-ctrl if this platform "
				"does not support OS-directed hart performance "
				"control.");
		}
		return FWTS_OK;
	}

	if (ctx.failed || pstate) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_040",
			"%u of %u per-hart object(s) failed CPPC (_CPC)%s. "
			"Systems supporting OS-directed hart performance "
			"control MUST expose it via CPPC (ACPI 8.4.6). "
			"Re-run with --brs-i-no-os-perf-ctrl if this "
			"platform has no such support.",
			ctx.failed, ctx.harts,
			pstate ? "; legacy P-state methods are present" : "");
		if (pstate)
			fwts_advice(fw,
				"Replace _PCT/_PSS/_PPC P-state objects with "
				"a per-hart _CPC package. On RISC-V the "
				"register fields typically use FFixedHW "
				"descriptors that encode SBI CPPC or a CSR.");
	} else {
		fwts_passed(fw,
			"AML_040: %u per-hart object(s) expose CPPC via a "
			"usable _CPC package.",
			ctx.with_cpc);
	}

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	unsigned int harts;
	unsigned int containers;
	unsigned int with_lpi;
	unsigned int failed;
} method_brsi_lpi_ctx;

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

	/*
	 * ACPI allows Entry Method to be an Integer (OS-initiated
	 * composition) or a Generic Register Descriptor Buffer.
	 * RISC-V FFH describes WFI / SBI HSM via FFixedHW buffers.
	 */
	if (obj->Type == ACPI_TYPE_INTEGER) {
		fwts_log_info(fw,
			"AML_050: %s._LPI state %u Entry Method = "
			"Integer 0x%" PRIx64 ".",
			path, state, (uint64_t)obj->Integer.Value);
		return true;
	}

	if (!method_brsi_cpc_parse_reg(obj, &gas, &is_null) || is_null) {
		fwts_log_info(fw,
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
		return true;

	addr = gas.address;
	type = (addr >> 60) & 0xf;
	mid = (addr >> 32) & 0xfffffff;

	if (gas.register_bit_width != 64 ||
	    gas.register_bit_offset != 0 ||
	    gas.access_width != 4) {
		fwts_log_info(fw,
			"AML_050: %s._LPI state %u FFixedHW Entry Method "
			"must be 64-bit, offset 0, AccessSize QWord.",
			path, state);
		return false;
	}
	if (mid != 0) {
		fwts_log_info(fw,
			"AML_050: %s._LPI state %u FFixedHW bits[59:32] "
			"must be 0.",
			path, state);
		return false;
	}
	if (type != LPI_FFH_TYPE_WFI && type != LPI_FFH_TYPE_SBI_HSM) {
		fwts_log_info(fw,
			"AML_050: %s._LPI state %u FFixedHW type 0x%x is "
			"reserved (want 0x0 WFI or 0x1 SBI HSM).",
			path, state, type);
		return false;
	}
	if (type == LPI_FFH_TYPE_WFI && (addr & 0xffffffffULL) != 0) {
		fwts_log_info(fw,
			"AML_050: %s._LPI state %u WFI Entry Method "
			"bits[31:0] must be 0.",
			path, state);
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
	bool rc = true;

	/*
	 * State package type and element types (Integer/Buffer/String) are
	 * already checked by src/acpi/method/method.c (method_test_LPI_return).
	 * Only RISC-V BRS-I field semantics remain here.
	 */
	if (pkg->Type != ACPI_TYPE_PACKAGE ||
	    pkg->Package.Count < LPI_STATE_MIN_ENT)
		return false;

	if (pkg->Package.Count > LPI_ST_ARCH_FLAGS &&
	    pkg->Package.Elements[LPI_ST_ARCH_FLAGS].Type == ACPI_TYPE_INTEGER) {
		uint64_t arch =
			pkg->Package.Elements[LPI_ST_ARCH_FLAGS].Integer.Value;

		fwts_log_info(fw,
			"AML_050: %s._LPI state %u MinResidency=%" PRIu64
			" WakeLatency=%" PRIu64 " Flags=0x%" PRIx64
			" ArchFlags=0x%" PRIx64 ".",
			path, state,
			(uint64_t)pkg->Package.Elements[LPI_ST_MIN_RESIDENCY].Integer.Value,
			(uint64_t)pkg->Package.Elements[LPI_ST_WAKE_LATENCY].Integer.Value,
			(uint64_t)pkg->Package.Elements[LPI_ST_FLAGS].Integer.Value,
			arch);
		if (arch & LPI_ARCH_FLAGS_RESERVED_MASK) {
			fwts_log_info(fw,
				"AML_050: %s._LPI state %u Arch. Context Lost "
				"Flags 0x%" PRIx64 " has reserved bits set "
				"(RISC-V FFH only allows bit 0).",
				path, state, arch);
			rc = false;
		}
	}

	if (pkg->Package.Count > LPI_ST_ENTRY_METHOD &&
	    !method_brsi_lpi_entry_ok(fw, path, state,
			&pkg->Package.Elements[LPI_ST_ENTRY_METHOD]))
		rc = false;

	if (pkg->Package.Count > LPI_ST_NAME &&
	    pkg->Package.Elements[LPI_ST_NAME].Type == ACPI_TYPE_STRING &&
	    pkg->Package.Elements[LPI_ST_NAME].String.Pointer)
		fwts_log_info(fw,
			"AML_050: %s._LPI state %u Name=\"%s\".",
			path, state,
			pkg->Package.Elements[LPI_ST_NAME].String.Pointer);

	return rc;
}

typedef enum {
	METHOD_BRSI_LPI_ABSENT = 0,
	METHOD_BRSI_LPI_BAD,
	METHOD_BRSI_LPI_OK
} method_brsi_lpi_rc;

static method_brsi_lpi_rc method_brsi_lpi_eval(
	fwts_framework *fw,
	ACPI_HANDLE handle,
	const char *path)
{
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;
	ACPI_STATUS status;
	uint64_t rev, count;
	unsigned int i;
	bool rc = true;

	status = AcpiEvaluateObject(handle, "_LPI", NULL, &buf);
	if (ACPI_FAILURE(status) || buf.Pointer == NULL)
		return METHOD_BRSI_LPI_ABSENT;

	obj = buf.Pointer;
	/*
	 * Package type, Revision/LevelID/Count types, revision==0, Count vs
	 * number of state packages, and per-state element types are already
	 * validated by src/acpi/method/method.c (method_test_LPI_return).
	 */
	if (obj->Type != ACPI_TYPE_PACKAGE ||
	    obj->Package.Count < LPI_STATE_FIRST + 1) {
		fwts_log_info(fw,
			"AML_050: %s._LPI is not a usable Package; see the "
			"generic method _LPI test.",
			path);
		free(buf.Pointer);
		return METHOD_BRSI_LPI_BAD;
	}

	rev = (obj->Package.Elements[LPI_IDX_REVISION].Type ==
		ACPI_TYPE_INTEGER) ?
		obj->Package.Elements[LPI_IDX_REVISION].Integer.Value : 0;
	count = (obj->Package.Elements[LPI_IDX_COUNT].Type ==
		ACPI_TYPE_INTEGER) ?
		obj->Package.Elements[LPI_IDX_COUNT].Integer.Value : 0;

	fwts_log_info(fw,
		"AML_050: %s._LPI revision=%" PRIu64 " LevelID=0x%" PRIx64
		" Count=%" PRIu64 " (package count %u).",
		path, rev,
		obj->Package.Elements[LPI_IDX_LEVELID].Type == ACPI_TYPE_INTEGER ?
			(uint64_t)obj->Package.Elements[LPI_IDX_LEVELID].Integer.Value : 0,
		count, obj->Package.Count);

	for (i = LPI_STATE_FIRST; i < obj->Package.Count; i++) {
		if (!method_brsi_lpi_state_ok(fw, path,
				i - LPI_STATE_FIRST + 1,
				&obj->Package.Elements[i]))
			rc = false;
	}

	free(buf.Pointer);
	return rc ? METHOD_BRSI_LPI_OK : METHOD_BRSI_LPI_BAD;
}

static bool method_brsi_hid_is(ACPI_HANDLE handle, const char *hid)
{
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;
	char id[16];
	bool match = false;

	if (ACPI_FAILURE(AcpiEvaluateObject(handle, "_HID", NULL, &buf)) ||
	    buf.Pointer == NULL)
		return false;

	obj = buf.Pointer;
	id[0] = '\0';

	if (obj->Type == ACPI_TYPE_STRING && obj->String.Pointer != NULL) {
		snprintf(id, sizeof(id), "%s", obj->String.Pointer);
		match = strcmp(id, hid) == 0;
	} else if (obj->Type == ACPI_TYPE_INTEGER &&
		   fwts_method_valid_EISA_ID((uint32_t)obj->Integer.Value,
					     id, sizeof(id))) {
		match = strcmp(id, hid) == 0;
	}

	free(buf.Pointer);
	return match;
}

/*
 * Idle states may be declared on the hart (ACPI0007) or on an ancestor
 * Processor Container (ACPI0010). Walk parents until a usable _LPI is
 * found or the root is reached.
 */
static bool method_brsi_lpi_on_hierarchy(
	fwts_framework *fw,
	ACPI_HANDLE handle,
	const char *path)
{
	ACPI_HANDLE cur = handle;
	char cur_path[128];
	unsigned int depth = 0;

	strncpy(cur_path, path, sizeof(cur_path) - 1);
	cur_path[sizeof(cur_path) - 1] = '\0';

	while (cur && depth < 16) {
		ACPI_HANDLE parent;

		switch (method_brsi_lpi_eval(fw, cur, cur_path)) {
		case METHOD_BRSI_LPI_OK:
			if (cur != handle)
				fwts_log_info(fw,
					"AML_050: %s inherits _LPI from "
					"ancestor %s.",
					path, cur_path);
			return true;
		case METHOD_BRSI_LPI_BAD:
			return false;
		case METHOD_BRSI_LPI_ABSENT:
		default:
			break;
		}

		if (ACPI_FAILURE(AcpiGetParent(cur, &parent)) ||
		    parent == NULL || parent == cur)
			break;

		if (!method_brsi_hid_is(parent, HID_CONTAINER))
			break;

		cur = parent;
		method_brsi_acpi_fullname(cur, cur_path, sizeof(cur_path));
		depth++;
	}

	fwts_log_info(fw,
		"AML_050: %s has no usable _LPI on the device or an "
		HID_CONTAINER" ancestor.",
		path);
	return false;
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

	if (method_brsi_lpi_on_hierarchy(ctx->fw, handle, device_path))
		ctx->with_lpi++;
	else
		ctx->failed++;

	return AE_OK;
}

static ACPI_STATUS method_brsi_lpi_container_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_lpi_ctx *ctx = context;
	char device_path[128];

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->containers++;
	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw,
		"AML_050: found Processor Container %s.", device_path);

	/*
	 * Containers are optional LPI nodes. Validate _LPI when present;
	 * absence is not a failure (the leaf hart may carry the states).
	 */
	if (method_brsi_lpi_eval(ctx->fw, handle, device_path) ==
	    METHOD_BRSI_LPI_OK)
		fwts_log_info(ctx->fw,
			"AML_050: %s exposes a usable container _LPI.",
			device_path);

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

	/*
	 * AML_050: processor idle states MUST be described using _LPI
	 * (ACPI 8.4.3 / 8.4.4), not legacy _CST/_CSD C-states.
	 * --brs-i-no-os-idle-states declares that the platform exposes
	 * no OS-directed hart idle states.
	 */
	if (no_os_idle_states) {
		fwts_skipped(fw,
			"AML_050: --brs-i-no-os-idle-states specified; "
			"platform does not describe OS-directed hart idle "
			"states, _LPI is not required.");
		return FWTS_OK;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_CPU, method_brsi_lpi_walk, &ctx, NULL);
	AcpiGetDevices(HID_CONTAINER,
		method_brsi_lpi_container_walk, &ctx, NULL);

	cstate = method_brsi_count_cstate(fw);

	if (ctx.harts == 0) {
		if (cstate) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_050",
				"Found %u legacy C-state method(s) (_CST/"
				"_CSD) but no per-hart "HID_CPU" objects "
				"with _LPI. BRS-I requires processor idle "
				"states via Low Power Idle (_LPI), not "
				"C-states.",
				cstate);
		} else {
			fwts_skipped(fw,
				"AML_050: no "HID_CPU" per-hart objects "
				"found; skipping _LPI check. Re-run with "
				"--brs-i-no-os-idle-states if this platform "
				"does not describe processor idle states.");
		}
		return FWTS_OK;
	}

	if (ctx.failed || cstate) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_050",
			"%u of %u per-hart object(s) failed Low Power Idle "
			"(_LPI)%s. Processor idle states MUST be described "
			"using _LPI (ACPI 8.4.3). Re-run with --brs-i-no-os-idle-states "
			"if this platform has no OS-directed idle states.",
			ctx.failed, ctx.harts,
			cstate ? "; legacy C-state methods are present" : "");
		if (cstate)
			fwts_advice(fw,
				"Replace _CST/_CSD C-state objects with "
				"per-hart or Processor Container _LPI "
				"packages. On RISC-V the Entry Method "
				"typically uses an FFixedHW descriptor "
				"encoding WFI (type 0) or SBI HSM suspend "
				"(type 1).");
	} else {
		fwts_passed(fw,
			"AML_050: %u per-hart object(s) describe idle "
			"states via _LPI (%u Processor Container(s) "
			"examined).",
			ctx.with_lpi, ctx.containers);
	}

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	unsigned int failed;
} method_brsi_tad_ctx;

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
	} else if (obj->Type == ACPI_TYPE_STRING && !strcmp(name, "_UID")) {
		char *end = NULL;
		unsigned long long v;

		v = strtoull(obj->String.Pointer, &end, 0);
		if (end != obj->String.Pointer && end != NULL && *end == '\0') {
			rc = true;
			if (value)
				*value = v;
		}
	}

	free(buf.Pointer);
	return rc;
}

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
	bool failed = false;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;

	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_060: found TAD %s (HID %s).",
		device_path, HID_TAD);

	if (!method_brsi_eval(handle, "_GCP", NULL, 0, &gcp)) {
		fwts_log_info(ctx->fw,
			"AML_060: %s._GCP is mandatory but missing, failed "
			"to evaluate, or did not return an Integer.",
			device_path);
		failed = true;
	} else {
		fwts_log_info(ctx->fw, "AML_060: %s._GCP returned 0x%" PRIx64 ".",
			device_path, gcp);
		if (gcp & ~0x1ff) {
			fwts_log_info(ctx->fw,
				"AML_060: %s._GCP reserved bits 9..31 are set.",
				device_path);
			failed = true;
		} else if (!(gcp & 0x4)) {
			fwts_log_info(ctx->fw,
				"AML_060: %s._GCP bit 2 (get/set real time) is not set.",
				device_path);
			failed = true;
		}
	}

	if (!method_brsi_eval(handle, "_GRT", NULL, 0, &grt_len)) {
		fwts_log_info(ctx->fw,
			"AML_060: %s._GRT is mandatory but missing, failed "
			"to evaluate, or did not return a Buffer.",
			device_path);
		failed = true;
	} else if (grt_len != sizeof(fwts_acpi_time_buffer)) {
		fwts_log_info(ctx->fw,
			"AML_060: %s._GRT returned a Buffer of %" PRIu64
			" bytes, expected %zu.",
			device_path, grt_len,
			sizeof(fwts_acpi_time_buffer));
		failed = true;
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
		fwts_log_info(ctx->fw,
			"AML_060: %s._SRT is mandatory but missing, failed "
			"to evaluate, or did not return an Integer.",
			device_path);
		failed = true;
	} else {
		fwts_log_info(ctx->fw, "AML_060: %s._SRT returned 0x%" PRIx64 ".",
			device_path, srt);
	}

	if (failed)
		ctx->failed++;

	return AE_OK;
}

static int method_brsi_aml060(fwts_framework *fw)
{
	method_brsi_tad_ctx ctx;

	/*
	 * AML_060 applies only when the platform has an RTC on a bus the
	 * OS manages (I2C, SPI, ...). That cannot be discovered from ACPI,
	 * so --brs-i-no-osbus-rtc declares that TAD is not required.
	 */
	if (no_osbus_rtc) {
		fwts_skipped(fw,
			"AML_060: --brs-i-no-osbus-rtc specified; no RTC on "
			"an OS-managed bus, Time and Alarm Device is not "
			"required.");
		return FWTS_OK;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_TAD, method_brsi_aml060_walk, &ctx, NULL);

	if (ctx.found == 0)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_060",
			"No Time and Alarm Device (HID %s) found. Systems "
			"with an RTC on an OS-managed bus MUST implement a "
			"TAD with functioning _GCP (bit 2 set), _GRT and "
			"_SRT. Re-run with --brs-i-no-osbus-rtc if this "
			"system has no such RTC.",
			HID_TAD);
	else if (ctx.failed)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_060",
			"%u of %u Time and Alarm Device(s) failed _GCP bit 2, "
			"_GRT or _SRT.",
			ctx.failed, ctx.found);
	else
		fwts_passed(fw,
			"AML_060: %u Time and Alarm Device(s) implement "
			"functioning _GCP (bit 2 set), _GRT and _SRT.",
			ctx.found);

	return FWTS_OK;
}



typedef struct {
	fwts_framework *fw;
	bool has_gsbus;
	bool has_sysmem;
	bool has_sysio;
	bool has_other;
	unsigned int regions;
} method_brsi_region_info;

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	unsigned int used_gsbus;
	unsigned int no_fallback;
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
	char name[128];
	const char *space;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	node = ACPI_CAST_PTR(ACPI_NAMESPACE_NODE, handle);
	if (node == NULL || node->Object == NULL)
		return AE_OK;

	obj = node->Object;
	if (obj->Common.Type != ACPI_TYPE_REGION)
		return AE_OK;

	space_id = obj->Region.SpaceId;
	info->regions++;

	switch (space_id) {
	case ACPI_ADR_SPACE_GSBUS:
		info->has_gsbus = true;
		space = "GenericSerialBus";
		break;
	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
		info->has_sysmem = true;
		space = "SystemMemory";
		break;
	case ACPI_ADR_SPACE_SYSTEM_IO:
		info->has_sysio = true;
		space = "SystemIO";
		break;
	default:
		info->has_other = true;
		space = AcpiUtGetRegionName(space_id);
		break;
	}

	method_brsi_acpi_fullname(handle, name, sizeof(name));
	fwts_log_info(info->fw, "AML_070: %s OperationRegion SpaceId=%s.",
		name[0] ? name : "(unknown)", space);

	return AE_OK;
}

static ACPI_STATUS method_brsi_aml070_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_aml070_ctx *ctx = context;
	method_brsi_region_info info;
	char device_path[128];

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;
	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));
	fwts_log_info(ctx->fw, "AML_070: found TAD %s (HID %s).",
		device_path, HID_TAD);

	memset(&info, 0, sizeof(info));
	info.fw = ctx->fw;
	AcpiWalkNamespace(ACPI_TYPE_REGION, handle, ACPI_UINT32_MAX,
		method_brsi_region_walk, NULL, &info, NULL);

	if (info.regions == 0) {
		fwts_log_info(ctx->fw,
			"AML_070: %s has no OperationRegion under the device.",
			device_path);
		return AE_OK;
	}

	fwts_log_info(ctx->fw,
		"AML_070: %s has %u OperationRegion(s) "
		"(GenericSerialBus=%s SystemMemory=%s SystemIO=%s).",
		device_path, info.regions,
		info.has_gsbus ? "yes" : "no",
		info.has_sysmem ? "yes" : "no",
		info.has_sysio ? "yes" : "no");

	if (info.has_gsbus) {
		ctx->used_gsbus++;
		if (!info.has_sysmem && !info.has_sysio)
			ctx->no_fallback++;
	}

	return AE_OK;
}

static int method_brsi_aml070(fwts_framework *fw)
{
	method_brsi_aml070_ctx ctx;

	/*
	 * AML_070: TAD must work with no vendor OS driver.
	 * fwts ACPICA installs simulated address-space handlers, including
	 * GenericSerialBus, so this test only inspects OperationRegion
	 * SpaceIds and does not treat simulated accesses as compliance.
	 */
	fwts_log_info(fw,
		"AML_070: inspect TAD OperationRegion SpaceIds. "
		"fwts simulates SystemMemory, SystemIO and GenericSerialBus.");

	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_TAD, method_brsi_aml070_walk, &ctx, NULL);

	if (ctx.found == 0) {
		fwts_skipped(fw,
			"AML_070: no Time and Alarm Device (HID %s) found; "
			"requirement applies only when a TAD is implemented.",
			HID_TAD);
		return FWTS_OK;
	}

	if (ctx.no_fallback) {
		fwts_warning(fw,
			"AML_070: %u of %u TAD(s) use GenericSerialBus "
			"with no SystemMemory/SystemIO OperationRegion. "
			"That path normally needs a bus driver; fwts "
			"cannot prove a no-driver fallback exists.",
			ctx.no_fallback, ctx.found);
		fwts_advice(fw,
			"AML_070 requires the TAD to work without a vendor "
			"OS driver. A SystemMemory or SystemIO fallback "
			"region is the usual no-driver path. GenericSerialBus "
			"alone is not sufficient evidence of compliance, and "
			"fwts simulated GSBUS handlers must not be treated "
			"as a pass.");
	} else if (ctx.used_gsbus) {
		fwts_skipped(fw,
			"AML_070: %u of %u TAD(s) declare GenericSerialBus "
			"and also SystemMemory/SystemIO. fwts simulates "
			"both, so this does not establish which path AML "
			"takes without a bus driver.",
			ctx.used_gsbus, ctx.found);
	} else {
		fwts_skipped(fw,
			"AML_070: %u TAD(s) have no GenericSerialBus "
			"OperationRegion. Region accesses are still "
			"simulated by fwts, so this is not treated as "
			"AML_070 compliance.",
			ctx.found);
	}

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	const char *hid;
	const char *kind;
	unsigned int found;
	unsigned int failed;
} method_brsi_gsb_ctx;

static ACPI_STATUS method_brsi_gsb_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_gsb_ctx *ctx = context;
	char device_path[128];
	uint64_t gsb = 0;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;

	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_080: found %s %s (HID %s).",
		ctx->kind, device_path, ctx->hid);

	if (!method_brsi_eval(handle, "_GSB", NULL, 0, &gsb)) {
		fwts_log_info(ctx->fw,
			"AML_080: %s._GSB is mandatory but missing, failed "
			"to evaluate, or did not return an Integer.",
			device_path);
		ctx->failed++;
	} else {
		fwts_log_info(ctx->fw,
			"AML_080: %s._GSB returned GSI base 0x%" PRIx64 ".",
			device_path, gsb);
	}

	return AE_OK;
}

static int method_brsi_aml080(fwts_framework *fw)
{
	method_brsi_gsb_ctx ctx;

	/*
	 * AML_080: every PLIC (RSCV0001) and APLIC (RSCV0002) namespace
	 * device must implement _GSB returning the GSI base as an Integer.
	 * Device presence when MADT has matching entries is AML_100.
	 */
	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	ctx.hid = HID_PLIC;
	ctx.kind = "PLIC";
	AcpiGetDevices(HID_PLIC, method_brsi_gsb_walk, &ctx, NULL);

	ctx.hid = HID_APLIC;
	ctx.kind = "APLIC";
	AcpiGetDevices(HID_APLIC, method_brsi_gsb_walk, &ctx, NULL);

	if (ctx.found == 0) {
		fwts_skipped(fw,
			"AML_080: no PLIC (HID %s) or APLIC (HID %s) device "
			"found; _GSB is required on those objects when they "
			"exist.",
			HID_PLIC, HID_APLIC);
		return FWTS_OK;
	}

	if (ctx.failed)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_080",
			"%u of %u PLIC/APLIC device(s) failed _GSB.",
			ctx.failed, ctx.found);
	else
		fwts_passed(fw,
			"AML_080: %u PLIC/APLIC device(s) implement _GSB "
			"returning an Integer GSI base.",
			ctx.found);

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	unsigned int found;
	unsigned int failed;
	bool clock_ok;
} method_brsi_uart_ctx;

static bool method_brsi_dsd_uuid_match(const ACPI_OBJECT *obj)
{
	if (obj == NULL || obj->Type != ACPI_TYPE_BUFFER)
		return false;
	if (obj->Buffer.Length != sizeof(dsd_devprop_uuid))
		return false;
	return memcmp(obj->Buffer.Pointer, dsd_devprop_uuid,
		sizeof(dsd_devprop_uuid)) == 0;
}

static void method_brsi_uart_dsd_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	method_brsi_uart_ctx *ctx = private;
	uint32_t i, j;

	FWTS_UNUSED(buf);

	if (obj == NULL || obj->Type != ACPI_TYPE_PACKAGE) {
		fwts_log_info(fw, "AML_090: %s did not return a Package.", name);
		return;
	}

	if (obj->Package.Count & 1) {
		fwts_log_info(fw,
			"AML_090: %s must contain UUID/data pairs "
			"(even element count), got %" PRIu32 ".",
			name, obj->Package.Count);
		return;
	}

	for (i = 0; i < obj->Package.Count; i += 2) {
		ACPI_OBJECT *uuid = &obj->Package.Elements[i];
		ACPI_OBJECT *data = &obj->Package.Elements[i + 1];

		if (!method_brsi_dsd_uuid_match(uuid))
			continue;
		if (data->Type != ACPI_TYPE_PACKAGE) {
			fwts_log_info(fw,
				"AML_090: %s Device Properties data is not "
				"a Package.",
				name);
			return;
		}

		for (j = 0; j < data->Package.Count; j++) {
			ACPI_OBJECT *prop = &data->Package.Elements[j];
			ACPI_OBJECT *key, *val;
			const char *keystr;
			uint64_t valint;

			if (prop->Type != ACPI_TYPE_PACKAGE ||
			    prop->Package.Count < 2) {
				fwts_log_info(fw,
					"AML_090: %s property %" PRIu32
					" is not a 2-element Package.",
					name, j);
				continue;
			}

			key = &prop->Package.Elements[0];
			val = &prop->Package.Elements[1];

			if (key->Type != ACPI_TYPE_STRING) {
				fwts_log_info(fw,
					"AML_090: %s property %" PRIu32
					" name is not a String.",
					name, j);
				continue;
			}
			keystr = key->String.Pointer;

			if (val->Type != ACPI_TYPE_INTEGER) {
				fwts_log_info(fw,
					"AML_090: %s property \"%s\" is not "
					"an Integer.",
					name, keystr);
				continue;
			}
			valint = val->Integer.Value;

			fwts_log_info(fw,
				"AML_090: %s \"%s\" = %" PRIu64 ".",
				name, keystr, valint);

			if (strcmp(keystr, "clock-frequency") == 0) {
				if (valint != 0)
					ctx->clock_ok = true;
				else
					fwts_log_info(fw,
						"AML_090: %s clock-frequency "
						"is 0; baud rate cannot be set.",
						name);
			}

			if (strcmp(keystr, "reg-io-width") == 0) {
				if (valint != 1 && valint != 2 &&
						valint != 4 && valint != 8)
					fwts_log_info(fw,
						"AML_090: %s reg-io-width "
						"must be 1, 2, 4 or 8.",
						name);
			}
		}
		return;
	}

	fwts_log_info(fw,
		"AML_090: %s has no Device Properties UUID "
		"(daffd814-6eba-4d8c-8a91-bc9bbf4aa301).",
		name);
}

static ACPI_STATUS method_brsi_uart_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_uart_ctx *ctx = context;
	char device_path[128];

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;
	ctx->clock_ok = false;

	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_090: found UART %s (HID %s).",
		device_path, HID_UART);

	if (fwts_evaluate_method(ctx->fw, METHOD_MANDATORY | METHOD_SILENT,
			&handle, "_DSD", NULL, 0,
			method_brsi_uart_dsd_return, ctx) != FWTS_OK)
		fwts_log_info(ctx->fw,
			"AML_090: %s._DSD is mandatory but missing or failed "
			"to evaluate.",
			device_path);

	if (!ctx->clock_ok)
		ctx->failed++;

	return AE_OK;
}

static int method_brsi_aml090(fwts_framework *fw)
{
	method_brsi_uart_ctx ctx;

	/*
	 * AML_090: RSCV0003 UART devices must implement the UART device
	 * properties (BRS acpi-prop.adoc) via _DSD Device Properties UUID.
	 * clock-frequency is required and must be a non-zero Integer.
	 */
	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	AcpiGetDevices(HID_UART, method_brsi_uart_walk, &ctx, NULL);

	if (ctx.found == 0) {
		fwts_skipped(fw,
			"AML_090: no UART device (HID %s) found; UART "
			"properties are required on those objects when they "
			"exist.",
			HID_UART);
		return FWTS_OK;
	}

	if (ctx.failed)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_090",
			"%u of %u UART device(s) (HID %s) missing a non-zero "
			"clock-frequency in _DSD Device Properties.",
			ctx.failed, ctx.found, HID_UART);
	else
		fwts_passed(fw,
			"AML_090: %u UART device(s) (HID %s) implement "
			"clock-frequency in _DSD Device Properties.",
			ctx.found, HID_UART);

	return FWTS_OK;
}

typedef enum {
	METHOD_BRSI_INTC_PLIC,
	METHOD_BRSI_INTC_APLIC,
	METHOD_BRSI_INTC_MAX,
} method_brsi_intc_type;

typedef struct {
	method_brsi_intc_type	type;
	uint8_t			id;
	uint32_t		gsi_base;
	bool			matched;
} method_brsi_intc;

typedef struct {
	fwts_framework		*fw;
	fwts_list		intc_list;
	method_brsi_intc_type	type;
	unsigned int		found;
	unsigned int		failed;
} method_brsi_intc_ctx;

static char *method_brsi_intc_kind(method_brsi_intc_type type)
{
	char* kinds[] = {"PLIC", "APLIC"};
	return type < METHOD_BRSI_INTC_MAX ? kinds[type] : "(unknown)";
}

static char *method_brsi_intc_hid(method_brsi_intc_type type)
{
	char* hids[] = {HID_PLIC, HID_APLIC};
	return type < METHOD_BRSI_INTC_MAX ? hids[type] : "(unknown)";
}

static void method_brsi_intc_add(
	fwts_list *list,
	method_brsi_intc_type type,
	uint8_t id,
	uint32_t gsi_base)
{
	method_brsi_intc *ic;

	ic = calloc(1, sizeof(*ic));
	if (ic == NULL)
		return;

	ic->type = type;
	ic->id = id;
	ic->gsi_base = gsi_base;
	fwts_list_append(list, ic);
}

static int method_brsi_collect_madt_intc(
	fwts_framework *fw,
	method_brsi_intc_ctx *ctx)
{
	uint8_t *data;
	ssize_t length;

	fwts_list_init(&ctx->intc_list);

	data = mtable->data;
	length = mtable->length;
	if (length < (ssize_t)sizeof(fwts_acpi_table_madt)) {
		fwts_log_error(fw, "AML_100: MADT is truncated.");
		return FWTS_ERROR;
	}

	data += sizeof(fwts_acpi_table_madt);
	length -= sizeof(fwts_acpi_table_madt);

	while (length > (ssize_t)sizeof(fwts_acpi_madt_sub_table_header)) {
		fwts_acpi_madt_sub_table_header *hdr =
			(fwts_acpi_madt_sub_table_header *)data;
		ssize_t body;

		if (hdr->length < sizeof(*hdr) || hdr->length > length)
			break;

		data += sizeof(*hdr);
		length -= sizeof(*hdr);
		body = hdr->length - sizeof(*hdr);

		if (hdr->type == FWTS_MADT_PLIC &&
		    body >= (ssize_t)sizeof(fwts_acpi_madt_plic)) {
			fwts_acpi_madt_plic *plic = (fwts_acpi_madt_plic *)data;

			method_brsi_intc_add(&ctx->intc_list,
				METHOD_BRSI_INTC_PLIC, plic->id, plic->gsi_base);
			fwts_log_info(fw,
				"AML_100: MADT PLIC id=%u gsi_base=0x%" PRIx32 ".",
				plic->id, plic->gsi_base);
		} else if (hdr->type == FWTS_MADT_APLIC &&
			   body >= (ssize_t)sizeof(fwts_acpi_madt_aplic)) {
			fwts_acpi_madt_aplic *aplic =
				(fwts_acpi_madt_aplic *)data;

			method_brsi_intc_add(&ctx->intc_list,
				METHOD_BRSI_INTC_APLIC, aplic->id,
				aplic->gsi_base);
			fwts_log_info(fw,
				"AML_100: MADT APLIC id=%u gsi_base=0x%" PRIx32 ".",
				aplic->id, aplic->gsi_base);
		}

		data += body;
		length -= body;
	}

	return FWTS_OK;
}

static ACPI_STATUS method_brsi_intc_walk(
	ACPI_HANDLE handle,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	method_brsi_intc_ctx *ctx = context;
	method_brsi_intc *hit = NULL;
	fwts_list_link *item;
	char device_path[128];
	const char *kind = method_brsi_intc_kind(ctx->type);
	const char *hid = method_brsi_intc_hid(ctx->type);
	bool have_uid;
	bool have_gsb;
	uint64_t uid = 0;
	uint64_t gsb = 0;

	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	ctx->found++;

	method_brsi_acpi_fullname(handle, device_path, sizeof(device_path));

	fwts_log_info(ctx->fw, "AML_100: found %s %s (HID %s).",
		kind, device_path, hid);

	have_uid = method_brsi_eval(handle, "_UID", NULL, 0, &uid);
	have_gsb = method_brsi_eval(handle, "_GSB", NULL, 0, &gsb);

	if (have_uid)
		fwts_log_info(ctx->fw, "AML_100: %s._UID = 0x%" PRIx64 ".",
			device_path, uid);
	else
		fwts_log_info(ctx->fw,
			"AML_100: %s._UID missing or not Integer/numeric String.",
			device_path);

	if (have_gsb)
		fwts_log_info(ctx->fw, "AML_100: %s._GSB = 0x%" PRIx64 ".",
			device_path, gsb);
	else
		fwts_log_info(ctx->fw,
			"AML_100: %s._GSB missing or not Integer.",
			device_path);

	if (!have_uid && !have_gsb) {
		fwts_log_info(ctx->fw,
			"AML_100: %s has neither _UID nor _GSB; cannot match "
			"a MADT %s entry.",
			device_path, kind);
		ctx->failed++;
		return AE_OK;
	}

	fwts_list_foreach(item, &ctx->intc_list) {
		method_brsi_intc *ic =
			fwts_list_data(method_brsi_intc *, item);
		bool uid_ok;
		bool gsb_ok;

		if (ic->type != ctx->type)
			continue;

		uid_ok = !have_uid || ic->id == uid;
		gsb_ok = !have_gsb || ic->gsi_base == gsb;

		if (uid_ok && gsb_ok) {
			hit = ic;
			break;
		}
	}

	if (hit == NULL) {
		fwts_log_info(ctx->fw,
			"AML_100: %s has no matching MADT %s entry "
			"(_UID%s0x%" PRIx64 ", _GSB%s0x%" PRIx64 ").",
			device_path, kind,
			have_uid ? "=" : " n/a ", have_uid ? uid : 0,
			have_gsb ? "=" : " n/a ", have_gsb ? gsb : 0);
		ctx->failed++;
		return AE_OK;
	}

	if (hit->matched)
		fwts_log_warning(ctx->fw,
			"AML_100: MADT %s id=%u gsi_base=0x%" PRIx32
			" matches more than one namespace device.",
			kind, hit->id, hit->gsi_base);

	hit->matched = true;
	fwts_log_info(ctx->fw,
		"AML_100: %s matches MADT %s id=%u gsi_base=0x%" PRIx32 ".",
		device_path, kind, hit->id, hit->gsi_base);

	return AE_OK;
}

static unsigned int method_brsi_intc_unmatched(
	fwts_framework *fw,
	fwts_list *list)
{
	fwts_list_link *item;
	unsigned int n = 0;

	fwts_list_foreach(item, list) {
		method_brsi_intc *ic =
			fwts_list_data(method_brsi_intc *, item);

		if (ic->matched)
			continue;

		fwts_log_info(fw,
			"AML_100: MADT %s id=%u gsi_base=0x%" PRIx32
			" has no namespace device.",
			method_brsi_intc_kind(ic->type), ic->id, ic->gsi_base);
		n++;
	}

	return n;
}

static int method_brsi_aml100(fwts_framework *fw)
{
	method_brsi_intc_ctx ctx;
	unsigned int intc_cnt;
	unsigned int missing;

	/*
	 * AML_100: a PLIC (RSCV0001) / APLIC (RSCV0002) namespace
	 * device MUST exist for every corresponding MADT entry.
	 * Match namespace _UID to MADT Id and _GSB to MADT GsiBase.
	 */
	memset(&ctx, 0, sizeof(ctx));
	ctx.fw = fw;

	if (method_brsi_collect_madt_intc(fw, &ctx) != FWTS_OK) {
		fwts_skipped(fw, "AML_100: MADT not available; skipping.");
		goto done;
	}

	intc_cnt = fwts_list_len(&ctx.intc_list);
	if (intc_cnt == 0) {
		fwts_skipped(fw,
			"AML_100: no MADT PLIC or APLIC entries; skipping.");
		goto done;
	}

	ctx.type = METHOD_BRSI_INTC_PLIC;
	AcpiGetDevices(method_brsi_intc_hid(ctx.type),
		method_brsi_intc_walk, &ctx, NULL);

	ctx.type = METHOD_BRSI_INTC_APLIC;
	AcpiGetDevices(method_brsi_intc_hid(ctx.type),
		method_brsi_intc_walk, &ctx, NULL);

	missing = method_brsi_intc_unmatched(fw, &ctx.intc_list);

	if (missing || ctx.failed)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "AML_100",
			"%u MADT PLIC/APLIC entry(ies) lack a namespace "
			"device, %u namespace device(s) failed to match "
			"(MADT entries %u, devices %u).",
			missing, ctx.failed, intc_cnt, ctx.found);
	else
		fwts_passed(fw,
			"AML_100: all %u MADT PLIC/APLIC entries have a "
			"matching namespace device via _UID/_GSB.",
			intc_cnt);

done:
	fwts_list_free_items(&ctx.intc_list, free);
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
	{ method_brsi_aml080,
	  "AML_080: PLIC and APLIC devices MUST implement _GSB." },
	{ method_brsi_aml090,
	  "AML_090: RSCV0003 UART devices MUST implement UART device properties." },
	{ method_brsi_aml100,
	  "AML_100: PLIC/APLIC namespace devices MUST exist for MADT entries." },
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
