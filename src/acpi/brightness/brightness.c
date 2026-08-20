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
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#include "brightness-helper.h"

static bool skip_tests = false;

static int brightness_test1(fwts_framework *fw)
{
	struct dirent *entry;
	int max_brightness;
	int saved_brightness;
	DIR *brightness_dir = brightness_get_dir();

	if (skip_tests)	{
		fwts_skipped(fw, "Test skipped because previous test failed.");
		return FWTS_SKIP;
	}

	rewinddir(brightness_dir);
	do {
		int ch;

		entry = readdir(brightness_dir);
		if (entry == NULL || entry->d_name[0] == '.')
			continue;

		if (brightness_get_setting(entry->d_name, "max_brightness", &max_brightness) != FWTS_OK) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessNotExist",
				"Maximum brightness could not be accessed for %s.",
				entry->d_name);
			continue;
		}
		if (max_brightness <= 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessMaxTest1",
				"Maximum brightness for %s is %d and should be > 0.",
				entry->d_name, max_brightness);
			continue;
		}
		if (brightness_get_setting(entry->d_name, "actual_brightness", &saved_brightness) != FWTS_OK) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessMaxTest1",
				"Failed to get current brightness setting for %s.",
				entry->d_name);
			continue;
		}

		fwts_printf(fw, "==== Setting backlight to a low level ====\n");
		/*
		 * Setting it to zero on some machines turns
		 * off the backlight, so set to a low value instead
		 */
		brightness_set_setting(entry->d_name, "brightness", 1);
		ch = fwts_get_reply(fw, "==== Is the Backlight now set to a dim level? [Y/N]: ", "ynYN");
		if (ch == 'y' || ch == 'Y')
			fwts_passed(fw, "Backlight %s set to dim level.", entry->d_name);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "BrightnessDimLevel", "Backlight %s was NOT set to dim level.", entry->d_name);

		fwts_printf(fw, "==== Setting backlight to highest level ====\n");
		brightness_set_setting(entry->d_name, "brightness", max_brightness);
		ch = fwts_get_reply(fw, "==== Is the Backlight now set to a bright level? [Y/N]: ", "ynYN");
		if (ch == 'y' || ch == 'Y')
			fwts_passed(fw, "Backlight %s set to bright level.", entry->d_name);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "BrightnessBrightLevel", "Backlight %s was NOT set to bright level.", entry->d_name);

		/* Restore */
		brightness_set_setting(entry->d_name, "brightness", saved_brightness);
	} while (entry);

	return FWTS_OK;
}

static int brightness_test2(fwts_framework *fw)
{
	struct dirent *entry;
	int max_brightness;
	int saved_brightness;
	DIR *brightness_dir = brightness_get_dir();

	if (skip_tests)	{
		fwts_skipped(fw, "Test skipped because previous test failed.");
		return FWTS_SKIP;
	}

	rewinddir(brightness_dir);
	do {
		entry = readdir(brightness_dir);
		if (entry == NULL || entry->d_name[0] == '.')
			continue;

		if (brightness_get_setting(entry->d_name, "max_brightness", &max_brightness) != FWTS_OK) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessNotExist",
				"Maximum brightness could not be accessed for %s.",
				entry->d_name);
			continue;
		}
		if (max_brightness <= 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessMaxTest2",
				"Maximum brightness for %s is %d and should be > 0.",
				entry->d_name, max_brightness);
			continue;
		}

		fwts_printf(fw, "==== Backlight will now slowly transition from dim to bright ====\n");
		if (brightness_get_setting(entry->d_name, "actual_brightness", &saved_brightness) == FWTS_OK) {
			long delay = 5000000 / max_brightness;
			int ch, i;

			if (delay > 1000000)
				delay = 1000000;

			for (i = 0; i <= max_brightness; i++) {
				fwts_printf(fw, "Setting to brightness level %d\r", i);
				brightness_set_setting(entry->d_name, "brightness", i);
				usleep(delay);
			}

			ch = fwts_get_reply(fw, "==== Did the backlight go from dim to bright? [Y/N]: ", "ynYN");
			if (ch == 'y' || ch == 'Y')
				fwts_passed(fw, "Backlight %s was observed going from dim to bright.", entry->d_name);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "BrightnessNoDimming", "Backlight %s was NOT observed going from dim to bright.", entry->d_name);

			/* Restore */
			brightness_set_setting(entry->d_name, "brightness", saved_brightness);
		}
	} while (entry);

	return FWTS_OK;
}

