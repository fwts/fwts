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

static int lid_init(fwts_framework *fw)
{
	uint32_t matched, not_matched;

	if (fwts_button_match_state(fw, FWTS_BUTTON_LID_ANY, &matched, &not_matched) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_LOW, "NoLIDPath",
			"No lid interface available: cannot test.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static void lid_check_field_poll(
	fwts_framework *fw,
	const uint32_t button,
	uint32_t *matching,
	uint32_t *not_matching)
{
	int i;

	uint32_t tmp_matching = 0;
	uint32_t tmp_not_matching = 0;

	for (i = 0; i < 100; i++) {
		fwts_button_match_state(fw, button,
			&tmp_matching, &tmp_not_matching);
		usleep(10);

		if (tmp_matching != 0)
			break;
	}
	*matching = tmp_matching;
	*not_matching = tmp_not_matching;
}

static int lid_test1(fwts_framework *fw)
{
	uint32_t matching = 0;
	uint32_t not_matching = 0;

	fwts_printf(fw, "==== Make sure laptop lid is open. ====\n");
	fwts_press_enter(fw);

	lid_check_field_poll(fw, FWTS_BUTTON_LID_OPENED, &matching, &not_matching);

	if ((matching == 0) || (not_matching > 0))
		fwts_failed(fw, LOG_LEVEL_HIGH, "LidNotOpen",
			"Detected a closed LID state.");
	else
		fwts_passed(fw, "Detected open LID state.");

	return FWTS_OK;
}

static int lid_test_state(fwts_framework *fw, int button)
{
	int gpe_count = 0;
	int fd;
	uint32_t matching = 0;
	uint32_t not_matching = 0;
	int events = 0;
	size_t len;
	char *state;
	int i;

	fwts_gpe *gpes_start;
	fwts_gpe *gpes_end;

	switch (button) {
	case FWTS_BUTTON_LID_OPENED:
		state = "open";
		break;
	case FWTS_BUTTON_LID_CLOSED:
		state = "closed";
		break;
	default:
		state = "unknown";
		break;
	}

	if ((gpe_count = fwts_gpe_read(&gpes_start)) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot read GPEs.");
		return FWTS_ERROR;
	}

	if ((fd = fwts_acpi_event_open()) < 0) {
		fwts_log_error(fw, "Cannot connect to acpid.");
		fwts_gpe_free(gpes_start, gpe_count);
		return FWTS_ERROR;
	}

	for (i = 0; i <= 20; i++) {
		char *buffer;

		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL) {
			if (strstr(buffer, "button/lid")) {
				events++;
				lid_check_field_poll(fw, button, &matching, &not_matching);
				break;
			}
			free(buffer);
		}
		fwts_printf(fw, "Waiting %2.2d/20\r", 20-i);
	}
	fwts_acpi_event_close(fd);

	if ((gpe_count = fwts_gpe_read(&gpes_end)) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot read GPEs.");
		fwts_gpe_free(gpes_start, gpe_count);
		return FWTS_ERROR;
	}

	fwts_gpe_test(fw, gpes_start, gpes_end, gpe_count);
	fwts_gpe_free(gpes_start, gpe_count);
	fwts_gpe_free(gpes_end, gpe_count);

	if (events == 0)
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoLidEvents",
			"Did not detect any ACPI LID events while waiting for to LID %s.", state);
	else {
		fwts_passed(fw, "Detected ACPI LID events while waiting for LID to %s.", state);
		if ((matching == 0) || (not_matching > 0))
			fwts_failed(fw, LOG_LEVEL_HIGH, "NoLidState",
				"Could not detect lid %s state.", state);
		else
			fwts_passed(fw, "Detected lid %s state.", state);
	}

	return FWTS_OK;
}


static int lid_test2(fwts_framework *fw)
{
	int ret;

	fwts_printf(fw, "==== Please close laptop lid for 2 seconds and then re-open. ====\n");

	if ((ret = lid_test_state(fw, FWTS_BUTTON_LID_CLOSED)) != FWTS_OK)
		return ret;
	if ((ret = lid_test_state(fw, FWTS_BUTTON_LID_OPENED)) != FWTS_OK)
		return ret;

	return FWTS_OK;
}

static int lid_test3(fwts_framework *fw)
{
	int i;
	fwts_log_info(fw, "Some machines may have EC or ACPI faults that cause detection of multiple open/close events to fail.");

	for (i = 1; i < 4; i++) {
		int ret;

		fwts_printf(fw, "==== %d of %d: Please close laptop lid for 2 seconds and then re-open. ====\n", i,3);

		if ((ret = lid_test_state(fw, FWTS_BUTTON_LID_CLOSED)) != FWTS_OK)
			return ret;
		if ((ret = lid_test_state(fw, FWTS_BUTTON_LID_OPENED)) != FWTS_OK)
			return ret;
	}

	return FWTS_OK;
}

static fwts_framework_minor_test lid_tests[] = {
	{ lid_test1, "Test LID buttons report open correctly." },
	{ lid_test2, "Test LID buttons on a single open/close." },
	{ lid_test3, "Test LID buttons on multiple open/close events." },
	{ NULL, NULL }
};

static fwts_framework_ops lid_ops = {
	.description = "Interactive lid button test.",
	.init        = lid_init,
	.minor_tests = lid_tests
};

FWTS_REGISTER("lid", &lid_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_INTERACTIVE)

#endif
