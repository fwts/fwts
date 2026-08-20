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
#define _GNU_SOURCE

#include "fwts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <inttypes.h>

#define FWTS_CPU_PATH	"/sys/devices/system/cpu"

#define MAX_FREQS	256

typedef struct {
	uint64_t	Hz;
	fwts_cpu_benchmark_result perf;
} fwts_cpu_freq;

struct cpu {
	int		idx;
	char		sysfs_path[2048];	/* 2K is plenty */
	bool		online;
	bool		master;

	int		n_freqs;
	fwts_cpu_freq	freqs[MAX_FREQS];

	/* saved state */
	char		*orig_governor;
	uint64_t	orig_frequency;
};

static struct cpu *cpus;
static int num_cpus;
static bool cpufreq_settable = true;

#define GET_PERFORMANCE_MAX (0)
#define GET_PERFORMANCE_MIN (1)
#define GET_PERFORMANCE_AVG (2)


#define MAX_ABSOLUTE_ERROR	20.0		/* In Hz */
#define MAX_RELATIVE_ERROR	0.0025		/* as fraction */
#define MAX_REPEATS		(5)

static inline void cpu_mkpath(
	char *const path,
	const int len,
	const struct cpu *cpu,
	const char *const name)
{
	snprintf(path, len, "%s/%s/cpufreq%s%s", FWTS_CPU_PATH,
			cpu->sysfs_path,
			name ? "/" : "",
			name ? name : "");
}

static int cpu_set_governor(
	fwts_framework *fw,
	const struct cpu *cpu,
	const char *governor)
{
	char path[PATH_MAX], *tmp;
	int rc;

	cpu_mkpath(path, sizeof(path), cpu, "scaling_governor");
	rc = fwts_set(path, governor);
	if (rc != FWTS_OK)
		goto out;

	tmp = fwts_get(path);
	rc = tmp && !strncmp(tmp, governor, strlen(governor))
		? FWTS_OK : FWTS_ERROR;
	free(tmp);

out:
	if (rc != FWTS_OK)
		fwts_log_info(fw, "Cannot set CPU %d governor to %s.",
				cpu->idx, governor);
	return rc;
}

static int cpu_set_frequency(
	fwts_framework *fw,
	const struct cpu *cpu,
	const uint64_t freq_hz)
{
	char path[PATH_MAX], *tmp;
	char buffer[64];
	int rc;

	cpu_mkpath(path, sizeof(path), cpu, "scaling_setspeed");
	snprintf(buffer, sizeof(buffer), "%" PRIu64 , freq_hz);
	rc = fwts_set(path, buffer);
	if (rc != FWTS_OK)
		goto out;

	tmp = fwts_get(path);
	rc = tmp && !strncmp(tmp, buffer, strlen(buffer))
		? FWTS_OK : FWTS_ERROR;
	free(tmp);

out:
	if (rc != FWTS_OK)
		fwts_warning(fw, "Cannot set CPU %d frequency to %s when setting %s.",
			cpu->idx, buffer, path);
	return rc;
}

static inline int cpu_set_lowest_frequency(
	fwts_framework *fw,
	const struct cpu *cpu)
{
	return cpu_set_frequency(fw, cpu, cpu->freqs[0].Hz);
}

static inline int cpu_set_highest_frequency(
	fwts_framework *fw,
	const struct cpu *cpu)
{
	return cpu_set_frequency(fw, cpu, cpu->freqs[cpu->n_freqs-1].Hz);
}


static int get_performance_repeat(
	fwts_framework *fw,
	struct cpu *cpu,
	const int count,
	const int type,
	uint64_t *retval)
{
	fwts_cpu_benchmark_result result;
	int i;

	uint64_t max = 0;
	uint64_t min = ~0;
	uint64_t real_count = 0;
	uint64_t cumulative = 0;

	for (i = 0; i < count; i++) {
		uint64_t temp;

		if (fwts_cpu_benchmark(fw, cpu->idx, &result) != FWTS_OK)
			return FWTS_ERROR;

		temp = fwts_cpu_benchmark_best_result(&result);
		if (temp) {
			if (temp < min)
				min = temp;

			if (temp > max)
				max = temp;

			cumulative += temp;
			real_count++;
		}
	}

	switch (type) {
	case GET_PERFORMANCE_MAX:
		*retval = max;
		break;
	case GET_PERFORMANCE_MIN:
		*retval = min;
		break;
	case GET_PERFORMANCE_AVG:
		if (real_count)
			*retval = cumulative / real_count;
		else
			*retval = 0;
		break;
	default:
		*retval = 0;
		break;
	}
	return FWTS_OK;
}

