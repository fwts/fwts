/*
 * Copyright (C) 2021-2026 Canonical
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
#include <fcntl.h>

#define ACPI_SCI_PATH	"/sys/firmware/acpi/interrupts/sci"
#define ACPI_SCI_THRESHOLD	30
#define ACPI_SCI_PERIOD		3

static int interrupt_init(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	return FWTS_OK;
}

static int interrupt_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	return FWTS_OK;
}

static uint32_t get_acpi_sci(void)
{
	char *str;
	uint32_t val;

	str = fwts_get(ACPI_SCI_PATH);
	if (!str)
		return 0;

	val = atoi(str);
	free(str);

	return val;
}

static int interrupt_test0(fwts_framework *fw)
{
	uint32_t sci_init, average;
	bool passed = true;

	if (access(ACPI_SCI_PATH, R_OK)) {
		fwts_log_warning(fw, "Could not open ACPI SCI data\n");
		return FWTS_ERROR;
	}

	sci_init = get_acpi_sci();
	sleep(ACPI_SCI_PERIOD);
	average = (get_acpi_sci() - sci_init) / ACPI_SCI_PERIOD;

	fwts_log_info(fw, "ACPI SCIs per second are %" PRId32 ".", average);

	if (average > ACPI_SCI_THRESHOLD) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH, "InterruptSCITooMany",
			"ACPI SCIs per second are above threshold %" PRId32 ".", ACPI_SCI_THRESHOLD);
	}

	if (passed)
		fwts_passed(fw, "No issues found in ACPI SCI generation.");

	return FWTS_OK;
}

static fwts_framework_minor_test interrupt_tests[] = {
	{ interrupt_test0, "Check whether ACPI SCI is triggered excessively." },
	{ NULL, NULL }
};

static fwts_framework_ops interrupt_ops = {
	.description = "Interrupt tests.",
	.init        = interrupt_init,
	.deinit      = interrupt_deinit,
	.minor_tests = interrupt_tests
};

FWTS_REGISTER("interrupt", &interrupt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
