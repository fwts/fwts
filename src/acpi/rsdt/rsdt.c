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

#if defined(FWTS_HAS_ACPI) && !(FWTS_ARCH_AARCH64)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

static fwts_acpi_table_info *table;
acpi_table_init(RSDT, &table)

/*
 *  RSDT Extended System Description Table
 */
static int rsdt_test1(fwts_framework *fw)
{
	fwts_acpi_table_rsdt *rsdt = (fwts_acpi_table_rsdt*)table->data;
	size_t i, n;
	bool passed = true;

	n = (table->length - sizeof(fwts_acpi_table_header)) / sizeof(uint32_t);
	for (i = 0; i < n; i++) {
		if (rsdt->entries[i] == 0) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"RSDTEntryNull",
				"RSDT Entry %zd is null, should not be non-zero.", i);
			fwts_advice(fw,
				"A RSDT pointer is null and therefore erroneously "
				"points to an invalid 32 bit ACPI table header. "
				"At worse this will cause the kernel to oops, at "
				"best the kernel may ignore this.  However, it "
				"should be fixed where possible.");
		}
	}
	if (passed)
		fwts_passed(fw, "No issues found in RSDT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test rsdt_tests[] = {
	{ rsdt_test1, "RSDT Root System Description Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops rsdt_ops = {
	.description = "RSDT Root System Description Table test.",
	.init        = RSDT_init,
	.minor_tests = rsdt_tests
};

FWTS_REGISTER("rsdt", &rsdt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