#ifdef FWTS_ARCH_INTEL
/*
 *  hz_almost_equal()
 *	used to compare CPU _PSS levels, are they almost
 *	equal?  E.g. within MAX_ABSOLUTE_ERROR Hz difference
 *	between each other, or a relative difference of
 *	MAX_RELATIVE_ERROR.  If they are, then they are deemed
 *	almost equal.
 */
static int hz_almost_equal(const uint64_t a, const uint64_t b)
{
	double da = (double)a, db = (double)b;
	double relative_error, abs_diff = fabs(da - db);

	if (a == b)
		return true;
	if (abs_diff < MAX_ABSOLUTE_ERROR)
		return true;
	if (db > da)
		relative_error = abs_diff / db;
	else
		relative_error = abs_diff / da;

	return relative_error <= MAX_RELATIVE_ERROR;
}
#endif

static char *hz_to_human(const uint64_t hz)
{
	static char buffer[32];

	if (hz > 1000000) {
		snprintf(buffer, sizeof(buffer), "%6.3f GHz",
			(double)hz / 1000000.0);
		return buffer;
	} else if (hz > 1000) {
		snprintf(buffer, sizeof(buffer), "%6.3f MHz",
			(double)hz / 1000.0);
		return buffer;
	} else {
		snprintf(buffer, sizeof(buffer), "%" PRIu64 " Hz", hz);
		return buffer;
	}
}

static uint64_t get_claimed_hz(struct cpu *cpu)
{
	char path[PATH_MAX];
	char *buffer;
	uint64_t value = 0;

	cpu_mkpath(path, sizeof(path), cpu, "scaling_max_freq");

	if ((buffer = fwts_get(path)) != NULL) {
		value = strtoul(buffer, NULL, 10);
		free(buffer);
	}
	return value;
}

static uint64_t get_bios_limit(struct cpu *cpu)
{
	char path[PATH_MAX];
	char *buffer;
	uint64_t value = 0;

	cpu_mkpath(path, sizeof(path), cpu, "bios_limit");

	if ((buffer = fwts_get(path)) != NULL) {
		value = strtoul(buffer, NULL, 10);
		free(buffer);
	}
	return value;
}

static int test_one_cpu_performance(
	fwts_framework *fw,
	struct cpu *cpu,
	const int cpu_idx,
	const int n_online_cpus)
{
	uint64_t cpu_top_perf = 1;
	int i;

	for (i = 0; i < cpu->n_freqs; i++) {
		uint64_t perf;

		cpu_set_frequency(fw, cpu, cpu->freqs[i].Hz);

		if (fwts_cpu_benchmark(fw, cpu->idx, &cpu->freqs[i].perf)
				!= FWTS_OK) {
			fwts_log_error(fw, "Failed to get CPU performance for "
				"CPU frequency %" PRId64 " Hz.",
				cpu->freqs[i].Hz);
		}
		perf = fwts_cpu_benchmark_best_result(&cpu->freqs[i].perf);
		if (perf > cpu_top_perf)
			cpu_top_perf = perf;

		fwts_progress(fw, (100 * ((cpu_idx * cpu->n_freqs) + i)) /
				(n_online_cpus * cpu->n_freqs));
	}

	fwts_log_info(fw, "CPU %d: %i CPU frequency steps supported.",
			cpu->idx, cpu->n_freqs);
	fwts_log_info_verbatim(fw,
		" Frequency | Relative Speed |  Cycles    | Bogo loops");
	fwts_log_info_verbatim(fw,
		"-----------+----------------+------------+-----------");
	for (i = 0; i < cpu->n_freqs; i++) {
		char *turbo = "";

#ifdef FWTS_ARCH_INTEL
		if ((i == 0) && (cpu->n_freqs > 1) &&
			(hz_almost_equal(cpu->freqs[i].Hz, cpu->freqs[i + 1].Hz)))
			turbo = " (Turbo Boost)";
#endif
		uint64_t perf = fwts_cpu_benchmark_best_result(
				&cpu->freqs[i].perf);
		fwts_log_info_verbatim(fw,
				"%10s |     %5.1f %%    "
				"| %10" PRIu64 " | %9" PRIu64 "%s",
			hz_to_human(cpu->freqs[i].Hz),
			100.0 * perf / cpu_top_perf,
			cpu->freqs[i].perf.cycles,
			cpu->freqs[i].perf.loops,
			turbo);
	}

	fwts_log_nl(fw);

	/* now check for increasing performance */
	for (i = 0; i < cpu->n_freqs - 1; i++) {
		uint64_t perf, last_perf;

		last_perf = fwts_cpu_benchmark_best_result(&cpu->freqs[i].perf);
		perf = fwts_cpu_benchmark_best_result(&cpu->freqs[i+1].perf);
		if (last_perf <= perf)
			continue;

		fwts_log_warning(fw,
			"Supposedly higher frequency %s is slower (%" PRIu64
			") than frequency %s (%" PRIu64
			") on CPU %i.",
			hz_to_human(cpu->freqs[i+1].Hz), perf,
			hz_to_human(cpu->freqs[i].Hz), last_perf,
			cpu->idx);
		return FWTS_ERROR;
	}

	fwts_log_info(fw, "CPU %d performance scaling OK", cpu->idx);
	return FWTS_OK;
}

