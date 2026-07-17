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

#define BRSI_RSDP_REVISION	2

static fwts_acpi_table_info *table;

static int rsdp_brsi_init(fwts_framework *fw)
{
	if (fwts_acpi_find_table(fw, "RSDP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI tables.");
		return FWTS_ERROR;
	}

	if (!table) {
		fwts_log_error(fw,
			"ACPI RSDP is required for the "
			"%s target architecture.",
			fwts_arch_get_name(fw->target_arch));
		return FWTS_ERROR;
	}

	/* We know there is an RSDP now, so do a quick sanity check */
	if (table->length == 0) {
		fwts_log_error(fw,
			"ACPI RSDP table has zero length");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}


static int rsdp_brsi_test1(fwts_framework *fw)
{
	fwts_acpi_table_rsdp *rsdp = (fwts_acpi_table_rsdp *)table->data;

	if (rsdp->revision != BRSI_RSDP_REVISION) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "RSDPRevisionTooOld",
			"RSDP: revision is %" PRIu8 ", expected "
			"value to be 2.", rsdp->revision);
		return FWTS_OK;
	}

	fwts_log_info(fw,
		"RSDP revision is %" PRIu8 ".", rsdp->revision);

	fwts_log_info(fw,
		"RSDP RSDT is %" PRIu32 ".", rsdp->rsdt_address);

	fwts_log_info(fw,
		"RSDP XSDT is %" PRIu64 ".", rsdp->xsdt_address);

	if ((rsdp->rsdt_address == 0) && (rsdp->xsdt_address != 0))
		fwts_passed(fw,
			"RSDP: ACPI implements a clean 64-bit "
			"(implemented XSDT, but not RSDT).");
	else
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_010",
			"RSDP: ACPI is not implemented in a clean "
			"64-bit (implemented XSDT, but not RSDT).");

	return FWTS_OK;
}

static fwts_framework_minor_test rsdp_brsi_tests[] = {
	{ rsdp_brsi_test1, "RSDP Root System Description Pointer test." },
	{ NULL, NULL }
};

static fwts_framework_ops rsdp_brsi_ops = {
	.description = "RISC-V BRS-I RSDP Root System Description Pointer tests.",
	.init        = rsdp_brsi_init,
	.minor_tests = rsdp_brsi_tests
};

FWTS_REGISTER("rsdp_brsi", &rsdp_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
