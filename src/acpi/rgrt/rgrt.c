/*
 * Copyright (C) 2022-2026 Canonical
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
acpi_table_init(RGRT, &table)

static void rgrt_data_hexdump(fwts_framework *fw, uint8_t *data, size_t size)
{
	size_t i;

	for (i = 0; i < size; i += 16) {
		char buffer[128];
		size_t left = size - i;

		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw,  "    %s", buffer + 2);
	}
}

static int rgrt_test1(fwts_framework *fw)
{
	bool passed = true;
	fwts_acpi_table_rgrt *rgrt = (fwts_acpi_table_rgrt *)table->data;

	if (!fwts_acpi_table_length(fw, "RGRT", table->length, sizeof(fwts_acpi_table_rgrt))) {
		passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "RGRT Regulatory Graphics Resource Table:");
	fwts_log_info_simp_int(fw, "  Version:        ", rgrt->version);
	fwts_log_info_simp_int(fw, "  Image Type:     ", rgrt->image_type);
	fwts_log_info_simp_int(fw, "  Reserved:       ", rgrt->reserved);
	fwts_log_info_verbatim(fw, "  Image:");
	rgrt_data_hexdump(fw, rgrt->image, table->length - sizeof(fwts_acpi_table_rgrt));

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "RGRT", "Version", rgrt->version, 1, &passed);
	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "RGRT", "Image Type", rgrt->image_type, 1, &passed);
	fwts_acpi_reserved_zero("RGRT", "Reserved", rgrt->reserved, &passed);

	fwts_log_nl(fw);
	if (passed)
		fwts_passed(fw, "No issues found in RGRT table.");

done:
	return FWTS_OK;
}

static fwts_framework_minor_test rgrt_tests[] = {
	{ rgrt_test1, "Validate RGRT table." },
	{ NULL, NULL }
};

static fwts_framework_ops rgrt_ops = {
	.description = "RGRT Regulatory Graphics Resource Table test",
	.init        = RGRT_init,
	.minor_tests = rgrt_tests
};

FWTS_REGISTER("rgrt", &rgrt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
