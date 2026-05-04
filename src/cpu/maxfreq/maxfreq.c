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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

#define CPU_FREQ_PATH	"/sys/devices/system/cpu"
#define CPU_INFO_PATH	"/proc/cpuinfo"

static double maxfreq_max(const char *str)
{
	double max = -1.0;

	while (str && *str) {
		double val;

		while ((*str != '\0') && isspace(*str))
			str++;

		if (!isdigit(*str))
			break;

		val = atof(str);
		if (val > max)
			max = val;
		str = strstr(str, " ");
	}
	return max;
}

static int maxfreq_test1(fwts_framework *fw)
{
	double *cpufreq;
	int cpu, cpus;
	bool advice = false, passed = true, cpufreqs_read = false;
	struct dirent *entry;
	DIR *dir;
	fwts_list_link *item;
	fwts_list *cpuinfo;

	fwts_log_info(fw,
		"This test checks the maximum CPU frequency as detected by "
		"the kernel for each CPU against maximum frequency as "
		"specified by the BIOS frequency scaling settings.");

	cpuinfo = fwts_file_open_and_read(CPU_INFO_PATH);
	if (cpuinfo == NULL) {
		fwts_log_error(fw, "Cannot create cpu info list.");
		return FWTS_ERROR;
	}

	/* Pass 1, figure out number of CPUs */
	cpus = 0;
	fwts_list_foreach(item, cpuinfo) {
		if (strstr(fwts_text_list_text(item), "model name"))
			cpus++;
	}
	if (!cpus) {
		fwts_list_free(cpuinfo, free);
		fwts_skipped(fw,
			"Cannot read CPU model names from %s, this generally "
			"happens on ARM CPUs, skipping test.", CPU_INFO_PATH);
		return FWTS_SKIP;
	}

	if ((cpufreq = calloc(cpus, sizeof(double))) == NULL) {
		fwts_log_error(fw, "Cannot create cpu frequency array.");
		fwts_list_free(cpuinfo, free);
		return FWTS_ERROR;
	}

	/* Pass 2, get speed */
	cpu = 0;
	fwts_list_foreach(item, cpuinfo) {
		char *str = fwts_text_list_text(item);
		if (strstr(str, "model name")) {
			if ((str = strstr(str, "@")) != NULL) {
				cpufreq[cpu] = atof(str+1);
				if (strstr(str, "GHz"))
					cpufreq[cpu] *= 1000000.0;
				if (strstr(str, "MHz"))
					cpufreq[cpu] *= 1000.0;
				cpufreqs_read = true;
			}
			else
				cpufreq[cpu] = -1.0;

			cpu++;
		}
	}
	fwts_list_free(cpuinfo, free);

	if (!cpufreqs_read) {
		fwts_skipped(fw,
			"Cannot read CPU frequencies from %s, this generally "
			"happens on AMD CPUs, skipping test.", CPU_INFO_PATH);
		free(cpufreq);
		return FWTS_SKIP;
	}

	if (!(dir = opendir(CPU_FREQ_PATH))) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"CPUFreqNoPath",
			"No %s directory available: cannot test.",
			CPU_FREQ_PATH);
		free(cpufreq);
		return FWTS_ERROR;
	}

	do {
		char path[PATH_MAX];
		char *data;
		double maxfreq, maxfreq_ghz, cpufreq_ghz;
		int cpunum;

		entry = readdir(dir);
		if (!entry || entry->d_name[0] == '.' ||
		    strlen(entry->d_name) < 3)
			continue;

		strlcpy(path, CPU_FREQ_PATH, strlen(CPU_FREQ_PATH) + 1);
		snprintf(path + strlen(CPU_FREQ_PATH), sizeof(path) - strlen(CPU_FREQ_PATH),
			"/%s/cpufreq/scaling_available_frequencies", entry->d_name);

		if ((data = fwts_get(path)) == NULL)
			continue;
		maxfreq = maxfreq_max(data);
		free(data);

		cpunum = atoi(entry->d_name + 3);
		if (maxfreq < 0.0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"CPUFreqReadFailed",
				"Cannot read cpu frequency from %s for CPU %s.",
				CPU_FREQ_PATH, entry->d_name);
			passed = false;
			continue;
		}

		maxfreq_ghz = maxfreq / 1000000.0;
		cpufreq_ghz = cpufreq[cpunum] / 1000000.0;

		if (fabs(maxfreq_ghz - cpufreq_ghz) > (maxfreq_ghz * 0.005)) {
			passed = false;
			fwts_failed(fw,
				LOG_LEVEL_MEDIUM,
				"CPUFreqSpeedMismatch",
				"Maximum scaling frequency %f GHz do not match "
				"expected frequency %f GHz.",
				maxfreq_ghz, cpufreq_ghz);
			if (!advice)  {
				advice = true;
				fwts_advice(fw,
					"The maximum scaling frequency %f GHz "
					"for CPU %d configured by the BIOS in "
					"%s does not match the expected "
					"maximum CPU frequency %f GHz that "
					"the CPU can run at. This usually "
					"indicates a misconfiguration of the "
					"ACPI _PSS (Performance Supported "
					"States) object. This is described in "
					"section 8.4.4.2 of the ACPI "
					"specification.",
					(double)maxfreq/1000000.0, cpunum, path,
					(double)cpufreq[cpunum]/1000000.0);
			} else {
				fwts_advice(fw, "See advice for previous CPU.");
			}
		} else {
			fwts_log_info(fw,
				"CPU %d maximum frequency %f GHz is sane.",
				cpunum, maxfreq_ghz);
		}
	} while (entry);

	if (passed)
		fwts_passed(fw, "%d CPUs passed the maximum frequency check.", cpus);

	free(cpufreq);
	(void)closedir(dir);

	return FWTS_OK;
}

static fwts_framework_minor_test maxfreq_tests[] = {
	{ maxfreq_test1, "Maximum CPU frequency test." },
	{ NULL, NULL }
};

static fwts_framework_ops maxfreq_ops = {
	.description = "Test max CPU frequencies against max scaling frequency.",
	.minor_tests = maxfreq_tests
};

FWTS_REGISTER("maxfreq", &maxfreq_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)