static int cpufreq_test_cpu_performance(fwts_framework *fw)
{
	int n_master_cpus, i, c, rc;
	bool ok = true;

	n_master_cpus = 0;

	if (!cpufreq_settable) {
		fwts_skipped(fw, "Can't set CPU frequencies");
		return FWTS_SKIP;
	}

	for (i = 0; cpufreq_settable && i < num_cpus; i++) {
		if (!(cpus[i].online && cpus[i].master))
			continue;
		n_master_cpus++;
		rc = cpu_set_lowest_frequency(fw, &cpus[i]);
		if (rc != FWTS_OK)
			cpufreq_settable = false;
	}

	/* then do the benchmark */
	for (i = 0, c = 0; i < num_cpus; i++) {
		if (!(cpus[i].online && cpus[i].master))
			continue;

		rc = test_one_cpu_performance(fw, &cpus[i], c++, n_master_cpus);
		if (rc != FWTS_OK)
			ok = false;

		cpu_set_lowest_frequency(fw, &cpus[i]);
	}

	if (ok)
		fwts_passed(fw, "CPU performance scaling OK");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"CPUFreqSlowerOnCPU",
			"CPU frequencies do not refelect actual performance");
	return FWTS_OK;
}

static int sw_tests_possible(fwts_framework *fw)
{
	int i, online_cpus;

#ifndef FWTS_ARCH_INTEL
	fwts_skipped(fw, "Platform doesn't perform SW_ cpu frequency control");
	return FWTS_SKIP;
#endif

	if (!cpufreq_settable) {
		fwts_skipped(fw, "Can't set CPU frequencies");
		return FWTS_SKIP;
	}

	/* count the number of CPUs online now */
	for (online_cpus = 0, i = 0; i < num_cpus; i++)
		if (cpus[i].online)
			online_cpus++;

	if (online_cpus < 2) {
		fwts_skipped(fw, "Machine only has one CPU online");
		return FWTS_SKIP;
	}

	if (cpus[0].n_freqs < 2) {
		fwts_skipped(fw, "No frequency changes possible");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

/*
 * 4) Is BIOS wrongly doing Sw_All P-state coordination across cpus
 * - Change frequency on all CPU to the lowest value
 * - Change frequency on one particular CPU to the highest
 * - If BIOS is doing Sw_All, the last high freq request will not work
 */
static int cpufreq_test_sw_all(fwts_framework *fw)
{
	uint64_t highperf, lowperf;
	int i, rc;

	rc = sw_tests_possible(fw);
	if (rc != FWTS_OK)
		return rc;

	/* All CPUs at the lowest frequency */
	for (i = 0; i < num_cpus; i++)
		cpu_set_lowest_frequency(fw, &cpus[i]);

	if (get_performance_repeat(fw, &cpus[0], MAX_REPEATS, GET_PERFORMANCE_MIN, &lowperf) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUFreqSW_ALLGetPerf",
			"Failed to get CPU performance.");
		return FWTS_ERROR;
	}

	cpu_set_highest_frequency(fw, &cpus[0]);
	if (get_performance_repeat(fw, &cpus[0], MAX_REPEATS, GET_PERFORMANCE_MAX, &highperf) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUFreqSW_ALLGetPerf",
			"Failed to get CPU performance.");
		return FWTS_ERROR;
	}

	if (lowperf >= highperf) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"CPUFreqSW_ALL",
			"Firmware not implementing hardware "
			"coordination cleanly. Firmware using SW_ALL "
			"instead?");
		return FWTS_ERROR;
	}

	fwts_passed(fw, "Firmware SW_ALL OK");
	return FWTS_OK;
}


