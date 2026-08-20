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
#include <ctype.h>

static fwts_acpi_table_info *table;
acpi_table_init(CPEP, &table)

/*
 *  CPEP Corrected Platform Error Polling Table
 */
static int cpep_test1(fwts_framework *fw)
{
	fwts_acpi_table_cpep *cpep = (fwts_acpi_table_cpep*)table->data;
	bool passed = true;
	uint64_t reserved;
	uint32_t i, n;

	fwts_log_info_verbatim(fw, "CPEP Corrected Platform Error Polling Table:");
	fwts_log_info_verbatim(fw, "  Reserved:                "
		" 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8
		" 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 ,
		cpep->reserved[0], cpep->reserved[1], cpep->reserved[2], cpep->reserved[3],
		cpep->reserved[4], cpep->reserved[4], cpep->reserved[6], cpep->reserved[7]);

	reserved = cpep->reserved[0] + ((uint64_t) cpep->reserved[1] << 8) +
		   ((uint64_t) cpep->reserved[2] << 16) + ((uint64_t) cpep->reserved[3] << 24) +
		   ((uint64_t) cpep->reserved[4] << 32) + ((uint64_t) cpep->reserved[5] << 40) +
		   ((uint64_t) cpep->reserved[6] << 48) + ((uint64_t) cpep->reserved[7] << 56);
	fwts_acpi_reserved_zero("CPEP", "Reserved", reserved, &passed);

	n = (table->length - sizeof(fwts_acpi_table_cpep)) /
		sizeof(fwts_acpi_cpep_processor_info);

	for (i = 0; i < n; i++) {
		bool cpep_passed = true;
		char label[40];

		fwts_acpi_cpep_processor_info *info = &cpep->cpep_info[i];

		snprintf(label, 40, "Processor Structure %d Type", i);
		fwts_acpi_fixed_value(fw, LOG_LEVEL_HIGH, "CPEP", label, info->type, 0, &cpep_passed);

		snprintf(label, 40, "Processor Structure %d Length", i);
		fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "CPEP", label, info->length, 8, &cpep_passed);

		/* Should probably sanity check processor UID, EIDs at a later date */

		/* Some feedback if polling interval is very short */
		if (info->polling_interval < 5) {
			fwts_warning(fw, "CPEP Processor Structure %" PRIu32
				" Polling Interval of %" PRIu32
				" milliseconds is relatively short and may "
				"cause excessive polling CPU load",
				i, info->polling_interval);
		}
		if (cpep_passed)
			fwts_log_info(fw, "CPEP Processor Structure %" PRIu32 " looks sane.", i);
		passed &= cpep_passed;
	}


	if (passed)
		fwts_passed(fw, "No issues found in CPEP table.");

	return FWTS_OK;
}

static fwts_framework_minor_test cpep_tests[] = {
	{ cpep_test1, "CPEP Corrected Platform Error Polling Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops cpep_ops = {
	.description = "CPEP Corrected Platform Error Polling Table test.",
	.init        = CPEP_init,
	.minor_tests = cpep_tests
};

FWTS_REGISTER("cpep", &cpep_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
