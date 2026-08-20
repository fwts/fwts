/*
 * Copyright (C) 2010-2026 Canonical
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
#include <stdbool.h>

#include "fwts.h"

static int version_test1(fwts_framework *fw)
{
	char *str;
	fwts_release *release;

	release = fwts_release_get();
	if (release) {
		bool not_ubuntu = strcmp(release->distributor, "Ubuntu");

		fwts_release_free(release);
		/* Following is Ubuntu specific, so don't fail */
		if (not_ubuntu) {
			fwts_skipped(fw, "Information not available with this kernel.");
			return FWTS_OK;
		}
	}

	if ((str = fwts_get("/proc/version_signature")) == NULL)
		fwts_skipped(fw,
			"Cannot get version signature info from "
			"/proc/version_signature");
	else {
		fwts_chop_newline(str);
		fwts_log_info(fw, "Signature: %s", str);
		free(str);
	}

	fwts_infoonly(fw);

	return FWTS_OK;
}

static int version_test2(fwts_framework *fw)
{
	char *str;

	if ((str = fwts_get("/proc/version")) == NULL)
		fwts_log_info(fw, "Cannot get version info from /proc/version");
	else {
		fwts_chop_newline(str);
		fwts_log_info(fw, "Kernel Version: %s", str);
		free(str);
	}
	fwts_infoonly(fw);

	return FWTS_OK;
}

static int version_test3(fwts_framework *fw)
{
	char *str;

	if ((str = fwts_get("/proc/cmdline")) == NULL)
		fwts_log_info(fw, "Cannot get version info from /proc/cmdline");
	else {
		fwts_chop_newline(str);
		fwts_log_info(fw, "Kernel boot command line: %s", str);
		free(str);
	}
	fwts_infoonly(fw);

	return FWTS_OK;
}

#if defined(FWTS_HAS_ACPI)
static int version_test4(fwts_framework *fw)
{
	char *str;

	if ((str = fwts_get("/sys/module/acpi/parameters/acpica_version")) == NULL)
		fwts_log_info(fw,
			"Cannot get ACPI version info from "
			"/sys/module/acpi/parameters/acpica_version "
			"or /proc/acpi/info, system does not have ACPI.");
	else {
		fwts_chop_newline(str);
		fwts_log_info(fw, "ACPICA Version: %s", str);
		free(str);
	}
	fwts_infoonly(fw);

	return FWTS_OK;
}
#endif

static fwts_framework_minor_test version_tests[] = {
	{ version_test1, "Gather kernel signature." },
	{ version_test2, "Gather kernel system information." },
	{ version_test3, "Gather kernel boot command line." },
#if defined(FWTS_HAS_ACPI)
	{ version_test4, "Gather ACPI driver version." },
#endif
	{ NULL, NULL },
};

static fwts_framework_ops version_ops = {
	.description = "Gather kernel system information.",
	.minor_tests = version_tests
};

FWTS_REGISTER("version", &version_ops, FWTS_TEST_FIRST, FWTS_FLAG_BATCH)
