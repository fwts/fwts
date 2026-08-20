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

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

static int osilinux_test1(fwts_framework *fw)
{
	fwts_acpi_table_info *info;
	fwts_list_link *item;
	fwts_list_link *dumpitem = NULL;
	fwts_list* disassembly = NULL;
	int depth = 0;
	int dumpdepth = 0;
	bool found = false;

	if (fwts_iasl_init(fw) != FWTS_OK) {
		fwts_aborted(fw, "Failure to initialise iasl, aborting.");
		fwts_iasl_deinit();
		return FWTS_ERROR;
	}
	if (fwts_acpi_find_table(fw, "DSDT", 0, &info) != FWTS_OK) {
		fwts_log_error(fw, "Cannot located DSDT.");
                return FWTS_ERROR;
        }
        if (info) {
		if (fwts_iasl_disassemble(fw, info, true, &disassembly) != FWTS_OK) {
			fwts_aborted(fw, "Cannot disassemble DSDT with iasl.");
			fwts_iasl_deinit();
			return FWTS_ERROR;
		}
	}
	fwts_iasl_deinit();

	if (disassembly == NULL) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoDSDT",
			"Could not read ACPI DSDT table.");
		return FWTS_ERROR;
	}

	fwts_log_info(fw,
		"This is not strictly a failure mode, it just alerts "
		"one that this has been defined in the DSDT and probably "
		"should be avoided since the Linux ACPI driver matches "
		"onto the Windows _OSI strings");

	fwts_list_foreach(item, disassembly) {
		char *line = fwts_text_list_text(item);
		char *str;

		if (strstr(line, "{")) {
			depth++;
			if (dumpdepth == 0)
				dumpitem = item;
		}

		if (strstr(line, "}")) {
			depth--;
			if (dumpdepth != 0 && dumpdepth != depth) {
				found = true;
				while (dumpitem != NULL &&
				       dumpitem != item->next) {
					fwts_log_warning_verbatim(fw, "%s", fwts_text_list_text(dumpitem));
					dumpitem = dumpitem->next;
				}
				dumpdepth = 0;
				fwts_warning(fw, "DSDT implements a deprecated _OSI(\"Linux\") test.");
			}
		}
		if ((str = strstr(line, "_OSI")) != NULL) {
			if (strstr(str + 4, "Linux") ||
			    strstr(str + 4, "linux"))
				dumpdepth = depth;
		}
	}
	fwts_text_list_free(disassembly);

	if (!found)
		fwts_passed(fw, "DSDT does not implement a deprecated _OSI(\"Linux\") test.");

	return FWTS_OK;
}

static fwts_framework_minor_test osilinux_tests[] = {
	{ osilinux_test1, "Disassemble DSDT to check for _OSI(\"Linux\")." },
	{ NULL, NULL }
};

static fwts_framework_ops osilinux_ops = {
	.description = "Disassemble DSDT to check for _OSI(\"Linux\").",
	.minor_tests = osilinux_tests
};

FWTS_REGISTER("osilinux", &osilinux_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)

#endif
