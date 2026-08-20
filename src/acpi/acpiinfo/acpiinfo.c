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
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int acpiinfo_compiled_by(fwts_framework *fw, char *name, int instance)
{
	char *compiler;
	char tmp[80];
	char num[10];
	fwts_acpi_table_info *table;
	fwts_acpi_table_header *header;

	if (fwts_acpi_find_table(fw, name, instance < 0 ? 0 : instance, &table) != FWTS_OK)
		return FWTS_ERROR;

	if (table == NULL || table->data == NULL)
		return FWTS_ERROR;

	header = (fwts_acpi_table_header *)table->data;

	if (strncmp(header->creator_id, "MSFT", 4) == 0) {
		compiler = "Microsoft";
	} else if (strncmp(header->creator_id, "INTL", 4) == 0) {
		compiler = "Intel";
	} else {
		snprintf(tmp, sizeof(tmp), "Unknown (%4.4s)", header->creator_id);
		compiler = tmp;
	}

	if (instance == -1)
		*num = '\0';
	else
		snprintf(num, sizeof(num), "%d", instance);

	fwts_log_info(fw,
		"Table %4.4s%s, OEM %6.6s, created with %4.4s (%s) compiler.",
		header->signature, num, header->oem_id, header->creator_id, compiler);

	return FWTS_OK;
}

static int acpiinfo_test1(fwts_framework *fw)
{
	char *str;

	if (((str = fwts_get("/sys/module/acpi/parameters/acpica_version")) == NULL) &&
	    ((str = fwts_get("/proc/acpi/info")) == NULL))
			fwts_log_info(fw,
			"Cannot get ACPI version info from "
			"/sys/module/acpi/parameters/acpica_version or /proc/acpi/info");
	else {
		float version;
		int yearmonth;

		fwts_chop_newline(str);

		sscanf(str, "%6d", &yearmonth);

		if (yearmonth > 202103) {
			version = 6.4;
		} else if (yearmonth > 201902) {
			version = 6.3;
		} else if (yearmonth > 201609) {
			version = 6.2;
		} else if (yearmonth > 201509) {
			version = 6.1;
		} else if (yearmonth > 201404) {
			version = 6.0;
		} else if (yearmonth > 201110) {
			version = 5.0;
		} else if (yearmonth > 200906) {
			version = 4.0;
		} else if (yearmonth > 200505) {
			version = 3.0;
		} else {
			version = 2.0;
		}

		fwts_log_info(fw, "Kernel ACPICA driver version: %s, supports ACPI %2.1f", str, version);
		free(str);
	}

	fwts_infoonly(fw);

	return FWTS_OK;
}

static int acpiinfo_test2(fwts_framework *fw)
{
	fwts_acpi_table_info *table;
	uint8_t major;
	uint8_t minor = 0;

	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK)
		return FWTS_ERROR;

	if (table == NULL || table->data == NULL)
		return FWTS_ERROR;

	fwts_get_fadt_version(fw, &major, &minor);

	fwts_log_info(fw,
		"FACP ACPI Version: %" PRIu8 ".%" PRIu8, major, minor);

	fwts_infoonly(fw);

	return FWTS_OK;
}

static int acpiinfo_test3(fwts_framework *fw)
{
	int i;

	fwts_log_info(fw,
		"Determine the compiler used to generate the ACPI AML in the DSDT and SSDT.");

	acpiinfo_compiled_by(fw, "DSDT", -1);

	for (i = 0; i < 32 ; i++) {
		/* If we can't fetch the Nth SSDT, we've reached the end */
		if (acpiinfo_compiled_by(fw, "SSDT", i) != FWTS_OK)
			break;
	}

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test acpiinfo_tests[] = {
	{ acpiinfo_test1, "Determine Kernel ACPI version." },
	{ acpiinfo_test2, "Determine machine's ACPI version." },
	{ acpiinfo_test3, "Determine AML compiler." },
	{ NULL, NULL }
};

static fwts_framework_ops acpiinfo_ops = {
	.description = "General ACPI information test.",
	.minor_tests = acpiinfo_tests
};

FWTS_REGISTER("acpiinfo", &acpiinfo_ops, FWTS_TEST_EARLY, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
