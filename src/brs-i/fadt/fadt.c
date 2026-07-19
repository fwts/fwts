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

#if defined(FWTS_HAS_ACPI) && (FWTS_ARCH_RISCV)

static const fwts_acpi_table_fadt *fadt;
static const fwts_acpi_table_facs *facs;
static int fadt_size;

static int fadt_brsi_init(fwts_framework *fw)
{
	fwts_acpi_table_info *table;

	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table FACP.");
		return FWTS_ERROR;
	}
	if (table == NULL) {
		fwts_log_error(fw, "ACPI table FACP does not exist!");
		return FWTS_ERROR;
	}
	fadt = (const fwts_acpi_table_fadt *)table->data;
	fadt_size = table->length;

	/*  Not having a FADT is a failure on RISC-V BRS-I Architecture */
	if (fadt_size == 0) {
		fwts_log_error(fw, "ACPI table FACP has zero length!");
		return FWTS_ERROR;
	}

	if (fwts_acpi_find_table(fw, "FACS", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table FACS.");
		return FWTS_ERROR;
	}
	if (table)
		facs = (const fwts_acpi_table_facs*)table->data;

	return FWTS_OK;
}

#define BRSI_VERSION(major, minor)	 ((((uint16_t)(major)) << 8) | (minor))

static int fadt_brsi_revision(fwts_framework *fw)
{
	const uint8_t BRSI_LEAST_MAJOR = 6;
	const uint8_t BRSI_LEAST_MINOR = 6;
	uint8_t major = fadt->header.revision;
	uint8_t minor = 0;

	fwts_get_fadt_version(fw, &major, &minor);

	fwts_log_info(fw, "FADT revision: %" PRIu8 ".%" PRIu8, major, minor);

	if (BRSI_VERSION(major, minor) >=
	    BRSI_VERSION(BRSI_LEAST_MAJOR, BRSI_LEAST_MINOR))
		fwts_passed(fw, "FADT revision is up to date.");
	else {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "fadt_revision:",
			"FADT revision is outdated, at least revision is %" PRIu8 ".%" PRIu8,
			BRSI_LEAST_MAJOR, BRSI_LEAST_MINOR);
	}

	return FWTS_OK;
}

static int fadt_brsi_reduced_hw(fwts_framework *fw)
{
	fwts_bool rhw = fwts_acpi_is_reduced_hardware(fw);

	if (rhw == FWTS_TRUE) {
		if (!facs)
			fwts_passed(fw, "Implement the hardware-reduced ACPI mode (no FACS table).");
		else
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_020",
				"RISC-V BRS-I must not implement the ACPI FACS table");
	} else if (rhw == FWTS_FALSE)
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "ACPI_020",
			"FADT indicates ACPI is not in reduced hardware mode.");
	else
		fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_020",
			"ACPI table reads error.");

	return FWTS_OK;
}


static fwts_framework_minor_test fadt_brsi_tests[] = {
	{ fadt_brsi_revision, "FADT Revision Test." },
	{ fadt_brsi_reduced_hw, "FADT Reduced HW Test." },
	{ NULL, NULL }
};

static fwts_framework_ops fadt_brsi_ops = {
	.description = "RISC-V BRS-I FADT Fixed ACPI Description Table tests.",
	.init        = fadt_brsi_init,
	.minor_tests = fadt_brsi_tests
};

FWTS_REGISTER("fadt_brsi", &fadt_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
