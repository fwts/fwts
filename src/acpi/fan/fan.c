/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
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

#define COOLING_DEV	"cooling_device"
#define THERMAL_PATH	"/sys/class/thermal"

typedef struct {
	char *name;
	char *type;
	int  max_state;
	int  cur_state;
} fan_info;

static void free_fan_info(void *ptr)
{
	fan_info *info = (fan_info*)ptr;

	free(info->name);
	free(info->type);
	free(info);
}

static fwts_list *get_fan_info(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *entry;
	fwts_list *fans;

	if ((fans = fwts_list_new()) == NULL) {
		fwts_log_error(fw, "Out of memory gathing fan information.");
		return NULL;
	}

	if (!(dir = opendir(THERMAL_PATH)))
		return fans;

	do {
		entry = readdir(dir);
		if (entry && strstr(entry->d_name, COOLING_DEV)) {
			char path[PATH_MAX];
			fan_info *info;
			char *str;

			info = calloc(1, sizeof(fan_info));
			if (info == NULL) {
				fwts_log_error(fw, "Out of memory gathing fan information.");
				fwts_list_free(fans, free_fan_info);
				closedir(dir);
				return NULL;
			}

			if ((info->name = strdup(entry->d_name)) == NULL) {
				fwts_log_error(fw, "Out of memory gathing fan information.");
				fwts_list_free(fans, free_fan_info);
				free(info);
				closedir(dir);
				return NULL;
			}

			snprintf(path, sizeof(path), THERMAL_PATH "/%s/type", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoFanType",
					"Fan present but has no fan type present.");
			} else {
				fwts_chop_newline(str);
			}
			info->type = str;	/* NULL is OK */

			snprintf(path, sizeof(path), THERMAL_PATH "/%s/max_state", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoFanMaxState",
					"Fan present but has no max_state present.");
				info->max_state = -1;
			} else {
				info->max_state = atoi(str);
				free(str);
			}

			snprintf(path, sizeof(path), THERMAL_PATH "/%s/cur_state", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoFanCurState",
					"Fan present but has no cur_state present.");
				info->cur_state = -1;
			} else {
				info->cur_state = atoi(str);
				free(str);
			}

			fwts_list_append(fans, info);
		}
	} while (entry);

	closedir(dir);

	return fans;
}

static int fan_test1(fwts_framework *fw)
{
	fwts_list *fans;
	fwts_list_link *item;

	fwts_log_info(fw,
		"Test how many fans there are in the system. "
		"Check for the current status of the fan(s).");

	if ((fans = get_fan_info(fw)) == NULL)
		return FWTS_ERROR;

	if (fwts_list_len(fans) == 0) {
		fwts_skipped(fw, "No thermal cooling information present: cannot test.");
		fwts_list_free(fans, free_fan_info);
		return FWTS_SKIP;
	}

	fwts_list_foreach(item, fans) {
		fan_info *info = fwts_list_data(fan_info *, item);
		if (info->type == NULL)
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoFanType",
				"Fan %s present but has no fan type present.",
				info->name);
		if (info->max_state == -1)
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoFanMaxState",
				"Fan %s present but has no max_state present.",
				info->name);
		if (info->type) {
			if ((info->cur_state == -1) &&
			    strcmp(info->type, "intel_powerclamp"))
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoFanCurState",
					"Fan %s present but has no cur_state present.",
					info->name);

			if ((info->max_state >= 0) && (info->cur_state >= 0))
				fwts_passed(fw, "Fan %s of type %s has max cooling "
					"state %d and current cooling state %d.",
					info->name, info->type, info->max_state,
					info->cur_state);
		}
	}

	fwts_list_free(fans, free_fan_info);

	return FWTS_OK;
}

static int fan_test2(fwts_framework *fw)
{
	fwts_list *fans1, *fans2;
	fwts_list_link *item1, *item2;
	bool changed = false;
	int i;

	fwts_log_info(fw,
		"Test how many fans there are in the system. "
		"Check for the current status of the fan(s).");

	if ((fans1 = get_fan_info(fw)) == NULL)
		return FWTS_ERROR;

	if (fwts_list_len(fans1) == 0) {
		fwts_skipped(fw, "No thermal cooling information present: cannot test.");
		fwts_list_free(fans1, free_fan_info);
		return FWTS_SKIP;
	}

	fwts_log_info(fw, "Loading CPUs for 20 seconds to try and get fan speeds to change.");
	for (i = 0; i < 20; i++) {
		fwts_progress(fw, (1+i) * 5);
		fwts_cpu_consume(1);
	}
	fwts_progress(fw, 100);

	if ((fans2 = get_fan_info(fw)) == NULL) {
		fwts_list_free(fans1, free_fan_info);
		return FWTS_ERROR;
	}

	if (fwts_list_len(fans2) == 0) {
		fwts_skipped(fw, "No thermal cooling information present: cannot test.");
		fwts_list_free(fans1, free_fan_info);
		fwts_list_free(fans2, free_fan_info);
		return FWTS_SKIP;
	}

	fwts_list_foreach(item1, fans1) {
		fan_info *info1 = fwts_list_data(fan_info *, item1);

		if (!info1->type)
			continue;

		fwts_list_foreach(item2, fans2) {
			fan_info *info2 = fwts_list_data(fan_info *, item2);

			if (!info2->type)
				continue;

			if (strcmp(info1->type, "Processor") == 0) {
				if (strcmp(info1->name, info2->name) == 0) {
					if (info2->cur_state < info1->cur_state) {
						fwts_failed(fw, LOG_LEVEL_HIGH, "FanCurStateDecreased",
							"Fan %s current state decreased from %d to %d "
							"while CPUs were busy, expected an increase.",
							info1->name, info1->cur_state, info2->cur_state);
						changed = true;
					}
					if (info2->cur_state > info1->cur_state) {
						fwts_passed(fw, "Fan %s current state increased from %d to %d "
							"while CPUs were busy.",
							info1->name, info1->cur_state, info2->cur_state);
						changed = true;
					}
					if (info2->cur_state == info1->cur_state)
						fwts_log_info(fw, "Fan %s current state did not change from value %d "
							"while CPUs were busy.",
							info1->name, info1->cur_state);
				}
			}
		}
	}

	if (!changed) {
		fwts_advice(fw,
			"Did not detect any change in the CPU related thermal cooling device states. "
			"It could be that the devices are returning static information back "
			"to the driver and/or the fan speed is automatically being "
			"controlled by firmware using System Management Mode in which case "
			"the kernel interfaces being examined may not work anyway.");


	}

	fwts_list_free(fans1, free_fan_info);
	fwts_list_free(fans2, free_fan_info);
	return FWTS_OK;
}

static fwts_framework_minor_test fan_tests[] = {
	{ fan_test1, "Test fan status." },
	{ fan_test2, "Load system, check CPU fan status." },
	{ NULL, NULL}
};

static fwts_framework_ops fan_ops = {
	.description = "Simple fan tests.",
	.minor_tests = fan_tests
};

FWTS_REGISTER("fan", &fan_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)

#endif
