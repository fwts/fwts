/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2025 Canonical
 *
 * This file was originally part of the Linux-ready Firmware Developer Kit
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

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define UNDEFINED	(-1)
#define NOT_EDGE	(0)
#define EDGE		(1)

/*
 * This test sanity checks apic irq information
 * rule of thumb:
 *   interrupts <16 should be EDGE or LEVEL
 *   interrupts >16 should be LEVEL
 *   acpi interrupt should be LEVEL
 */

static int apicedge_test1(fwts_framework *fw)
{
	FILE *file;

	if ((file = fopen("/proc/interrupts", "r")) == NULL) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"NoProcInterrupts",
			"Could not open file /proc/interrupts.");
		return FWTS_ERROR;
	}

	while (!feof(file)) {
		char line[8192], *c;
		int edge = UNDEFINED;
		int irq = 0;

		memset(line, 0, sizeof(line));
		if (fgets(line, sizeof(line) - 1, file) == NULL)
			break;

		if (! (line[0] == ' ' || (line[0] >= '0' && line[0] <= '9')) )
			continue;

		irq = strtoul(line, &c, 10);
		if (c == line)
			continue;
		if (strstr(line, "IO-APIC") && strstr(line, "edge"))
			edge = EDGE;
		else if (strstr(line, "IO-APIC") && strstr(line, "fasteoi"))
			edge = NOT_EDGE;
		else if (strstr(line, "PCI-MSI") && strstr(line, "level"))
			edge = NOT_EDGE;
		else if (strstr(line, "PCI-MSI") && strstr(line, "edge"))
			edge = EDGE;
		else if (strstr(line, "IO-APIC") && strstr(line, "level"))
			edge = NOT_EDGE;

		if (strstr(line,"acpi")) {
			if (edge == EDGE)
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"ACPIIRQEdgeTrig",
					"ACPI Interrupt is incorrectly edge triggered.");
			continue;
		}

		if ((irq < 15) && (edge == UNDEFINED))
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"NonLegacyIRQLevelTrig",
				"Non-Legacy interrupt %i is incorrectly undefined.", irq);
	}
	(void)fclose(file);

	if (fwts_tests_passed(fw))
		fwts_passed(fw, "Legacy interrupts are edge and PCI interrupts are level triggered.");

	return FWTS_OK;
}

static fwts_framework_minor_test apicedge_tests[] = {
	{ apicedge_test1, "Legacy and PCI Interrupt Edge/Level trigger tests." },
	{ NULL, NULL }
};

static fwts_framework_ops apicedge_ops = {
	.description = "APIC edge/level test.",
	.minor_tests = apicedge_tests
};

FWTS_REGISTER("apicedge", &apicedge_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)

#endif
