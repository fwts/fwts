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
static bool acpi_initialized;
static fwts_acpi_table_info *table;
static fwts_list *klog;
static bool exist_gop;

static int spcr_brsi_init(fwts_framework *fw)
{
	int rc;

	rc = fwts_acpi_find_table(fw, "SPCR", 0, &table);
	if (rc != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI tables.");
		return FWTS_ERROR;
	}

	if (fw->klog)
		klog = fwts_file_open_and_read(fw->klog);
	else
		klog = fwts_klog_read();

	if (!klog) {
		fwts_log_error(fw, "Cannot read kernel log.");
		return FWTS_ERROR;
	}

	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}
	acpi_initialized = true;

	return rc;
}

static int spcr_brsi_deinit(fwts_framework *fw)
{

	if (klog)
		fwts_klog_free(klog);

	if (acpi_initialized)
		fwts_acpi_deinit(fw);

	return FWTS_OK;
}

/*
 * Best-effort check that graphics were exposed to an OS loader through
 * EFI_GRAPHICS_OUTPUT_PROTOCOL. After ExitBootServices the protocol itself
 * is gone, so we look for surviving firmware artifacts:
 *
 *   - Kernel log messages from efifb or simpledrm (these remain even after
 *     a native GPU driver takes over the framebuffer). A user-provided
 *     kernel log dump (--klog) is also accepted.
 *   - BGRT table (produced from GOP boot graphics)
 *   - Platform devices created by sysfb (efi-framebuffer.0 or
 *     simple-framebuffer.0), only on live systems
 *
 * In offline/dumpfile mode, sysfs is not trusted. BGRT and a user-provided
 * kernel log dump remain valid evidence.
 */
static bool graphics_gop_available(fwts_framework *fw)
{
	bool offline;
	fwts_acpi_table_info *bgrt = NULL;
	static const char *const paths[] = {
		"/sys/bus/platform/devices/efi-framebuffer.0",
		"/sys/bus/platform/devices/simple-framebuffer.0",
		NULL
	};
	const char *const *path;
	static char *gop_klog_patterns[] = {
		"efifb: probing for efifb",
		"[drm] Initialized simpledrm .* for simple-framebuffer.* minor .*",
		NULL
	};
	char **pattern;

	if (fwts_acpi_find_table(fw, "BGRT", 0, &bgrt) == FWTS_OK &&
	    bgrt != NULL && bgrt->length > 0) {
		fwts_log_info(fw, "BGRT table present; treating GOP as available.");
		return true;
	}

	offline = fw->acpi_table_acpidump_file || fw->acpi_table_path;

	if (!offline || fw->klog) {
		for (pattern = gop_klog_patterns; klog && *pattern != NULL; pattern++) {
			if (fwts_klog_regex_find(fw, klog, (char*)*pattern)) {
				fwts_log_info(fw,
					"Kernel log contains firmware framebuffer evidence "
					"(pattern: %s); treating GOP as available.",
					*pattern);
				return true;
			}
		}
	}

	if (!offline) {
		for (path = paths; *path != NULL; path++) {
			if (access(*path, F_OK) == 0) {
				fwts_log_info(fw,
					"%s exists; treating EFI GOP framebuffer as available.",
					*path);
				return true;
			}
		}
	}

	fwts_log_info(fw,
		"No kernel log evidence, BGRT table, or firmware framebuffer "
		"device (efi-framebuffer/simple-framebuffer) found; "
		"graphics are treated as unavailable to the OS loader.");
	return false;
}

