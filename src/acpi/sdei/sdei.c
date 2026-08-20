/*
 * Copyright (C) 2017-2026 Canonical
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
#include <inttypes.h>
#include <string.h>

static fwts_acpi_table_info *table;
acpi_table_init(SDEI, &table)

static int sdei_test1(fwts_framework *fw)
{
	fwts_acpi_table_sdei *sdei = (fwts_acpi_table_sdei *) table->data;
	bool passed = true;

	fwts_log_info_verbatim(fw, "SDEI (Software Delegated Exception Interface) Table:");

	/* Current spec says:
	 * "The table consists only of a basic header with revision 1."
	 * "Later revisions of the SDEI table may define additional fields."
	 *
	 * More validation will be implemented for revision 2 and later.
	 */
	switch (sdei->header.revision) {
	case 1:
		/* nothing to validate */
		break;
	default:
		fwts_log_info(fw, "Unsupported SDEI Revision %" PRIu8, sdei->header.revision);
		break;
	}

	if (passed)
		fwts_passed(fw, "No issues found in SDEI table.");

	return FWTS_OK;
}

static fwts_framework_minor_test sdei_tests[] = {
	{ sdei_test1, "Validate SDEI table." },
	{ NULL, NULL }
};

static fwts_framework_ops sdei_ops = {
	.description = "SDEI Software Delegated Exception Interface Table test",
	.init        = SDEI_init,
	.minor_tests = sdei_tests
};

FWTS_REGISTER("sdei", &sdei_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