static int cpufreq_test_sw_any(fwts_framework *fw)
{
	uint64_t low_perf, high_perf, newhigh_perf;
	int i, j, rc, n_tests, performed_tests;
	fwts_cpu_benchmark_result result;
	bool ok;

	rc = sw_tests_possible(fw);
	if (rc != FWTS_OK)
		return rc;

	n_tests = performed_tests = 0;
	for (i = 0; i < num_cpus; i++)
		if (cpus[i].online)
			n_tests++;

	/* we do two performance measurements per cpu */
	n_tests *= 2;

	/* First set all processors to their lowest speed */
	for (i = 0; i < num_cpus; i++)
		cpu_set_lowest_frequency(fw, &cpus[i]);

	/* assume that all processors have the same low performance */
	if (fwts_cpu_benchmark(fw, cpus[0].idx, &result) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"CPUFreqCPsSetToSW_ANYGetPerf",
			"Cannot get CPU performance.");
		return FWTS_ERROR;
	}
	low_perf = fwts_cpu_benchmark_best_result(&result);

	ok = true;

	for (i = 0; i < num_cpus; i++) {
		const struct cpu *cpu = &cpus[i];

		if (!cpu->online)
			continue;

		cpu_set_highest_frequency(fw, cpu);
		if (!cpu->online)
			continue;

		if (fwts_cpu_benchmark(fw, cpu->idx, &result) != FWTS_OK) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"CPUFreqCPsSetToSW_ANYGetPerf",
				"Cannot get CPU performance.");
			return FWTS_ERROR;
		}
		high_perf = fwts_cpu_benchmark_best_result(&result);

		performed_tests++;
		if (n_tests)
			fwts_progress(fw, 100 * performed_tests / n_tests);
		/*
		 * now set all the others to low again; sw_any will cause
		 * the core in question to now also get the low speed, while
		 * hardware max will keep the performance
		 */
		for (j = 0; j < num_cpus; j++)
			if (i != j)
				cpu_set_lowest_frequency(fw, &cpus[j]);
		if (fwts_cpu_benchmark(fw, cpu->idx, &result)
				!= FWTS_OK) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"CPUFreqCPsSetToSW_ANYGetPerf",
				"Cannot get CPU performance.");
			return FWTS_ERROR;
		}
		newhigh_perf = fwts_cpu_benchmark_best_result(&result);
		if ((high_perf > newhigh_perf) &&
		    (high_perf - newhigh_perf > (high_perf - low_perf)/4) &&
		    (high_perf - low_perf > 20)) {
			cpu_set_lowest_frequency(fw, cpu);
			ok = false;
		}
		performed_tests++;
		if (n_tests)
			fwts_progress(fw, 100 * performed_tests / n_tests);
	}

	if (!ok)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"CPUFreqCPUsSetToSW_ANY",
			"Processors are set to SW_ANY.");
	else
		fwts_passed(fw, "P-state coordination "
				"under hardware control.");

	return FWTS_OK;
}

static int cpufreq_compare_freqs(
	fwts_framework *fw,
	const struct cpu *c1,
	const struct cpu *c2)
{
	int i;

	if (c1->n_freqs != c2->n_freqs) {
		fwts_log_warning(fw, "cpu %d has %d freqs, cpu %d has %d freqs",
				c1->idx, c1->n_freqs,
				c2->idx, c2->n_freqs);
		return FWTS_ERROR;
	}

	for (i = 0; i < c1->n_freqs; i++) {
		if (c1->freqs[i].Hz != c2->freqs[i].Hz) {
			fwts_log_warning(fw, "freq entry %d: "
						"cpu %d is %" PRId64 ", "
						"cpu %d is %" PRId64, i,
					c1->idx, c1->freqs[i].Hz,
					c2->idx, c2->freqs[i].Hz);
			return FWTS_ERROR;
		}
	}

	return FWTS_OK;
}

static int cpufreq_test_consistency(fwts_framework *fw)
{
	struct cpu *cpu0;
	bool consistent = true;
	int i;

	if (num_cpus < 2) {
		fwts_skipped(fw, "Test skipped, only one processor present");
		return FWTS_SKIP;
	}

	cpu0 = &cpus[0];

	for (i = 1; i < num_cpus; i++) {
		struct cpu *cpu = &cpus[i];
		if (cpufreq_compare_freqs(fw, cpu0, cpu) != FWTS_OK) {
			consistent = false;
			fwts_log_error(fw,
				"cpu %d has an inconsistent frequency table",
				cpu->idx);
		}
	}

	if (!consistent)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUFreqInconsistent",
				"inconsistencies found in CPU "
				"frequency tables");
	else
		fwts_passed(fw, "CPU frequency tables are consistent");

	return FWTS_OK;
}

