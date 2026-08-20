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
#include <ctype.h>

static fwts_acpi_table_info *table;
acpi_table_init(MSDM, &table)

/*
 *  Microsoft Data Management (MSDM) Table
 *	http://feishare.com/attachments/article/265/microsoft-software-licensing-tables.pdf
 *	and derived from other sources.
 */
static int msdm_test1(fwts_framework *fw)
{
	fwts_acpi_table_msdm *msdm = (fwts_acpi_table_msdm *)table->data;
	bool passed = true;

	/* Size sanity check #1, got enough table to get initial header */
	if (!fwts_acpi_table_length(fw, "MSDM", table->length, sizeof(fwts_acpi_table_msdm))) {
		passed = false;
		goto done;
	}
	fwts_log_info_simp_int(fw, "  Reserved:                 ", msdm->reserved);
	fwts_log_info_simp_int(fw, "  Data Type:                ", msdm->data_type);
	fwts_log_info_simp_int(fw, "  Data Reserved:            ", msdm->data_reserved);
	fwts_log_info_simp_int(fw, "  Data Length:              ", msdm->data_length);

	fwts_acpi_reserved_zero("MSDM", "Reserved", msdm->reserved, &passed);
	fwts_acpi_reserved_zero("MSDM", "Data Reserved", msdm->data_reserved, &passed);

	/* Now check table is big enough for the data payload */
	if (table->length < sizeof(fwts_acpi_table_msdm) + msdm->data_length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"MSDMDataLengthInvalid",
			"MSDM Data Length field states that the data "
			" is %" PRIu32 " bytes long, but the table is only "
			"%zu bytes in total",
			msdm->data_length, table->length);
		goto done;
	}

	/*
	 *  Based on inspection of a few hundred tables
	 */
	switch (msdm->data_type) {
	case 0x0001:
		/* Check license key size */
		if (msdm->data_length != 0x1d)
			fwts_acpi_fixed_value(fw, LOG_LEVEL_HIGH, "MSDM", "Data Length", msdm->data_length, 29, &passed);
		else {
			size_t i;
			bool invalid = false;
			/* Note, no checks to see if this is a valid key! */

			fwts_log_info_verbatim(fw, "  Data:                     '%*.*s'",
				msdm->data_length, msdm->data_length, msdm->data);
			/* Expect XXXXX-XXXXX-XXXXX-XXXXX-XXXXX */

			for (i = 0; i < 29; i++) {
				if ((i % 6) == 5) {
					if (msdm->data[i] != '-')
						invalid = true;
				} else {
					if (!isdigit(msdm->data[i]) &&
					    !isupper(msdm->data[i]))
						invalid = true;
				}
			}
			if (invalid) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"MSDMDataLengthInvalid",
					"MSDM Data field did not contain digits, uppercase letters and "
					"- characters in the form XXXXX-XXXXX-XXXXX-XXXXX-XXXXX");
			}
		}
		break;
	default:
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"MSDMDataTypeInvalid",
			"MSDM Data Type field should be 0x00000001, got 0x%8.8" PRIx32
			" instead",
			msdm->data_type);
		break;
	}

done:
	/*
	 *  Until this format is described somewhere, the checks
	 *  will be totally minimal.
	 */
	fwts_log_info(fw, "MSDM has had minimal check due to proprietary nature of the table");
	if (passed)
		fwts_passed(fw, "No issues found in MSDM table.");

	return FWTS_OK;
}

static fwts_framework_minor_test msdm_tests[] = {
	{ msdm_test1, "MSDM Microsoft Data Management Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops msdm_ops = {
	.description = "MSDM Microsoft Data Management Table test.",
	.init        = MSDM_init,
	.minor_tests = msdm_tests
};

FWTS_REGISTER("msdm", &msdm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
