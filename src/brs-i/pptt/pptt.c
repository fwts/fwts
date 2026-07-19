/*
 * Copyright (C) 2026 Xiang W <wangxiang@iscas.ac.cn>
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

#if defined(FWTS_HAS_ACPI) && defined(FWTS_ARCH_RISCV)

static fwts_acpi_table_info *table;

static int pptt_brsi_init(fwts_framework *fw)
{
	int rc;
	rc = acpi_table_generic_init(fw, "PPTT", &table);
	if (table == NULL || table->length == 0)
		return FWTS_OK;
	return rc;
}

static int pptt_brsi_test1(fwts_framework *fw)
{
	if (table == NULL || table->length == 0)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_030",
			"The Processor Properties Table (PPTT) MUST be implemented "
			"on RISC-V BRS systems, even on systems with simple hart topology "
			"(per ACPI_030).");
	else
		fwts_passed(fw, "The Processor Properties Table (PPTT) has been implemented "
			"on RISC-V BRS systems.");

	return FWTS_OK;

}

static fwts_framework_minor_test pptt_brsi_tests[] = {
	{ pptt_brsi_test1, "Check if PPTT table exists." },
	{ NULL, NULL }
};

static fwts_framework_ops pptt_brsi_ops = {
	.description = "RISC-V BRS-I PPTT Processor Properties Topology Table test.",
	.init        = pptt_brsi_init,
	.minor_tests = pptt_brsi_tests
};

FWTS_REGISTER("pptt_brsi", &pptt_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
