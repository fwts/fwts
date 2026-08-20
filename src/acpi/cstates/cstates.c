/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This file is was originally from the Linux-ready Firmware Developer Kit
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
#define _GNU_SOURCE	/* for sched_setaffinity */

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#define PROCESSOR_PATH	"/sys/devices/system/cpu"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <ctype.h>

#define MIN_CSTATE	1
#define MAX_CSTATE	16

typedef struct {
	int counts[MAX_CSTATE];
	bool used[MAX_CSTATE];
	bool present[MAX_CSTATE];
} fwts_cstates;

static int statecount = -1;
static int firstcpu = -1;

static void get_cstates(char *path, fwts_cstates *state)
{
	struct dirent *entry;
	char filename[PATH_MAX];
	char *data;
	DIR *dir;
	int i;

	for (i = MIN_CSTATE; i < MAX_CSTATE; i++) {
		state->counts[i] = 0;
		state->present[i] = false;
		state->used[i] = false;
	}

	if ((dir = opendir(path)) == NULL)
		return;

	while ((entry = readdir(dir)) != NULL) {
		if (strlen(entry->d_name) > 3) {
			long int nr;
			int count;

			snprintf(filename, sizeof(filename), "%s/%s/name",
				path, entry->d_name);
			if ((data = fwts_get(filename)) == NULL)
				break;

			/*
			 * Names can be "Cx\n", or "ATM-Cx\n", or "SNB-Cx\n",
			 * or newer kernels can be "Cx\n" or "Cx-SNB\n" etc
			 * where x is the C state number.
			 */
			if ((data[0] == 'C') && isdigit(data[1]))
				nr = strtol(data + 1, NULL, 10);
			else if (strcmp("POLL", data) == 0)
				nr = 0;
			else {
				char *ptr = strstr(data, "-C");
				if (ptr)
					nr = strtol(ptr + 2, NULL, 10);
				else
					nr = 0;
			}
			free(data);

			snprintf(filename, sizeof(filename), "%s/%s/usage",
				path, entry->d_name);
			if ((data = fwts_get(filename)) == NULL)
				break;
			count = strtoull(data, NULL, 10);
			free(data);

			if ((nr >= 0) && (nr < MAX_CSTATE)) {
				state->counts[nr] = count;
				state->present[nr] = true;
			}
		}
	}
	closedir(dir);
}

#define TOTAL_WAIT_TIME		20

static void do_cpu(fwts_framework *fw, int nth, int cpus, int cpu, char *path)
{
	fwts_cstates initial, current;
	int	count;
	char	buffer[128];
	char	tmp[8];
	bool	keepgoing = true;
	int	i;

	get_cstates(path, &initial);

	for (i = 0; (i < TOTAL_WAIT_TIME) && keepgoing; i++) {
		int j;

		snprintf(buffer, sizeof(buffer),"(CPU %d of %d)", nth + 1, cpus);
		fwts_progress_message(fw,
			100 * (i + (TOTAL_WAIT_TIME*nth)) /
				(cpus * TOTAL_WAIT_TIME), buffer);

		if ((i & 7) < 4)
			sleep(1);
		else {
			fwts_cpu_benchmark_result result;

			if (fwts_cpu_benchmark(fw, cpu, &result) != FWTS_OK) {
				fwts_failed(fw, LOG_LEVEL_HIGH, "CPUFailedPerformance",
					"Could not determine the CPU performance, this "
					"may be due to not being able to get or set the "
					"CPU affinity for CPU %d.", cpu);
			}
		}

		get_cstates(path, &current);

		keepgoing = false;
		for (j = MIN_CSTATE; j < MAX_CSTATE; j++) {
			if (initial.counts[j] != current.counts[j]) {
				initial.counts[j] = current.counts[j];
				initial.used[j] = true;
			}
			if (initial.present[j] && !initial.used[j])
				keepgoing = true;
		}
	}

	*buffer = '\0';
	if (keepgoing) {
		/* Not a failure, but not a pass either! */
		for (i = MIN_CSTATE; i < MAX_CSTATE; i++)  {
			if (initial.present[i] && !initial.used[i]) {
				snprintf(tmp, sizeof(tmp), "C%d ", i);
				strcat(buffer, tmp);
			}
		}
		fwts_log_info(fw, "Processor %d has not reached %s during tests. "
				  "This is not a failure, however it is not a "
				  "complete and thorough test.", cpu, buffer);
	} else {
		for (i = MIN_CSTATE; i < MAX_CSTATE; i++)  {
			if (initial.present[i] && initial.used[i]) {
				snprintf(tmp, sizeof(tmp), "C%d ", i);
				strcat(buffer, tmp);
			}
		}
		fwts_passed(fw, "Processor %d has reached all C-states: %s",
			cpu, buffer);
	}

	count = 0;
	for (i = MIN_CSTATE; i < MAX_CSTATE; i++)
		if (initial.present[i])
			count++;

	if (statecount == -1)
		statecount = count;

	if (statecount != count)
		fwts_failed(fw, LOG_LEVEL_HIGH, "CPUNoCState",
			"Processor %d is expected to have %d C-states but has %d.",
			cpu, statecount, count);
	else
		if (firstcpu == -1)
			firstcpu = cpu;
		else
			fwts_passed(fw, "Processor %d has the same number of C-states as processor %d",
				cpu, firstcpu);
}

static int cstates_test1(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *entry;
	int cpus;
	int i;

	fwts_log_info(fw,
		"This test checks if all processors have the same number of "
		"C-states, if the C-state counter works and if C-state "
		"transitions happen.");

	if ((dir = opendir(PROCESSOR_PATH)) == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "CPUNoSysMounted",
			"Cannot open %s: /sys not mounted?", PROCESSOR_PATH);
		return FWTS_ERROR;
	}

	/* How many CPUs are there? */
	for (cpus = 0; (entry = readdir(dir)) != NULL; )
		if (entry &&
		    (strlen(entry->d_name)>3) &&
		    (strncmp(entry->d_name, "cpu", 3) == 0) &&
		    (isdigit(entry->d_name[3])))
			cpus++;

	rewinddir(dir);

	for (i = 0; (cpus > 0) && (entry = readdir(dir)) != NULL; ) {
		if (entry &&
		    (strlen(entry->d_name)>3) &&
		    (strncmp(entry->d_name, "cpu", 3) == 0) &&
		    (isdigit(entry->d_name[3]))) {
			char cpupath[PATH_MAX];

			snprintf(cpupath, sizeof(cpupath), "%s/%s/cpuidle",
				PROCESSOR_PATH, entry->d_name);
			do_cpu(fw, i++, cpus, strtoul(entry->d_name+3, NULL, 10), cpupath);
		}
	}

	closedir(dir);

	return FWTS_OK;
}

static fwts_framework_minor_test cstates_tests[] = {
	{ cstates_test1, "Test all CPUs C-states." },
	{ NULL, NULL }
};

static fwts_framework_ops cstates_ops = {
	.description = "Processor C state support test.",
	.minor_tests = cstates_tests
};

FWTS_REGISTER("cstates", &cstates_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