static int cpufreq_test_duplicates(fwts_framework *fw)
{
	struct cpu *cpu0 = &cpus[0];
	bool dup = false;
	int i;

	/* the frequency list is sorted, so we can do this in one pass */
	for (i = 0; i < cpu0->n_freqs - 1; i++) {
		uint64_t freq = cpu0->freqs[i].Hz;

		if (cpu0->freqs[i+1].Hz != freq)
			continue;

		dup = true;
		fwts_log_error(fw, "duplicate cpu frequency %" PRIx64,
				freq);

		/* don't report further duplicates for this entry */
		for (i++; i < cpu0->n_freqs - 1; i++)
			if (cpu0->freqs[i].Hz != freq)
				break;
	}

	if (dup) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUFreqDuplicate",
				"duplicates found in CPU frequency table");
	} else {
		fwts_passed(fw, "No duplicates in CPU frequency table");
	}

	return FWTS_OK;
}

static int cpufreq_test_bios_limits(fwts_framework *fw)
{
	bool ok, present;
	int i;

	present = false;
	ok = true;

	for (i = 0; i < num_cpus; i++) {
		struct cpu *cpu = &cpus[i];
		uint64_t bios_limit;

		bios_limit = get_bios_limit(cpu);

		if (!bios_limit)
			continue;

		present = true;

		if (bios_limit < cpu->freqs[cpu->n_freqs-1].Hz) {
			ok = false;
			fwts_warning(fw, "cpu %d has bios limit of %" PRId64
					", lower than max freq of %"
					PRId64, cpu->idx, bios_limit,
					cpu->freqs[cpu->n_freqs-1].Hz);
		}
	}

	if (!present)
		fwts_passed(fw, "No BIOS limits imposed");
	else if (ok)
		fwts_passed(fw, "CPU BIOS limit OK");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUFreqBIOSLimit",
				"CPU BIOS limit is set too low");

	return FWTS_OK;
}

static int cpufreq_test_claimed_max(fwts_framework *fw)
{
	bool ok, present;
	int i;

	present = false;
	ok = true;

	for (i = 0; i < num_cpus; i++) {
		struct cpu *cpu = &cpus[i];
		uint64_t max;

		max = get_claimed_hz(cpu);

		if (!max)
			continue;

		if (!cpu->n_freqs)
			continue;

		present = true;

		if (max > cpu->freqs[cpu->n_freqs-1].Hz) {
			ok = false;
			fwts_warning(fw, "cpu %d has claimed frequency of %"
					PRId64 ", higher than max freq of %"
					PRId64, cpu->idx, max,
					cpu->freqs[cpu->n_freqs-1].Hz);
		}
	}

	if (!present)
		fwts_passed(fw, "No max frequencies present");
	else if (ok)
		fwts_passed(fw, "CPU max frequencies OK");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUFreqClaimedMax",
				"CPU max frequency is unreachable");

	return FWTS_OK;
}

static int cpu_freq_compare(const void *v1, const void *v2)
{
	const fwts_cpu_freq *f1 = v1;
	const fwts_cpu_freq *f2 = v2;

	return f1->Hz - f2->Hz;
}

