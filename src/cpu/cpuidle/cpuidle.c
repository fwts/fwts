/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2025 Canonical
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#define _GNU_SOURCE	/* for sched_setaffinity */

#include "fwts.h"

#define PROCESSOR_PATH	"/sys/devices/system/cpu"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdint.h>
#include <ctype.h>

#define MIN_CSTATE	1
#define MAX_CSTATE	16

typedef struct {
	int counts[MAX_CSTATE];
	bool used[MAX_CSTATE];
	bool present[MAX_CSTATE];
} fwts_cpuidle_states;

static int statecount = -1;
static int firstcpu = -1;

static void get_cpuidle_states(char *path, fwts_cpuidle_states *state)
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
#ifdef FWTS_ARCH_INTEL
		if (strlen(entry->d_name) > 3) {
#else
		if (strlen(entry->d_name) > 5 && strncmp(entry->d_name, "state", 5) == 0) {
#endif
			long int nr;
			int count;

			snprintf(filename, sizeof(filename), "%s/%s/name",
				path, entry->d_name);
			if ((data = fwts_get(filename)) == NULL)
				break;

#ifdef FWTS_ARCH_INTEL
			/*
			 * Names can be "Cx\n", or "ATM-Cx\n", or "SNB-Cx\n",
			 * or newer kernels can be "Cx\n" or "Cx-SNB\n" etc
			 * where x is the idle state number.
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
#else
			/*
			 * For non-x86 platforms (ARM, etc.), use directory name
			 * which should be "state0", "state1", "state2", etc.
			 * No need to parse the state name for ARM platforms.
			 */
			nr = strtol(entry->d_name + 5, NULL, 10);
#endif

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
	fwts_cpuidle_states initial, current;
	int	count;
	char	buffer[128];
	char	tmp[16];  /* Increased to accommodate "state%d " format */
	bool	keepgoing = true;
	int	i;

	get_cpuidle_states(path, &initial);

	for (i = 0; (i < TOTAL_WAIT_TIME) && keepgoing; i++) {
		int j;

		/* Report progress less frequently to reduce overhead */
		if ((i % 3) == 0) {
			snprintf(buffer, sizeof(buffer),"(CPU %d of %d)", nth + 1, cpus);
			fwts_progress_message(fw,
				100 * (i + (TOTAL_WAIT_TIME*nth)) /
					(cpus * TOTAL_WAIT_TIME), buffer);
		}

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

		get_cpuidle_states(path, &current);

		keepgoing = false;
		for (j = MIN_CSTATE; j < MAX_CSTATE; j++) {
			if (initial.counts[j] != current.counts[j]) {
				initial.counts[j] = current.counts[j];
				initial.used[j] = true;
			}
			if (initial.present[j] && !initial.used[j])
				keepgoing = true;
		}
		
		/* Early termination: if all states have been reached, we can stop */
		if (!keepgoing) {
			break;
		}
	}

	*buffer = '\0';
	if (keepgoing) {
		/* Not a failure, but not a pass either! */
		for (i = MIN_CSTATE; i < MAX_CSTATE; i++)  {
			if (initial.present[i] && !initial.used[i]) {
#ifdef FWTS_ARCH_INTEL
				snprintf(tmp, sizeof(tmp), "C%d ", i);
#else
				snprintf(tmp, sizeof(tmp), "state%d ", i);
#endif
				strcat(buffer, tmp);
			}
		}
		fwts_log_info(fw, "Processor %d has not reached %s during tests. "
				  "This is not a failure, however it is not a "
				  "complete and thorough test.", cpu, buffer);
	} else {
		/* Build the result string more efficiently */
		int pos = 0;
		for (i = MIN_CSTATE; i < MAX_CSTATE; i++)  {
			if (initial.present[i] && initial.used[i]) {
#ifdef FWTS_ARCH_INTEL
				pos += snprintf(buffer + pos, sizeof(buffer) - pos, "C%d ", i);
#else
				pos += snprintf(buffer + pos, sizeof(buffer) - pos, "state%d ", i);
#endif
			}
		}
		fwts_passed(fw, "Processor %d has reached all idle states: %s",
			cpu, buffer);
	}

	count = 0;
	for (i = MIN_CSTATE; i < MAX_CSTATE; i++)
		if (initial.present[i])
			count++;

	if (statecount == -1)
		statecount = count;

	if (statecount != count)
		fwts_failed(fw, LOG_LEVEL_HIGH, "CPUNoIdleState",
			"Processor %d is expected to have %d idle states but has %d.",
			cpu, statecount, count);
	else
		if (firstcpu == -1)
			firstcpu = cpu;
		else
			fwts_passed(fw, "Processor %d has the same number of idle states as processor %d",
				cpu, firstcpu);
}

static int cpuidle_test1(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *entry;
	int cpus;
	int i;
	bool has_cpuidle = false;

	fwts_log_info(fw,
		"This test checks if all processors have the same number of "
		"idle states, if the idle state counter works and if idle state "
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

	/* Check if any CPU has cpuidle support */
	for (i = 0; (cpus > 0) && (entry = readdir(dir)) != NULL; ) {
		if (entry &&
		    (strlen(entry->d_name)>3) &&
		    (strncmp(entry->d_name, "cpu", 3) == 0) &&
		    (isdigit(entry->d_name[3]))) {
			char cpupath[PATH_MAX];
			DIR *cpuidle_dir;

			snprintf(cpupath, sizeof(cpupath), "%s/%s/cpuidle",
				PROCESSOR_PATH, entry->d_name);
			
			if ((cpuidle_dir = opendir(cpupath)) != NULL) {
				has_cpuidle = true;
				closedir(cpuidle_dir);
				break;
			}
		}
	}

	/* If no CPU has cpuidle support, check if this is an ACPI system */
	if (!has_cpuidle) {
		struct stat statbuf;
		if (!stat("/sys/firmware/acpi", &statbuf)) {
			/* ACPI system without cpuidle - this is a failure */
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPIWithoutCPUIDLE",
				"ACPI system detected but no CPU idle states found. "
				"This indicates the CPU idle driver is not properly "
				"configured or loaded.");
			closedir(dir);
			return FWTS_ERROR;
		} else {
			/* Non-ACPI system without cpuidle - skip the test */
			fwts_skipped(fw, "No CPU idle states found on this system. "
				"This is normal for systems that don't support "
				"CPU idle power management.");
			closedir(dir);
			return FWTS_SKIP;
		}
	}

	rewinddir(dir);

	/* Now run the actual tests */
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

static fwts_framework_minor_test cpuidle_tests[] = {
	{ cpuidle_test1, "Test all CPUs idle states." },
	{ NULL, NULL }
};

static fwts_framework_ops cpuidle_ops = {
	.description = "Processor idle state support test.",
	.minor_tests = cpuidle_tests
};

FWTS_REGISTER("cpuidle", &cpuidle_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)
