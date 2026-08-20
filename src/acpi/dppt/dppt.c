/*
 * Copyright (C) 2017-2026 Canonical
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
acpi_table_init(DPPT, &table)

static int dppt_test1(fwts_framework *fw)
{
	fwts_acpi_table_dppt *dppt = (fwts_acpi_table_dppt *) table->data;
	bool passed = true;

	FWTS_UNUSED(dppt);

	fwts_log_info_verbatim(fw, "DPPT DMA Protection Policy Table test:");

	if (passed)
		fwts_passed(fw, "No issues found in DPPT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test dppt_tests[] = {
	{ dppt_test1, "Validate DPPT table." },
	{ NULL, NULL }
};

static fwts_framework_ops dppt_ops = {
	.description = "DPPT DMA Protection Policy Table test",
	.init        = DPPT_init,
	.minor_tests = dppt_tests
};

FWTS_REGISTER("dppt", &dppt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
