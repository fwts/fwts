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

static fwts_framework_minor_test method_brsi_tests[] = {
	{ method_brsi_aml010,
	  "AML_010: PCIe Root Complex _CRS SHOULD NOT return I/O ranges." },
	{ method_brsi_aml020,
	  "AML_020: _PRS and _SRS methods SHOULD NOT be implemented." },
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
