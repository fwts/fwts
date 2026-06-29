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
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>

static fwts_acpi_table_info *table;
acpi_table_init(TPM2, &table)

/*
 * TPM2 table
 *   available @ https://trustedcomputinggroup.org/tcg-acpi-specification/
 */
static int tpm2_test1(fwts_framework *fw)
{
	fwts_acpi_table_tpm2 *tpm2 = (fwts_acpi_table_tpm2*) table->data;
	bool passed = true;

	fwts_log_info_verbatim(fw, "TPM2 Table:");
	fwts_log_info_simp_int(fw, "  Platform Class:                  ", tpm2->platform_class);
	fwts_log_info_simp_int(fw, "  Reserved:                        ", tpm2->reserved);
	fwts_log_info_simp_int(fw, "  Address of Control Area:         ", tpm2->address_of_control_area);
	fwts_log_info_simp_int(fw, "  Start Method:                    ", tpm2->start_method);

	if (tpm2->platform_class != 0 && tpm2->platform_class != 1) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"TPM2BadPlatformClass",
			"TPM2's platform class must be zero (client) or one (server), got 0x%" PRIx16,
			tpm2->platform_class);
	}

	fwts_acpi_reserved_zero("TPM2", "Reserved", tpm2->reserved, &passed);

	if (tpm2->start_method < 1 || tpm2->start_method >= 14) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"TPM2BadStartMethod",
			"TPM2's Start Method must be between 0x1 and 0xd, got 0x%" PRIx32,
			tpm2->start_method);
	}

	/* When TPM2 includes fields "LAML" & "LASA", table size will be fixed to 76. */
	if (table->length != 76) {
		if (tpm2->start_method == 2 && table->length != sizeof(fwts_acpi_table_tpm2) + 4) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"TPM2BadPlatformParameters",
				"Table length must be 0x%" PRIx32 " if Start Method equals 2, "
				"got 0x%" PRIx32, (uint32_t) sizeof(fwts_acpi_table_tpm2) + 4,
				(uint32_t) table->length);
		}

		if (tpm2->start_method == 11 && table->length < sizeof(fwts_acpi_table_tpm2) + 12) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"TPM2BadPlatformParameters",
				"Table length must be at least 0x%" PRIx32 " if Start Method equals 11, "
				"got 0x%" PRIx32, (uint32_t) sizeof(fwts_acpi_table_tpm2) + 12,
				(uint32_t) table->length);
		}
	}

	if (passed)
		fwts_passed(fw, "No issues found in TPM2 table.");

	return FWTS_OK;
}

static fwts_framework_minor_test tpm2_tests[] = {
	{ tpm2_test1, "Validate TPM2 table." },
	{ NULL, NULL }
};

static fwts_framework_ops tpm2_ops = {
	.description = "TPM2 Trusted Platform Module 2 test.",
	.init        = TPM2_init,
	.minor_tests = tpm2_tests
};

FWTS_REGISTER("tpm2", &tpm2_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
