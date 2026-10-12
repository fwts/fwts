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

#define NS16550_IF			0x12
#define NS16550_HID			"RSCV0003"
static fwts_acpi_table_info *table;

static int spcr_brsi_init(fwts_framework *fw)
{
	int rc;

	rc = acpi_table_generic_init(fw, "SPCR", &table);
	if (table == NULL || table->length == 0)
		return FWTS_OK;

	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return rc;
}

static int spcr_brsi_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

/*
 * Best-effort check that graphics were exposed to an OS loader through
 * EFI_GRAPHICS_OUTPUT_PROTOCOL.  After ExitBootServices the protocol itself
 * is gone, so use surviving firmware artifacts:
 *   - BGRT is produced from GOP boot graphics
 *   - Linux binds remaining GOP framebuffers as efi-framebuffer
 */
static bool graphics_gop_available(fwts_framework *fw)
{
	fwts_acpi_table_info *bgrt = NULL;
	static const char *const paths[] = {
		"/sys/bus/platform/devices/efi-framebuffer.0",
		"/sys/bus/platform/drivers/efi-framebuffer/efi-framebuffer.0",
		"/sys/firmware/efi/gop",
		NULL
	};
	const char *const *path;

	if (fwts_acpi_find_table(fw, "BGRT", 0, &bgrt) == FWTS_OK &&
	    bgrt != NULL && bgrt->length > 0) {
		fwts_log_info(fw, "BGRT table present; treating GOP as available.");
		return true;
	}

	for (path = paths; *path != NULL; path++) {
		if (access(*path, F_OK) == 0) {
			fwts_log_info(fw,
				"%s exists; treating EFI GOP framebuffer as available.",
				*path);
			return true;
		}
	}

	fwts_log_info(fw,
		"No BGRT table or EFI GOP framebuffer found; "
		"graphics are treated as unavailable to the OS loader.");
	return false;
}

static int spcr_brsi_test1(fwts_framework *fw)
{
	bool have_spcr = (table != NULL && table->length > 0);
	bool have_gop;

	if (have_spcr) {
		fwts_passed(fw,
			"The Serial Port Console Redirection Table (SPCR) is present "
			"on this RISC-V BRS system.");
		return FWTS_OK;
	}

	have_gop = graphics_gop_available(fw);
	if (have_gop) {
		fwts_passed(fw,
			"SPCR is not present, but graphics hardware appears available "
			"to an OS loader via EFI_GRAPHICS_OUTPUT_PROTOCOL; "
			"ACPI_050 does not require SPCR in this case.");
		return FWTS_OK;
	}

	fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_050",
		"A Serial Port Console Redirection Table MUST be present on "
		"systems where the graphics hardware is not present or not made "
		"available to an OS loader via the standard UEFI "
		"EFI_GRAPHICS_OUTPUT_PROTOCOL interface (per ACPI_050).");

	return FWTS_OK;
}

typedef struct {
	fwts_framework *fw;
	uint64_t spcr_base;
	bool matched;
	ACPI_HANDLE device;
} ns16550_search;

static bool resource_address(ACPI_RESOURCE *resource,
			     uint64_t *base, uint64_t *length)
{
	bool rc = true;
	ACPI_RESOURCE_ADDRESS64 addr64;

	switch (resource->Type) {
	case ACPI_RESOURCE_TYPE_MEMORY24:
		*base = resource->Data.Memory24.Minimum;
		*length = resource->Data.Memory24.AddressLength;
		break;
	case ACPI_RESOURCE_TYPE_MEMORY32:
		*base = resource->Data.Memory32.Minimum;
		*length = resource->Data.Memory32.AddressLength;
		break;
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		*base = resource->Data.FixedMemory32.Address;
		*length = resource->Data.FixedMemory32.AddressLength;
		break;
	default:
		if (ACPI_FAILURE(AcpiResourceToAddress64(
				(ACPI_RESOURCE *)resource, &addr64))) {
			rc = false;
			break;
		}
		*base = addr64.Address.Minimum;
		*length = addr64.Address.AddressLength;
		break;
	}
	return rc;
}

