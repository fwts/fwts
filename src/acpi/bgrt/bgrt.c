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
acpi_table_init(BGRT, &table)

/*
 *  BGRT Boot Graphics Resource Table
 */
static int bgrt_test1(fwts_framework *fw)
{
	fwts_acpi_table_bgrt *bgrt = (fwts_acpi_table_bgrt*)table->data;
	bool passed = true;

	fwts_log_info_verbatim(fw, "BGRT Boot Graphics Resource Table:");
	fwts_log_info_simp_int(fw, "  Version:                  ", bgrt->version);
	fwts_log_info_simp_int(fw, "  Status:                   ", bgrt->status);
	fwts_log_info_simp_int(fw, "  Image Type:               ", bgrt->image_type);
	fwts_log_info_simp_int(fw, "  Image Memory Address:     ", bgrt->image_addr);
	fwts_log_info_simp_int(fw, "  Image Offset X:           ", bgrt->image_offset_x);
	fwts_log_info_simp_int(fw, "  Image Offset Y:           ", bgrt->image_offset_y);

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "BGRT", "Version", bgrt->version, 1, &passed);
	fwts_acpi_reserved_bits("BGRT", "BGRT Status", bgrt->status, 3, 7, &passed);
	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "BGRT", "Image Type", bgrt->image_type, 0, &passed);

	if (passed)
		fwts_passed(fw, "No issues found in BGRT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test bgrt_tests[] = {
	{ bgrt_test1, "BGRT Boot Graphics Resource Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops bgrt_ops = {
	.description = "BGRT Boot Graphics Resource Table test.",
	.init        = BGRT_init,
	.minor_tests = bgrt_tests
};

FWTS_REGISTER("bgrt", &bgrt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