static int spcr_brsi_test1(fwts_framework *fw)
{
	bool have_spcr = (table != NULL && table->length > 0);

	if (have_spcr) {
		fwts_passed(fw,
			"The Serial Port Console Redirection Table (SPCR) is present "
			"on this RISC-V BRS system.");
	} else if (exist_gop) {
		fwts_passed(fw,
			"SPCR is not present, but --brs-i-gop was specified; "
			"ACPI_050 does not require SPCR in this case.");
	} else if (graphics_gop_available(fw)) {
		fwts_passed(fw,
			"SPCR is not present, but graphics hardware appears available "
			"to an OS loader via EFI_GRAPHICS_OUTPUT_PROTOCOL; "
			"ACPI_050 does not require SPCR in this case.");
	} else {
		fwts_warning(fw,
			"No SPCR table found and no evidence of GOP was detected. "
			"This does not prove GOP was unavailable. "
			"If graphics were not exposed via EFI_GRAPHICS_OUTPUT_PROTOCOL, "
			"this is a BRS violation. Re-run with --brs-i-gop if GOP was present.");
	}

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
	ACPI_STATUS status;
	bool rc = true;
	ACPI_RESOURCE_ADDRESS64 addr64;

	switch (resource->Type) {
	case ACPI_RESOURCE_TYPE_MEMORY24:
		*base = resource->Data.Memory24.Minimum << 8;
		*length = resource->Data.Memory24.AddressLength << 8;
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
		status =AcpiResourceToAddress64(
				(ACPI_RESOURCE *)resource, &addr64);
		if (ACPI_FAILURE(status) || (addr64.ResourceType != ACPI_MEMORY_RANGE)) {
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
	ACPI_STATUS status;
	ns16550_search *search = context;
	fwts_framework *fw = search->fw;

	FWTS_UNUSED(level);
	FWTS_UNUSED(ret_val);

	status = AcpiWalkResources(handle, "_CRS", match_crs_base, search);
	if (ACPI_FAILURE(status))
		fwts_log_error(fw,"failed to walk _CSR of "NS16550_HID);

	if (!search->matched)
		return AE_OK;

	search->device = handle;
	return AE_CTRL_TERMINATE;
}

static bool find_ns16550_device(ns16550_search *search)
{
	ACPI_STATUS status;
	fwts_framework *fw = search->fw;
	search->matched = false;
	search->device = NULL;

	status = AcpiGetDevices(NS16550_HID, get_ns16550_handle, search, NULL);
	if (ACPI_FAILURE(status))
		fwts_log_error(fw, "Cannot find the ACPI device ("NS16550_HID")");

	return search->matched;
}

static int spcr_brsi_test2(fwts_framework *fw)
{
	const fwts_acpi_table_spcr *spcr;
	const char *ns;
	ns16550_search search;

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
	if (spcr->header.revision < 4) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR revision is %" PRIu8 ", MUST be revision 4 or later "
			"(per ACPI_060).",
			spcr->header.revision);
		return FWTS_OK;
	}

	if (table->length < offsetof(fwts_acpi_table_spcr, namespace_str)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR table is too short to read the revision-4 NamespaceString "
			"fields (length %" PRIu64 ", need at least %zu).",
			table->length, offsetof(fwts_acpi_table_spcr, namespace_str));
		return FWTS_OK;
	}

	if (spcr->namespace_str_offset < offsetof(fwts_acpi_table_spcr, namespace_str)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR NamespaceStringOffset 0x%" PRIx16 " is within the fixed "
			"fields (must be >= %zu).",
			spcr->namespace_str_offset,
			offsetof(fwts_acpi_table_spcr, namespace_str));
		return FWTS_OK;
	}

	if (spcr->namespace_str_offset + spcr->namespace_str_length > table->length) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR NamespaceString (offset 0x%" PRIx16 ", length %" PRIu16
			") extends beyond the table (length %" PRIu64 ").",
			spcr->namespace_str_offset, spcr->namespace_str_length,
			table->length);
		return FWTS_OK;
	}

	fwts_log_info(fw, "SPCR Interface Type: 0x%" PRIx8, spcr->interface_type);

	if (spcr->interface_type != NS16550_IF) {
		search.spcr_base = spcr->base_address.address;
		if (find_ns16550_device(&search)) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
				"SPCR Interface Type is 0x%02" PRIx8 ", but an RSCV0003 "
				"device's _CRS covers the SPCR base address 0x%" PRIx64 ". "
				"NS16550-compatible UARTs MUST use Interface Type 0x12.",
				spcr->interface_type, (uint64_t)spcr->base_address.address);
		} else if (spcr->interface_type == 0 || spcr->interface_type == 1) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
				"SPCR Interface Type 0x%02" PRIx8 " is NS16550-compatible; "
				"ACPI_060 requires Interface Type 0x12 for NS16550-compatible UARTs.",
				spcr->interface_type);
		} else if (spcr->interface_type != 0x03 && spcr->interface_type != 0x15) {
			fwts_warning(fw,
				"SPCR Interface Type 0x%02" PRIx8 " is not one of the "
				"types listed in BRS §8.3.9 (0x03, 0x12, 0x15).",
				spcr->interface_type);
		} else {
			fwts_log_info(fw,
				"SPCR Interface Type is 0x%" PRIx8 ", not 0x12; "
				"RSCV0003 AML device check does not apply.",
				spcr->interface_type);
		}
		return FWTS_OK;
	}

	if (spcr->base_address.address_space_id != FWTS_GAS_ADDR_SPACE_ID_SYSTEM_MEMORY) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR base address Address Space ID is 0x%02" PRIx8 ", "
			"not System Memory (0x00). NS16550-compatible UART with "
			"Interface Type 0x12 is expected to be memory-mapped.",
			spcr->base_address.address_space_id);
		return FWTS_OK;
	}

	ns = (const void*)spcr + spcr->namespace_str_offset;
	if (strcmp(ns, ".") == 0) {
		fwts_warning(fw,
			"SPCR Interface Type is 0x12 but NamespaceString is '.'. SPCR "
			"revision 4 defines NamespaceString to name the matching device; "
			"'.' indicates no namespace device.");
	}

	fwts_log_info(fw,
		"SPCR Interface Type is 0x12 (16550-compatible with parameters "
		"defined in Generic Address Structure).");

	search.spcr_base = spcr->base_address.address;
	if (find_ns16550_device(&search)) {
		ACPI_BUFFER name_buf = { ACPI_ALLOCATE_BUFFER, NULL };

		fwts_passed(fw,
			"Found a matching AML device object with _HID or _CID %s "
			"whose _CRS covers the SPCR base address.",
			NS16550_HID);

		if (ACPI_FAILURE(AcpiGetName(search.device, ACPI_FULL_PATHNAME, &name_buf))
				|| (name_buf.Pointer == NULL)) {
			fwts_log_error(fw,
				"Found a matching RSCV0003 device, but AcpiGetName failed; "
				"cannot compare with SPCR NamespaceString \"%s\".", ns);
			return FWTS_OK;
		}
		if (strcmp(ns, name_buf.Pointer) != 0) {
			fwts_warning(fw,
				"SPCR NamespaceString is \"%s\", but the matching "
				"RSCV0003 device path is \"%s\". "
				"SPCR revision 4 NamespaceString should be the fully "
				"qualified path of the device.",
				ns, (char *)name_buf.Pointer);
		}
		free(name_buf.Pointer);
	} else {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_060",
			"SPCR Interface Type 0x12 MUST have a matching AML device "
			"object with _HID or _CID %s whose _CRS covers the SPCR "
			"base address 0x%" PRIx64 " (per ACPI_060).",
			NS16550_HID, (uint64_t)spcr->base_address.address);
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
		case 0:	/* --brs-i-gop */
			exist_gop = true;
			break;
		}
	}
	return FWTS_OK;
}

static fwts_option options[] = {
	{ "brs-i-gop", "", 0,
	  "Platform has implemented GOP (Graphics Output Protocol)" },
	{NULL, NULL, 0, NULL}
};

static fwts_framework_minor_test spcr_brsi_tests[] = {
	{ spcr_brsi_test1, "Check SPCR table presence when GOP is unavailable." },
	{ spcr_brsi_test2, "Check SPCR ACPI_060 revision, interface type and RSCV0003." },
	{ NULL, NULL }
};

static fwts_framework_ops spcr_brsi_ops = {
	.description     = "RISC-V BRS-I SPCR Serial Port Console Redirection Table test.",
	.init            = spcr_brsi_init,
	.deinit          = spcr_brsi_deinit,
	.minor_tests     = spcr_brsi_tests,
	.options         = options,
	.options_handler = options_handler
};

FWTS_REGISTER("spcr_brsi", &spcr_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
