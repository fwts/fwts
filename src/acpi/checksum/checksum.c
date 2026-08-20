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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

static void checksum_rsdp(fwts_framework *fw, fwts_acpi_table_info *table)
{
	uint8_t checksum;
	fwts_acpi_table_rsdp *rsdp = (fwts_acpi_table_rsdp*)table->data;

	if (table->length < 20) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "ACPITableCheckSumShortRSDP",
			"RSDP was expected to be at least 20 bytes long, "
			"got a shortened size of %zd bytes.",
			table->length);
		/* Won't test on a short RSDP */
		return;
	}

	/* Version 1.0 RSDP checksum, always applies */
	checksum = fwts_checksum(table->data, 20);
	if (checksum != 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPITableChecksumRSDP",
			"RSDP has incorrect checksum, expected 0x%2.2" PRIx8 ", "
			"got 0x%2.2" PRIx8 ".",
			(uint8_t)(rsdp->checksum-checksum), rsdp->checksum);
		fwts_advice(fw,
			"The kernel will not load the RSDP with an "
			"invalid checksum and hence all other ACPI "
			"tables will also fail to load.");
	} else
		fwts_passed(fw, "Table RSDP has correct checksum 0x%2.2" PRIx8 ".",
			rsdp->checksum);

	/*
	 * Version 2.0 RSP or more. Note ACPI 1.0 is indicated by a
	 * zero version number
	 */
	if (rsdp->revision > 0) {
		if (table->length < sizeof(fwts_acpi_table_rsdp)) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"ACPITableCheckSumShortRSDP",
				"RSDP was expected to be %zd bytes long, "
				"got a shortened size of %zd bytes.",
				sizeof(fwts_acpi_table_rsdp),
				table->length);
			/* Won't test on a short RSDP */
			return;
		}
		checksum = fwts_checksum(table->data, sizeof(fwts_acpi_table_rsdp));
		if (checksum != 0) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"ACPITableChecksumRSDP",
				"RSDP has incorrect extended checksum, "
				"expected 0x%2.2" PRIx8 ", got 0x%2.2" PRIx8 ".",
				(uint8_t)(rsdp->extended_checksum-checksum),
				rsdp->extended_checksum);
			fwts_advice(fw,
				"The kernel will not load the RSDP with an "
				"invalid extended checksum and hence all "
				"other ACPI tables will also fail to load.");
		} else
			fwts_passed(fw, "Table RSDP has correct extended "
				"checksum 0x%2.2" PRIx8 ".", rsdp->extended_checksum);
	}

}

static int checksum_scan_tables(fwts_framework *fw)
{
	int i;

	for (i = 0;; i++) {
		fwts_acpi_table_info *table;
		fwts_acpi_table_header *hdr;
		uint8_t checksum;

		if (fwts_acpi_get_table(fw, i, &table) != FWTS_OK) {
			fwts_aborted(fw, "Cannot load ACPI tables.");
			return FWTS_ABORTED;
		}
		if (table == NULL)
			break;

		hdr = (fwts_acpi_table_header*)table->data;

		if (strcmp("RSDP", table->name) == 0) {
			checksum_rsdp(fw, table);
			continue;
		}

		/* FACS doesn't have a checksum, so ignore */
		if (strcmp("FACS", table->name) == 0)
			continue;

		checksum = fwts_checksum(table->data, table->length);
		if (checksum == 0)
			fwts_passed(fw, "Table %s has correct checksum 0x%2.2" PRIx8,
				table->name, hdr->checksum);
		else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPITableChecksum",
				"Table %s has incorrect checksum, "
				"expected 0x%2.2" PRIx8 ", got 0x%2.2" PRIx8 ".",
				table->name, (uint8_t)(hdr->checksum-checksum),
				hdr->checksum);

			fwts_advice(fw,
				"The kernel will warn that this table has "
				"an invalid checksum but will ignore the "
				"error and still load it. This is not a "
				"critical issue, but should be fixed if "
				"possible to avoid the warning messages.");
		}
	}
	return FWTS_OK;
}

static int checksum_test1(fwts_framework *fw)
{
	return checksum_scan_tables(fw);
}

static fwts_framework_minor_test checksum_tests[] = {
	{ checksum_test1, "ACPI table checksum test." },
	{ NULL, NULL }
};

static fwts_framework_ops checksum_ops = {
	.description = "ACPI table checksum test.",
	.minor_tests = checksum_tests
};

FWTS_REGISTER("checksum", &checksum_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
