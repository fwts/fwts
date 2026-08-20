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
#include <unistd.h>
#include <string.h>

static int ac_adapter_init(fwts_framework *fw)
{
	int matching = 0;
	int not_matching = 0;

	if (fwts_ac_adapter_get_state(FWTS_AC_ADAPTER_ANY, &matching, &not_matching) != FWTS_OK) {
		fwts_log_error(fw, "Power Supply does not exist, skipping test");
		return FWTS_SKIP;
	}
	return FWTS_OK;
}

static int ac_adapter_test1(fwts_framework *fw)
{
	int matching = 0;
	int not_matching = 0;

	fwts_ac_adapter_get_state(FWTS_AC_ADAPTER_ANY, &matching, &not_matching);

	if ((matching == 0) || (not_matching > 0))
		fwts_failed(fw, LOG_LEVEL_LOW, "NoACAdapterState",
			"Failed to detect any state in the ac_adapter state info.");
	else
		fwts_passed(fw, "Detected a state in the ac_adapter state info.");

	return FWTS_OK;
}

static int ac_adapter_test2(fwts_framework *fw)
{
	int matching = 0;
	int not_matching = 0;

	fwts_printf(fw, "==== Make sure laptop is connected to the mains power. ====\n");
	fwts_press_enter(fw);

	fwts_ac_adapter_get_state(FWTS_AC_ADAPTER_ONLINE, &matching, &not_matching);

	if ((matching == 0) || (not_matching > 0))
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoACAdapterOnLine",
			"Failed to detect an ac_adapter on-line state.");
	else
		fwts_passed(fw, "Detected ac_adapter on-line state.");

	return FWTS_OK;
}

static int ac_adapter_test3(fwts_framework *fw)
{
	int fd;
	size_t len;
	char *buffer;
	int matching;
	int not_matching;
	int events;
	int i;

	if ((fd = fwts_acpi_event_open()) < 0) {
		fwts_log_error(fw, "Cannot connect to acpid.");
		return FWTS_ERROR;
	}

	events = 0;
	matching = 0;
	not_matching = 0;

	fwts_printf(fw, "==== Please unplug the laptop power. ====\n");

	for (i = 0; i < 20; i++) {
		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL) {
			if (strstr(buffer, "ac_adapter")) {
				events++;
				fwts_ac_adapter_get_state(FWTS_AC_ADAPTER_OFFLINE, &matching, &not_matching);
				break;
			}
			free(buffer);
		}
		fwts_printf(fw, "Waiting %2.2d/20\r", 20-i);
	}
	if (events == 0)
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoACAdapterEventsOff",
			"Did not detect any ACPI ac-adapter events while waiting for power to be disconnected.");
	else
		if ((matching == 0) || (not_matching > 0))
			fwts_failed(fw, LOG_LEVEL_HIGH, "NoACAdapterOffline",
				"Could not detect ac_adapter off-line state.");
		else
			fwts_passed(fw, "Detected ac_adapter off-line state.");

	events = 0;
	matching = 0;
	not_matching = 0;

	fwts_printf(fw, "==== Please re-connect the laptop power. ====\n");

	for (i = 0; i < 20; i++) {
		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL) {
			events++;
			if (strstr(buffer, "ac_adapter")) {
				events++;
				fwts_ac_adapter_get_state(FWTS_AC_ADAPTER_ONLINE, &matching, &not_matching);
				break;
			}
			free(buffer);
		}
		fwts_printf(fw, "Waiting %2.2d/20\r", 20-i);
	}
	if (events == 0)
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoACAdapterEventsOn",
			"Did not detect any ACPI ac-adapter events while waiting for power to be re-connected.");
	else
		if ((matching == 0) || (not_matching > 0))
			fwts_failed(fw, LOG_LEVEL_HIGH, "NoACAdapterOnline",
				"Could not detect ac_adapter on-line state.");
		else
			fwts_passed(fw, "Detected ac_adapter on-line state.");

	fwts_acpi_event_close(fd);

	return FWTS_OK;
}

static fwts_framework_minor_test ac_adapter_tests[] = {
	{ ac_adapter_test1, "Test ACPI ac_adapter state." },
	{ ac_adapter_test2, "Test ac_adapter initial on-line state." },
	{ ac_adapter_test3, "Test ac_adapter state changes." },
	{ NULL, NULL }
};

static fwts_framework_ops ac_adapter_ops = {
	.description = "Interactive ac_adapter power test.",
	.init        = ac_adapter_init,
	.minor_tests = ac_adapter_tests
};

FWTS_REGISTER("ac_adapter", &ac_adapter_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_INTERACTIVE)

#endif
