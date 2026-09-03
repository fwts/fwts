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

	return rc;
}

static int spcr_brsi_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if (klog)
		fwts_klog_free(klog);

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
