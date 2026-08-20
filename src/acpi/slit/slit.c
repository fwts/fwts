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

#define	INDEX(i, j)	(((i) * slit->num_of_system_localities) + (j))

static fwts_acpi_table_info *table;
acpi_table_init(SLIT, &table)

/*
 *  For SLIT System Locality Distance Information refer to
 *    section 5.2.17 of the ACPI specification version 6.0
 */
static int slit_test1(fwts_framework *fw)
{
	bool passed = true;
	uint64_t i, j, size, n, reserved = 0, bad_entry = 0;
	uint8_t *entry;
	fwts_acpi_table_slit *slit = (fwts_acpi_table_slit *)table->data;

	/* Size sanity check #1, got enough table to at least get matrix size */
	if (!fwts_acpi_table_length(fw, "SLIT", table->length, sizeof(fwts_acpi_table_slit))) {
		passed = false;
		goto done;
	}

	n = slit->num_of_system_localities;
	fwts_log_info_verbatim(fw, "SLIT System Locality Distance Information Table:");
	fwts_log_info_simp_int(fw, "  Number of Localities:     ", n);

	/*
	 *  ACPI table length is 32 bits, so maximum matrix of entries size is
	 *  is 2^32 - sizeof(fwts_acpi_table_slit) = 2^32 - 44 = 4294967252
	 *  and table is a N x N matrix, so maximum number of localities is
	 *  limited to int(sqrt(4294967252)) = 65535.
	 */
	if (n > 0xffff) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SLITTooManySystemLocalities",
			"SLIT table size is %zu, however, the number of "
			"System Locality Entries is %" PRIu64 " which "
			"results in an ACPI table larger than the maximum "
			"32 bit ACPI table size of 4MB",
			table->length,
			n);
		goto done;
	}

	/*
	 *  Now that we are confident of no overflow, check that the matrix
	 *  + SLIT table header is not bigger than the actual table.
	 */
	size = (n * n) + sizeof(fwts_acpi_table_slit);
	if ((uint64_t)table->length < size) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SLITTooManySystemLocalities",
			"SLIT table size is %zu, however, the number of "
			"System Locality Entries is %" PRIu64 " and this "
			"results in a table of size %" PRIu64 " which "
			"is larger than the SLIT table size",
			table->length, size, n);
		goto done;
	}

	entry = (uint8_t *)table->data + sizeof(fwts_acpi_table_slit);

	/*
	 *  Now sanity check the entries..
	 */
	if (entry[INDEX(0,0)] != 10) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SLITBadCornerEntry",
			"SLIT Entry[0][0] is 0x%" PRIx8 ", expecting value 0x0a.",
			entry[INDEX(0,0)]);
	}
	if (entry[INDEX(n - 1, n - 1)] != 10) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SLITBadCornerEntry",
			"SLIT Entry[%" PRIu64 "][%" PRIu64 "] is 0x%" PRIx8 ", expecting value 0x0a.",
			n - 1, n - 1,
			entry[INDEX(n - 1, n - 1)]);
	}

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {

			/* Check for distances less than 10 (reserved, no meaning) */
			if (entry[INDEX(i, j)] < 10) {
				reserved++;
				/* Report first 16 errors */
				if (reserved < 16) {
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"SLITEntryReserved",
						"SLIT Entry[%" PRIu64 "][%" PRIu64 "]"
						" is 0x%" PRIx8 " which is a reserved value"
						" and has no defined meaning",
						i, j, entry[INDEX(i, j)]);
				}
			}
		}
	}

	if (reserved)
		fwts_log_info(fw, "Total of %" PRIu64 " entries were using reserved values",
			reserved);
	if (bad_entry)
		fwts_log_info(fw, "Total of %" PRIu64 " entries were not matching "
			"their diagonal parner element", bad_entry);
done:
	if (passed)
		fwts_passed(fw, "No issues found in SLIT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test slit_tests[] = {
	{ slit_test1, "SLIT System Locality Distance Information test." },
	{ NULL, NULL }
};

static fwts_framework_ops slit_ops = {
	.description = "SLIT System Locality Distance Information test.",
	.init        = SLIT_init,
	.minor_tests = slit_tests
};

FWTS_REGISTER("slit", &slit_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
