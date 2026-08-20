/*
 * Copyright (C) 2023-2026 Canonical
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
acpi_table_init(RAS2, &table)

static int ras2_test1(fwts_framework *fw)
{
	fwts_acpi_table_ras2 *ras2 = (fwts_acpi_table_ras2 *)table->data;
	bool passed = true;
	uint32_t offset;

	if (!fwts_acpi_table_length(fw, "RAS2", table->length, sizeof(fwts_acpi_table_ivrs)))
		return FWTS_OK;

	fwts_log_info_verbatim(fw, "RAS2 (ACPI RAS2 Feature) Table:");
	offset = sizeof(fwts_acpi_table_ras2);
	fwts_log_info_simp_int(fw, "  Reserved:                  ", ras2->reserved);
	fwts_log_info_simp_int(fw, "  Number of PCC descriptors: ", ras2->num_pcc_descriptors);

	fwts_acpi_reserved_zero("RAS2", "Reserved", ras2->reserved, &passed);

	for (uint32_t i = 0; i < ras2->num_pcc_descriptors; i++) {
		if ((offset + sizeof(fwts_acpi_ras2_pcc_desc)) > table->length) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "RAS2TooShort",
				"RAS2 table too short, PCC descriptor list exceeds the table.");
			passed = false;
			return FWTS_OK;
		}
		fwts_acpi_ras2_pcc_desc *pcc_desc = (fwts_acpi_ras2_pcc_desc *)(table->data + offset);
		fwts_log_info_verbatim(fw, "  PCC Descriptor List:");
		fwts_log_info_simp_int(fw, "    PCC Identifier:          ", pcc_desc->pcc_id);
		fwts_log_info_simp_int(fw, "    Reserved:                ", pcc_desc->reserved);
		fwts_log_info_simp_int(fw, "    Feature Type:            ", pcc_desc->feature_type);
		fwts_log_info_simp_int(fw, "    Instance:                ", pcc_desc->instance);

		fwts_acpi_reserved_zero("RAS2", "Reserved", pcc_desc->reserved, &passed);
		if (pcc_desc->feature_type >= 0x01 && pcc_desc->feature_type <= 0x7F) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "RAS2ReservedType",
				"RAS2 RAS feature types 0x01-0x7f is reserved, got 0x%" PRIx8,
				pcc_desc->feature_type);
			passed = false;
		}

		offset += sizeof(fwts_acpi_ras2_pcc_desc);
	}

	if (passed)
		fwts_passed(fw, "No issues found in RAS2 table.");

	return FWTS_OK;
}

static fwts_framework_minor_test ras2_tests[] = {
	{ ras2_test1, "Validate RAS2 table." },
	{ NULL, NULL }
};

static fwts_framework_ops ras2_ops = {
	.description = "ACPI RAS2 Feature Table test",
	.init        = RAS2_init,
	.minor_tests = ras2_tests
};

FWTS_REGISTER("ras2", &ras2_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
