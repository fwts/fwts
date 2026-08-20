/*
 * Copyright (C) 2015-2026 Canonical
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
acpi_table_init(SBST,&table)

/*
 *  SBST Extended System Description Table
 */
static int sbst_test1(fwts_framework *fw)
{
	fwts_acpi_table_sbst *sbst = (fwts_acpi_table_sbst*)table->data;
	bool passed = true;

	if (sbst->critical_energy_level > sbst->low_energy_level) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SBSTEnergyLevel1",
			"SBST Critical Energy Level (%" PRIu32 ") "
			"is greater than the Low Energy Level (%" PRIu32 ").",
			sbst->critical_energy_level,
			sbst->low_energy_level);
	}
	if (sbst->low_energy_level > sbst->warning_energy_level) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SBSTEnergeyLevel2",
			"SBST Low Energy Energy Level (%" PRIu32 ") "
			"is greater than the Warning Energy Level (%" PRIu32 ").",
			sbst->low_energy_level,
			sbst->warning_energy_level);
	}
	if (sbst->warning_energy_level == 0) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SBSTEnergyLevelZero",
			"SBST Warning Energy Level is zero, "
			"which is probably too low.");
	}

	if (passed)
		fwts_passed(fw, "No issues found in SBST table.");

	return FWTS_OK;
}

static fwts_framework_minor_test sbst_tests[] = {
	{ sbst_test1, "SBST Smart Battery Specification Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops sbst_ops = {
	.description = "SBST Smart Battery Specification Table test.",
	.init        = SBST_init,
	.minor_tests = sbst_tests
};

FWTS_REGISTER("sbst", &sbst_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
