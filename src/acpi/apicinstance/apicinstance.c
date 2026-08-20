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

static int apicinstance_test1(fwts_framework *fw)
{
	fwts_acpi_table_info *first_madt_table = NULL;
	int i;
	int count;

	for (i = 0, count = 0;; i++) {
		fwts_acpi_table_info *table;

		if (fwts_acpi_get_table(fw, i, &table) != FWTS_OK) {
			fwts_aborted(fw, "Cannot load ACPI table.");
			return FWTS_ERROR;
		}

		if (table == NULL)
			break;

		if (strcmp(table->name, "APIC") == 0)  {
			fwts_log_info(fw, "Found APIC/MADT table %s @ %" PRIx64 ", length 0x%zx",
				table->name,
				table->addr,
				table->length);
			if (first_madt_table == NULL)
				first_madt_table = table;
			else {
				if ((first_madt_table->length == table->length) &&
				    (memcmp(first_madt_table->data, table->data, table->length) == 0))
					fwts_log_info(fw, "  (and matches the first APIC/MADT table).");
				else
					fwts_log_info(fw, "  (and differs from first APIC/MADT table).");
			}
			count++;
		}
	}

	if (count > 1) {
		char buffer[32];
		fwts_failed(fw, LOG_LEVEL_HIGH, "MultipleAPICMADT",
			"Found %d APIC/MADT tables, the kernel expects just one.",
			count);
		snprintf(buffer, sizeof(buffer), " (or up to %d)", count);
		fwts_log_advice(fw,
			"If you find any APIC issues, perhaps try using "
			"acpi_apic_instance=2%s.",
			count > 2 ? buffer : "");
	} else
		fwts_passed(fw, "Found %d APIC/MADT table(s), as expected.", count);

	return FWTS_OK;
}

static fwts_framework_minor_test apicinstance_tests[] = {
	{ apicinstance_test1, "Test for single instance of APIC/MADT table." },
	{ NULL, NULL }
};

static fwts_framework_ops apicinstance_ops = {
	.description = "Test for single instance of APIC/MADT table.",
	.minor_tests = apicinstance_tests
};

FWTS_REGISTER("apicinstance", &apicinstance_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