static int brightness_wait_event(fwts_framework *fw)
{
	int fd;
	int events = 0;
	size_t len;
	int i;

	if ((fd = fwts_acpi_event_open()) < 0) {
		fwts_log_error(fw, "Cannot connect to acpid.");
		return FWTS_ERROR;
	}

	for (i = 0; i <= 20; i++) {
		char *buffer;

		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL)
			if (strstr(buffer, "video")) {
				free(buffer);
				events++;
				break;
			}

		fwts_printf(fw, "Waiting %2.2d/20\r", 20 - i);
	}
	fwts_acpi_event_close(fd);

	return events;
}

static int brightness_test3(fwts_framework *fw)
{
	struct dirent *entry;
	int saved_brightness;
	DIR *brightness_dir = brightness_get_dir();

	rewinddir(brightness_dir);
	do {
		int tmp;

		entry = readdir(brightness_dir);
		if (entry == NULL || entry->d_name[0] == '.')
			continue;

		if (brightness_get_setting(entry->d_name, "actual_brightness", &saved_brightness) != FWTS_OK) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessNotExist",
				"Actual brightness could not be accessed for %s.",
				entry->d_name);
			continue;
		}

		brightness_set_setting(entry->d_name, "brightness", 1);
		fwts_printf(fw, "==== Press the brightness UP hotkey for %s ====\n", entry->d_name);

		if (brightness_wait_event(fw) == 0)
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessNoHotKey",
				"Did not detect ACPI hotkey event.");
		else {
			int new_brightness;

			if (brightness_get_setting(entry->d_name, "actual_brightness", &new_brightness) != FWTS_OK) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"BrightnessNotExist",
					"Actual brightness could not be accessed for %s.",
					entry->d_name);
			} else {
				if (new_brightness > 0)
					fwts_passed(fw, "Brightness increased on UP hotkey for %s.", entry->d_name);
				else
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"BrightnessNoIncrease",
						"Did not see brightness increased for %s.",
						entry->d_name);
			}
		}

		tmp = (saved_brightness > 0) ? saved_brightness : 1;
		brightness_set_setting(entry->d_name, "brightness", tmp);
		fwts_printf(fw, "==== Press the brightness DOWN hotkey for %s ====\n", entry->d_name);

		if (brightness_wait_event(fw) == 0)
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"BrightnessNoHotKey",
				"Did not detect ACPI hotkey event.");
		else {
			int new_brightness;

			if (brightness_get_setting(entry->d_name, "actual_brightness", &new_brightness) != FWTS_OK) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"BrightnessNotExist",
					"Actual brightness could not be accessed for %s.",
					entry->d_name);
			} else {
				if (new_brightness < tmp)
					fwts_passed(fw, "Brightness decreased on DOWN hotkey for %s.", entry->d_name);
				else
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"BrightnessNoDecrease",
						"Did not see brightness decrease for %s.",
						entry->d_name);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static fwts_framework_minor_test brightness_tests[] = {
	{ brightness_test1, "Observe all brightness changes." },
	{ brightness_test2, "Observe min, max brightness changes." },
	{ brightness_test3, "Test brightness hotkeys." },
	{ NULL, NULL }
};

static fwts_framework_ops brightness_ops = {
	.description = "Interactive LCD brightness test.",
	.init        = brightness_init,
	.deinit      = brightness_deinit,
	.minor_tests = brightness_tests
};

FWTS_REGISTER("brightness", &brightness_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_INTERACTIVE)

#endif