static ACPI_STATUS match_crs_base(ACPI_RESOURCE *resource, void *context)
{
	ns16550_search *search = context;
	uint64_t base, length;

	if (resource_address(resource, &base, &length)) {
		if (base <= search->spcr_base && search->spcr_base < base + length) {
			search->matched = true;
			return AE_CTRL_TERMINATE;
		}
	}

	return AE_OK;
}

/*
 * AcpiGetDevices() matches both _HID and _CID. Keep walking when _CRS
 * does not describe the same base address as SPCR.
 */
static ACPI_STATUS get_ns16550_handle(ACPI_HANDLE handle, uint32_t level,
				      void *context, void **ret_val)
{
	ns16550_search *search = context;

	FWTS_UNUSED(level);
	FWTS_UNUSED(ret_val);

	AcpiWalkResources(handle, "_CRS", match_crs_base, search);

	if (!search->matched)
		return AE_OK;

	search->device = handle;
	return AE_CTRL_TERMINATE;
}

static bool find_ns16550_device(fwts_framework *fw, uint64_t spcr_base)
{
	ns16550_search search = {
		.fw = fw,
		.spcr_base = spcr_base,
		.matched = false,
		.device = NULL
	};

	AcpiGetDevices(NS16550_HID, get_ns16550_handle, &search, NULL);

	return search.device != NULL;
}

static int spcr_brsi_test2(fwts_framework *fw)
{
	const fwts_acpi_table_spcr *spcr;

	if (table == NULL || table->length == 0) {
		fwts_skipped(fw,
			"SPCR table is not present; ACPI_060 applies only when SPCR exists.");
		return FWTS_SKIP;
	}

	if (table->length < sizeof(fwts_acpi_table_header)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR table is too short to read the header revision.");
		return FWTS_OK;
	}

	spcr = (const fwts_acpi_table_spcr *)table->data;

	fwts_log_info(fw, "SPCR revision: %" PRIu8, spcr->header.revision);
	if (spcr->header.revision >= 4)
		fwts_passed(fw, "SPCR revision is 4 or later as required by ACPI_060.");
	else
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR revision is %" PRIu8 ", MUST be revision 4 or later "
			"(per ACPI_060).",
			spcr->header.revision);

	if (table->length < offsetof(fwts_acpi_table_spcr, interrupt_type)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR table is too short to read Interface Type.");
		return FWTS_OK;
	}

	fwts_log_info(fw, "SPCR Interface Type: 0x%" PRIx8, spcr->interface_type);

	if (spcr->interface_type != NS16550_IF) {
		fwts_log_info(fw,
			"SPCR Interface Type is 0x%" PRIx8 ", not 0x12; "
			"RSCV0003 AML device check does not apply.",
			spcr->interface_type);
		return FWTS_OK;
	}

	fwts_log_info(fw,
		"SPCR Interface Type is 0x12 (16550-compatible with parameters "
		"defined in Generic Address Structure).");

	if (find_ns16550_device(fw, spcr->base_address.address))
		fwts_passed(fw,
			"Found a matching AML device object with _HID or _CID %s "
			"whose _CRS covers the SPCR base address.",
			NS16550_HID);
	else
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR Interface Type 0x12 MUST have a matching AML device "
			"object with _HID or _CID %s whose _CRS covers the SPCR "
			"base address 0x%" PRIx64 " (per ACPI_060).",
			NS16550_HID, (uint64_t)spcr->base_address.address);

	return FWTS_OK;
}

static fwts_framework_minor_test spcr_brsi_tests[] = {
	{ spcr_brsi_test1, "Check SPCR table presence when GOP is unavailable." },
	{ spcr_brsi_test2, "Check SPCR ACPI_060 revision, interface type and RSCV0003." },
	{ NULL, NULL }
};

static fwts_framework_ops spcr_brsi_ops = {
	.description = "RISC-V BRS-I SPCR Serial Port Console Redirection Table test.",
	.init        = spcr_brsi_init,
	.deinit      = spcr_brsi_deinit,
	.minor_tests = spcr_brsi_tests
};

FWTS_REGISTER("spcr_brsi", &spcr_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