static int parse_cpu_info(
	fwts_framework *fw,
	struct cpu *cpu,
	const struct dirent *dir)
{
	char *end, path[PATH_MAX+1], *str, *tmp;
	struct stat statbuf;
	int rc, i = 0;

	strcpy(cpu->sysfs_path, dir->d_name);
	cpu->idx = strtoul(cpu->sysfs_path + strlen("cpu"), &end, 10);
	cpu->online = true;

	/* check if this is the master of a group of CPUs; we only
	 * need to do perf checks on those that are the master */
	cpu_mkpath(path, sizeof(path), cpu, NULL);
	rc = lstat(path, &statbuf);
	if (rc) {
		fwts_log_warning(fw, "Can't access cpufreq info for CPU %d", cpu->idx);
		return FWTS_ERROR;
	}

	/* non-master CPUs will have a link, not a dir */
	cpu->master = S_ISDIR(statbuf.st_mode);

	cpu_mkpath(path, sizeof(path), cpu, "scaling_governor");
	cpu->orig_governor = fwts_get(path);

	if (cpu->orig_governor && !strcmp(cpu->orig_governor, "userspace")) {
		cpu_mkpath(path, sizeof(path), cpu, "scaling_setspeed");
		tmp = fwts_get(path);
		cpu->orig_frequency = strtoull(tmp, NULL, 10);
		free(tmp);
	}

	/* parse available frequencies */
	cpu_mkpath(path, sizeof(path), cpu, "scaling_available_frequencies");
	str = fwts_get(path);

	/* cpu driver like intel_pstate has no scaling_available_frequencies */
	if (str != NULL) {
		for (tmp = str; ; tmp = NULL) {
			const char *tok = strtok(tmp, " ");

			if (!tok)
				break;
			if (!isdigit(tok[0]))
				continue;
			cpu->freqs[i++].Hz = strtoull(tok, NULL, 10);
		}
		cpu->n_freqs = i;
		qsort(cpu->freqs, cpu->n_freqs, sizeof(cpu->freqs[0]),
			cpu_freq_compare);
		free(str);
	}

	/* parse boost frequencies */
	cpu_mkpath(path, sizeof(path), cpu, "scaling_boost_frequencies");
	str = fwts_get(path);

	if (str) {
		for (tmp = str; ; tmp = NULL) {
			const char *tok = strtok(tmp, " ");

			if (!tok)
				break;
			if (!isdigit(tok[0]))
				continue;
			cpu->freqs[i++].Hz = strtoull(tok, NULL, 10);
		}
		cpu->n_freqs = i;
		qsort(cpu->freqs, cpu->n_freqs, sizeof(cpu->freqs[0]),
		      cpu_freq_compare);
		free(str);
	}

	return FWTS_OK;
}

static int is_cpu_dir(const struct dirent *dir)
{
	return strncmp(dir->d_name, "cpu", 3) == 0 &&
		isdigit(dir->d_name[3]);
}

static int cpufreq_init(fwts_framework *fw)
{
	struct dirent **dirs;
	int i;

	num_cpus = scandir(FWTS_CPU_PATH, &dirs, is_cpu_dir, versionsort);
	cpus = calloc(num_cpus, sizeof(*cpus));

	/* all test require a userspace governor */
	for (i = 0; i < num_cpus; i++) {
		int rc;

		rc = parse_cpu_info(fw, &cpus[i], dirs[i]);
		if (rc != FWTS_OK) {
			fwts_log_warning(fw,
				"Failed to parse cpufreq for CPU %d", i);
			cpufreq_settable = false;
			return FWTS_ERROR;
		}

		rc = cpu_set_governor(fw, &cpus[i], "userspace");
		if (rc != FWTS_OK) {
			fwts_log_info(fw, "Cannot initialize cpufreq "
					"to set CPU speed for CPU %d", i);
			cpufreq_settable = false;
			return FWTS_SKIP;
		}
	}

	return FWTS_OK;
}

static int cpufreq_deinit(fwts_framework *fw)
{
	int i;

	for (i = 0; i < num_cpus; i++) {
		const struct cpu *cpu = &cpus[i];

		if (cpu->orig_governor) {
			cpu_set_governor(fw, cpu, cpu->orig_governor);
			free(cpu->orig_governor);
		}

		if (cpu->orig_frequency)
			cpu_set_frequency(fw, cpu, cpu->orig_frequency);
	}
	free(cpus);

	return FWTS_OK;
}

static fwts_framework_minor_test cpufreq_tests[] = {
	{ cpufreq_test_consistency,	"CPU frequency table consistency" },
	{ cpufreq_test_duplicates,	"CPU frequency table duplicates" },
	{ cpufreq_test_bios_limits,	"CPU frequency firmware limits" },
	{ cpufreq_test_claimed_max,	"CPU frequency claimed maximum" },
	{ cpufreq_test_sw_any,		"CPU frequency SW_ANY control" },
	{ cpufreq_test_sw_all,		"CPU frequency SW_ALL control" },
	{ cpufreq_test_cpu_performance,	"CPU frequency performance tests." },
	{ NULL, NULL }
};

static fwts_framework_ops cpufreq_ops = {
	.init        = cpufreq_init,
	.deinit      = cpufreq_deinit,
	.description = "CPU frequency scaling tests.",
	.minor_tests = cpufreq_tests
};

FWTS_REGISTER("cpufreq", &cpufreq_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)
