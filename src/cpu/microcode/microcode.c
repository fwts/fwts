/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This file was derived from part of the Linux-ready Firmware Developer Kit,
 * however, it has been completely re-written.
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

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#define SYS_CPU_PATH		"/sys/devices/system/cpu"
#define UNKNOWN			-1

typedef struct {
	int cpu;		/* CPU # */
	int old_revision;	/* Old microcode revision */
	int new_revision;	/* New microcode revision */
	int year;		/* Year of new revision */
	int month;		/* Month of new revision */
	int day;		/* Day of new revision */
} microcode_info;

static fwts_list *klog;

static int microcode_init(fwts_framework *fw)
{
	bool intel;

	if (fwts_cpu_is_Intel(&intel) != FWTS_OK) {
		fwts_log_error(fw, "Cannot determine processor type.");
		return FWTS_ERROR;
	}

	if (!intel) {
		fwts_log_info(fw, "The microcode test currently only supports Intel processors.");
		return FWTS_SKIP;
	}

	klog = fwts_klog_read();
	if (klog == NULL) {
		fwts_log_error(fw, "Cannot read kernel log.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int microcode_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_klog_free(klog);
	return FWTS_OK;
}

static microcode_info *microcode_find_cpu(
	const int cpu,
	fwts_list *microcode_info_list)
{
	fwts_list_link *item;

	fwts_list_foreach(item, microcode_info_list) {
		microcode_info *info = fwts_list_data(microcode_info *, item);
		if (info->cpu == cpu)
			return info;
	}
	return NULL;
}

static int microcode_test1(fwts_framework *fw)
{
	fwts_list_link *item;
	fwts_list microcode_info_list;
	microcode_info *info;

	DIR *dir;
	struct dirent *entry;
	int failed = 0;
	int passed = 0;

	fwts_list_init(&microcode_info_list);

	fwts_log_info(fw,
		"This test verifies if the firmware has put a recent revision "
		"of the microcode into the processor at boot time. Recent "
		"microcode is important to have all the required "
		"features and errata updates for the processor.");

	/*
	 * Gather data from klog, scan for patterns like:
	 *   microcode: CPU0 sig=0x306a9, pf=0x10, revision=0x12
         *   microcode: CPU0 updated to revision 0x17, date = 2013-01-09
	 */
	fwts_list_foreach(item, klog) {
		char *ptr;
		char *line = fwts_list_data(char *, item);
		line = fwts_klog_remove_timestamp(line);

		if (strstr(line, "microcode:") == NULL)
			continue;

		ptr = strstr(line, "revision=0x");
		if (ptr) {
			int cpu;
			unsigned int revision;

			if (sscanf(line, "%*s CPU%d sig=0x%*x, pf=0x%*x, revision=0x%x", &cpu, &revision) != 2)
				continue;

			if (microcode_find_cpu(cpu, &microcode_info_list) == NULL) {
				info = calloc(1, sizeof(microcode_info));
				if (info == NULL) {
					fwts_log_error(fw, "Cannot allocate memory.");
					fwts_list_free_items(&microcode_info_list, free);
					return FWTS_ERROR;
				}
				info->cpu = cpu;
				info->old_revision = revision;
				info->new_revision = UNKNOWN;
				info->year = UNKNOWN;
				info->month = UNKNOWN;
				info->day = UNKNOWN;
				fwts_list_append(&microcode_info_list, info);
			}
			continue;
		}

		ptr = strstr(line, "updated to revision");
		if (ptr) {
			int cpu;
			unsigned int revision;
			int year;
			int month;
			int day;

			if (sscanf(line, "%*s CPU%d updated to revision 0x%x, date = %d-%d-%d",
				&cpu, &revision, &year, &month, &day) != 5)
				continue;

			info = microcode_find_cpu(cpu, &microcode_info_list);
			if (info == NULL) {
				/*
				 * Strange, we found the update but not the
				 * original revision info, create an entry for
				 * this CPU anyhow, mark original revision as
				 * unknown
				 */
				info = calloc(1, sizeof(microcode_info));
				if (info == NULL) {
					fwts_log_error(fw, "Cannot allocate memory.");
					fwts_list_free_items(&microcode_info_list, free);
					return FWTS_ERROR;
				}
				info->cpu = cpu;
				info->old_revision = UNKNOWN;
				info->new_revision = revision;
				info->year = year;
				info->month = month;
				info->day = day;
				fwts_list_append(&microcode_info_list, info);
			} else {
				/* Exists, so update */
				info->new_revision = revision;
				info->year = year;
				info->month = month;
				info->day = day;
			}
		}
	}

	/*
	 *  Now sanity check all CPUs
	 */
	if ((dir = opendir(SYS_CPU_PATH)) == NULL) {
		fwts_log_error(fw, "Cannot open %s.", SYS_CPU_PATH);
		fwts_list_free_items(&microcode_info_list, free);
		return FWTS_ERROR;
	}

	/* Scan and check */
	while ((entry = readdir(dir)) != NULL) {
	        if ((strlen(entry->d_name) > 3) &&
		    (strncmp(entry->d_name,"cpu", 3) == 0) &&
		    (isdigit(entry->d_name[3]))) {
			char path[PATH_MAX];
			char *data;

	        	snprintf(path, sizeof(path),
				SYS_CPU_PATH "/%s/microcode/version",
				entry->d_name);
			if ((data = fwts_get(path)) != NULL) {
				int cpu = (int)strtoul(&entry->d_name[3], NULL, 16);
				int revision = (int)strtoul(data, NULL, 16);
				free(data);

				info = microcode_find_cpu(cpu, &microcode_info_list);
				if (info == NULL) {
					/*
					 * It may be null because the kernel log
					 * is old and we've now lost the log
					 * messages, so we can't really sanity
					 * check, so it's not strictly a failure.
					 */
					fwts_log_info(fw,
						"Could not determine if CPU %d had a microcode "
						"update from the kernel message log.", cpu);
					continue;
				}

				/*
				 * Kernel does not have newer version than BIOS
				 */
				if (info->new_revision == UNKNOWN) {
					fwts_log_info(fw, "The kernel did not report that CPU %d has had a microcode update. "
						"The current firmware is revision 0x%x and probably has not been updated.",
						cpu, info->old_revision);
					continue;
				}

				/*
				 * We found a new revision but it does not
				 * match the CPU info, failed
				 */
				if (info->new_revision != revision) {
					failed++;
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "MicrocodeMismatch",
						"The kernel has reported that CPU %d has had a microcode update "
						"to revision 0x%x, however, the processor seems to be running "
						"with a different revision 0x%x",
						cpu, info->new_revision, revision);
					continue;
				}

				/*
				 * We found a new revision but not an old one,
				 * assume it was OK since it got updated
				 */
				if (info->old_revision == UNKNOWN) {
					passed++;
					fwts_log_info(fw, "CPU %d microcode updated to revision 0x%x.",
						cpu, info->new_revision);
					continue;
				}

				/* Final sanity check */
				if (info->new_revision < info->old_revision) {
					failed++;
					fwts_failed(fw, LOG_LEVEL_HIGH, "MicrocodeDowngrade",
						"The kernel has reported that CPU %d has had a microcode update "
						"downgrade from revision 0x%x down to revision 0x%x (%d-%2.2d-%2.2d).",
						cpu, info->new_revision, info->old_revision,
						info->year, info->month, info->day);
				} else {
					passed++;
					fwts_log_info(fw, "CPU %d microcode updated from revision 0x%x to revision 0x%x (%d-%2.2d-%2.2d).",
						cpu, info->old_revision, info->new_revision,
						info->year, info->month, info->day);
				}
			}
		}
	}
	if (!failed) {
		if (passed > 0)
			fwts_passed(fw, "%d CPU(s) have the latest microcode loaded.", passed);
		else
			fwts_skipped(fw, "Could not determine from kernel log if latest microcode has been loaded.");
	}

	closedir(dir);

	fwts_list_free_items(&microcode_info_list, free);

	return FWTS_OK;
}

static fwts_framework_minor_test microcode_tests[] = {
	{ microcode_test1, "Test for most recent microcode being loaded." },
	{ NULL, NULL }
};

static fwts_framework_ops microcode_ops = {
	.description = "Test if system is using latest microcode.",
	.init        = microcode_init,
	.deinit      = microcode_deinit,
	.minor_tests = microcode_tests
};

FWTS_REGISTER("microcode", &microcode_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
