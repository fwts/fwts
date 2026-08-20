/*
 * Copyright (C) 2010-2026 Canonical
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

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

static int power_button_test1(fwts_framework *fw)
{
	int fd;
	size_t len;
	char *buffer;
	int matching;
	int i;

	fwts_printf(fw, "==== Please press the laptop power button. ====\n");

	if ((fd = fwts_acpi_event_open()) < 0) {
		fwts_log_error(fw, "Cannot connect to acpid.");
		return FWTS_ERROR;
	}

	matching = 0;

	for (i = 0; i <= 20; i++) {
		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL) {
			if (strstr(buffer, "button/power")) {
				matching++;
				break;
			}
			free(buffer);
		}
		fwts_printf(fw, "Waiting %2.2d/20\r", 20-i);
	}
	if (matching == 0)
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoPowerButtonEvents",
			"Did not detect any ACPI power buttons events while waiting for power button to be pressed.");
	else  {
		char button[4096];
		memset(button, 0, sizeof(button));
		sscanf(buffer, "%*s %4095s", button);

		fwts_passed(fw, "Detected %s power button event.", button);
	}

	fwts_acpi_event_close(fd);

	return FWTS_OK;
}

static fwts_framework_minor_test power_button_tests[] = {
	{ power_button_test1, "Test press of power button and ACPI event." },
	{ NULL, NULL }
};

static fwts_framework_ops power_button_ops = {
	.description = "Interactive power_button button test.",
	.minor_tests = power_button_tests
};

FWTS_REGISTER("power_button", &power_button_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_INTERACTIVE)

#endif
