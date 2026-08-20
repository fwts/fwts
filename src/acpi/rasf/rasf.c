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
acpi_table_init(RASF, &table)

static int rasf_test1(fwts_framework *fw)
{
	fwts_acpi_table_rasf *rasf = (fwts_acpi_table_rasf *) table->data;
	bool passed = true;

	fwts_log_info_verbatim(fw, "RASF (RAS Feature) Table:");
	if (rasf->header.length == sizeof(fwts_acpi_table_rasf)) {
		uint8_t i;

		for (i = 0; i < 12; i++)
			fwts_log_info_verbatim(fw, "  Channel Identifier [%2.2" PRId8 "]:   0x%2.2" PRIx8, i, rasf->platform_cc_id[i]);
	} else {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "RASFBadLength",
			    "RASF structure must have length 0x%8.8" PRIx32 ", got "
			    "0x%8.8" PRIx32, (uint32_t)sizeof(fwts_acpi_table_rasf), rasf->header.length);
	}

	if (passed)
		fwts_passed(fw, "No issues found in RASF table.");

	return FWTS_OK;
}

static fwts_framework_minor_test rasf_tests[] = {
	{ rasf_test1, "Validate RASF table." },
	{ NULL, NULL }
};

static fwts_framework_ops rasf_ops = {
	.description = "RASF RAS Feature Table test",
	.init        = RASF_init,
	.minor_tests = rasf_tests
};

FWTS_REGISTER("rasf", &rasf_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
