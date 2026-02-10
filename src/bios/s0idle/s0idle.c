/*
 * Copyright (C) 2020-2026 Canonical
 *
 * Portions of this code original from the Linux-ready Firmware Developer Kit
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include "fwts_acpi_object_eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>


static fwts_acpi_table_info *table;

static int s0idle_init(fwts_framework *fw)
{
	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table FACP.");
		return FWTS_ERROR;
	}
	if (table == NULL) {
		fwts_log_error(fw, "ACPI table FACP does not exist!");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int s0idle_test1(fwts_framework *fw)
{
	const fwts_acpi_table_fadt *fadt = (const fwts_acpi_table_fadt *)table->data;

	if (fadt->flags & FWTS_FACP_FLAG_LOW_POWER_S0_IDLE_CAPABLE) {
		fwts_passed(fw, "LOW_POWER_S0_IDLE_CAPABLE is set in ACPI FADT.");
	} else {
		fwts_warning(fw,
			"LOW_POWER_S0_IDLE_CAPABLE is not set in ACPI FADT, "
			"indicating hardware may not support S0 Low Power Idle.");
	}

	return FWTS_OK;
}

static fwts_framework_minor_test s0idle_tests[] = {
	{ s0idle_test1, "Test ACPI FADT S0 idle capable." },
	{ NULL, NULL }
};

static fwts_framework_ops s0idle = {
	.description = "S0IDLE FADT Low Power S0 Idle tests.",
	.init        = s0idle_init,
	.minor_tests = s0idle_tests
};

FWTS_REGISTER("s0idle", &s0idle, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)
#endif
