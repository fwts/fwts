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

static fwts_acpi_table_info *mtable;
static bool acpi_initialized;

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

static fwts_framework_minor_test method_brsi_tests[] = {
	{ method_brsi_aml010,
	  "AML_010: PCIe Root Complex _CRS SHOULD NOT return I/O ranges." },
	{ method_brsi_aml020,
	  "AML_020: _PRS and _SRS methods SHOULD NOT be implemented." },
	{ method_brsi_aml030,
	  "AML_030: per-hart devices MUST be under \\_SB, not \\_PR." },
	{ NULL, NULL }
};

static fwts_framework_ops method_brsi_ops = {
	.description = "RISC-V BRS-I ACPI Methods and Objects test.",
	.init        = method_brsi_init,
	.deinit      = method_brsi_deinit,
	.minor_tests = method_brsi_tests
};

FWTS_REGISTER("method_brsi", &method_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
