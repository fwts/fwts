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
#define _FILE_OFFSET_BITS 64

#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <math.h>
#include <sched.h>

#include <linux/perf_event.h>

#include "fwts_types.h"
#include "fwts_cpu.h"
#include "fwts_pipeio.h"

static int fwts_cpu_num;
static pid_t *fwts_cpu_pids;

#define MSR_AMD64_OSVW_ID_LENGTH	0xc0010140
#define MSR_AMD64_OSVW_STATUS		0xc0010141

/*
 *  fwts_cpu_readmsr()
 *	Read a given msr on a specified CPU
 */
int fwts_cpu_readmsr(
	fwts_framework *fw,
	const int cpu,
	const uint32_t reg,
	uint64_t *val)
{
	char buffer[PATH_MAX];
	uint64_t value = 0;
	int fd;
	int ret;

	snprintf(buffer, sizeof(buffer), "/dev/cpu/%d/msr", cpu);
	if ((fd = open(buffer, O_RDONLY)) < 0) {
		bool loaded = false;

		/*
		 *  msr not there, so force a load of the msr
		 *  module and retry
		 */
		if (fwts_module_load(fw, "msr") != FWTS_OK)
			return FWTS_ERROR;
		if (fwts_module_loaded(fw, "msr", &loaded) != FWTS_OK)
			return FWTS_ERROR;
		if (!loaded)
			return FWTS_ERROR;
		if ((fd = open(buffer, O_RDONLY)) < 0)
			return FWTS_ERROR; /* Really failed */
	}

	ret = pread(fd, &value, 8, reg);
	(void)close(fd);

	*val = value;

	if (ret < 0)
		return FWTS_ERROR;

	return FWTS_OK;
}

/*
 *  fwts_cpu_free_info()
 *	free CPU information
 */
void fwts_cpu_free_info(fwts_cpuinfo_x86 *cpu)
{
	if (cpu) {
		free(cpu->vendor_id);
		free(cpu->model_name);
		free(cpu->flags);
	}
	free(cpu);
}

/*
 *  fwts_cpu_get_info()
 *	get CPU information for specified CPU.
 *	Specify the CPU as -1 to query the first online CPU.
 */
fwts_cpuinfo_x86 *fwts_cpu_get_info(int which_cpu)
{
	FILE *fp;
	char buffer[1024];
	fwts_cpuinfo_x86 *cpu;
	int cpu_num = -1;
	bool found = false;

	if ((cpu = (fwts_cpuinfo_x86*)calloc(1, sizeof(fwts_cpuinfo_x86))) == NULL)
		return NULL;

	if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
		free(cpu);
		return NULL;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		char *ptr = strstr(buffer, ":");
		if (ptr)
			ptr += 2;
		else
			continue;

		buffer[strlen(buffer)-1] = '\0';

		if (!strncmp(buffer, "processor", 9)) {
			sscanf(ptr, "%d", &cpu_num);
			if (which_cpu == -1)
				which_cpu = cpu_num;
			if (cpu_num > which_cpu)
				break;
			continue;
		} else {
			if (cpu_num != which_cpu)
				continue;
		}

		found = true;

		if (!cpu->vendor_id && !strncmp(buffer, "vendor_id", 9)) {
			cpu->vendor_id = strdup(ptr);
			continue;
		}
		if (!strncmp(buffer, "cpu family",10)) {
			sscanf(ptr, "%d", &cpu->x86);
			continue;
		}
		if (!cpu->model_name && !strncmp(buffer, "model name", 10)) {
			cpu->model_name = strdup(ptr);
			continue;
		}
		if (!strncmp(buffer, "model", 5)) {
			sscanf(ptr, "%d", &cpu->x86_model);
			continue;
		}
		if (!strncmp(buffer, "stepping", 8)) {
			sscanf(ptr, "%d", &cpu->stepping);
			continue;
		}
		if (!cpu->flags && !strncmp(buffer, "flags", 5)) {
			cpu->flags = strdup(ptr);
			continue;
		}
	}
	(void)fclose(fp);

	if (!found) {
		free(cpu);
		cpu = NULL;
	}

	return cpu;
}

static int fwts_cpu_matches_vendor_id(const char *vendor_id, bool *matches)
{
	fwts_cpuinfo_x86 *cpu;

	if ((cpu = fwts_cpu_get_info(-1)) == NULL)
		return FWTS_ERROR;
	if (cpu->vendor_id == NULL) {
		fwts_cpu_free_info(cpu);
		return FWTS_ERROR;
	}

	*matches = (strstr(cpu->vendor_id, vendor_id) != NULL);
	fwts_cpu_free_info(cpu);

	return FWTS_OK;
}

int fwts_cpu_is_Intel(bool *is_intel)
{
	return fwts_cpu_matches_vendor_id("Intel", is_intel);
}

int fwts_cpu_is_AMD(bool *is_amd)
{
	return fwts_cpu_matches_vendor_id("AuthenticAMD", is_amd);
}

int fwts_cpu_is_Hygon(bool *is_hygon)
{
	return fwts_cpu_matches_vendor_id("HygonGenuine", is_hygon);
}

/*
 *  fwts_cpu_has_c1e()
 *	check if CPU has C1E bit
 */
fwts_bool fwts_cpu_has_c1e(void)
{
#if FWTS_ARCH_X86
	uint64_t val;
	fwts_bool rc = FWTS_FALSE;

	fwts_cpuinfo_x86 *cpu;

	if ((cpu = fwts_cpu_get_info(-1)) == NULL)
		return FWTS_BOOL_ERROR;
	if (cpu->flags == NULL) {
		rc FWTS_BOOL_ERROR;
		goto free_info;
	}
	if (cpu->vendor_id == NULL) {
		rc FWTS_BOOL_ERROR;
		goto free_info;
	}

	/* no C1E on AMD and Hygon */
	if (strstr(cpu->vendor_id, "AuthenticAMD") == NULL &&
	    strstr(cpu->vendor_id, "HygonGenuine") == NULL ) {
		rc = FWTS_FALSE;
		goto free_info;
	}

        /* Family 0x0f models < rev F do not have C1E */
        if (cpu->x86 == 0x0F && cpu->x86_model >= 0x40) {
                rc = FWTS_FALSE;
		goto free_info;
	}

        if (cpu->x86 == 0x10) {
                /*
                 * check OSVW bit for CPUs that are not affected
                 * by erratum #400
                 */
		if (strstr(cpu->flags, "osvw") != NULL) {
			if (fwts_cpu_readmsr(0, MSR_AMD64_OSVW_ID_LENGTH, &val) != FWTS_OK) {
				rc = FWTS_BOOL_ERROR;
				goto free_info;
			}

                        if (val >= 2) {
				if (fwts_cpu_readmsr(0, MSR_AMD64_OSVW_STATUS, &val) != FWTS_OK) {
					rc = FWTS_BOOL_ERROR;
					goto free_info;
				}
                                if (!(val & 2)) {
					rc = FWTS_FALSE;
					goto free_info;
				}
                        }
                }
                rc = FWTS_TRUE;
        }

free_info:
	fwts_cpu_free_info(cpu);
	return rc;
#else
	return FWTS_FALSE;
#endif
}

/*
 *  fwts_cpu_enumerate()
 *	enumerate all CPUs
 */
int fwts_cpu_enumerate(void)
{
	int cpus = sysconf(_SC_NPROCESSORS_CONF);

	if (cpus < 0)
		return FWTS_ERROR;

	return cpus;
}

/*
 *  fwts_cpu_consume_kill()
 *	kill CPU consumer processes as created by fwts_cpu_consume_cycles()
 */
static void fwts_cpu_consume_kill(void)
{
	int i;
	siginfo_t info;

	for (i = 0; i < fwts_cpu_num; i++) {
		if (fwts_cpu_pids[i] != 0) {
			kill(fwts_cpu_pids[i], SIGUSR1);
			waitid(P_PID, fwts_cpu_pids[i], &info, WEXITED);
		}
	}
}

/*
 *  fwts_cpu_consume_sighandler()
 *	CPU consumer processes signal handler
 */
static void fwts_cpu_consume_sighandler(int dummy)
{
	FWTS_UNUSED(dummy);

	_exit(0);
}

/*
 *  fwts_cpu_sigint_handler()
 *	kill all CPU consumer processes and die
 */
static void fwts_cpu_sigint_handler(int dummy)
{
	FWTS_UNUSED(dummy);

	fwts_cpu_consume_kill();
	_exit(0);
}


/*
 *  fwts_cpu_burn_cycles()
 *	burn some CPU cycles
 */
static void fwts_cpu_burn_cycles(void)
{
	double A = 1.234567;
	double B = 3.121213;
	int i;

	for (i = 0; i < 100; i++) {
		A = A * B;
		B = A * A;
		A = A - B + sqrt(A);
		A = A * B;
		B = A * A;
		A = A - B + sqrt(A);
		A = A * B;
		B = A * A;
		A = A - B + sqrt(A);
		A = A * B;
		B = A * A;
		A = A - B + sqrt(A);
	}
}

static int perf_setup_counter(int cpu)
{
	struct perf_event_attr attr;
	int fd;

	memset(&attr, 0, sizeof(attr));
	attr.type = PERF_TYPE_HARDWARE;
	attr.config = PERF_COUNT_HW_CPU_CYCLES;
	attr.disabled = 1;
	attr.size = sizeof(attr);

	fd = syscall(__NR_perf_event_open, &attr, -1, cpu, -1, 0);
	return fd;
}

static int perf_start_counter(int fd)
{
	int rc;

	rc = ioctl(fd, PERF_EVENT_IOC_ENABLE);
	return (rc == 0) ? FWTS_OK : FWTS_ERROR;
}

static int perf_stop_counter(int fd)
{
	int rc;

	rc = ioctl(fd, PERF_EVENT_IOC_DISABLE);
	return (rc == 0) ? FWTS_OK : FWTS_ERROR;
}

static int perf_read_counter(int fd, unsigned long long *result)
{
	unsigned long long buf;
	int rc;

	rc = read(fd, &buf, sizeof(buf));
	if (rc == sizeof(buf)) {
		*result = buf;
		rc = FWTS_OK;
	} else {
		rc = FWTS_ERROR;
	}

	(void)close(fd);
	return rc;
}

/*
 *  fwts_cpu_benchmark()
 *
 */
int fwts_cpu_benchmark(
	fwts_framework *fw,
	const int cpu,		/* CPU we want to measure performance */
	fwts_cpu_benchmark_result *result)
{
	struct timeval start, end, duration;
	unsigned long long perfctr_result;
	fwts_cpu_benchmark_result tmp;
	cpu_set_t mask, oldset;
	int perfctr, ncpus;
	double duration_sec;
	bool perf_ok;

	ncpus = fwts_cpu_enumerate();
	memset(&tmp, 0, sizeof(tmp));

	if (ncpus == FWTS_ERROR)
		return FWTS_ERROR;

	if ((cpu < 0) || (cpu > ncpus))
		return FWTS_ERROR;

	/* setup perf counter */
	perf_ok = true;
	perfctr = perf_setup_counter(cpu);
	if (perfctr < 0) {
		static bool warned;

		if (!warned) {
			fwts_log_warning(fw, "Can't use linux performance "
					"counters (perf), falling back to "
					"relative measurements");
			warned = true;
		}
		perf_ok = false;
	}

	/* Pin to the specified CPU */
	if (sched_getaffinity(0, sizeof(oldset), &oldset) < 0) {
		fwts_log_error(fw, "Cannot get scheduling affinity.");
		return FWTS_ERROR;
	}

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
		fwts_log_error(fw, "Cannot set scheduling affinity to CPU %d.", cpu);
		return FWTS_ERROR;
	}

	if (perf_ok)
		perf_start_counter(perfctr);
	gettimeofday(&start, NULL);

	/*
	 * And burn some CPU cycles and get a bogo-compute like
	 * loop count measure of CPU performance.
	 */
	for (;;) {
		fwts_cpu_burn_cycles();

		tmp.loops++;

		gettimeofday(&end, NULL);
		timersub(&end, &start, &duration);
		if (duration.tv_usec >= 250000)
			break;
	}

	if (perf_ok)
		perf_stop_counter(perfctr);

	if (sched_setaffinity(0, sizeof(oldset), &oldset) < 0) {
		fwts_log_error(fw, "Cannot restore old CPU affinity settings.");
		return FWTS_ERROR;
	}

	duration_sec = duration.tv_sec +
		((1.0 * duration.tv_usec) / 1000000.0);

	tmp.loops = (1.0 * tmp.loops) / duration_sec;

	if (perf_ok) {
		int rc = perf_read_counter(perfctr, &perfctr_result);

		if (rc == FWTS_OK) {
			tmp.cycles = (1.0 * perfctr_result) / duration_sec;
			tmp.cycles_valid = true;
		} else {
			fwts_log_warning(fw, "failed to read perf counters");
		}

	}

	*result = tmp;

	return FWTS_OK;
}

uint64_t fwts_cpu_benchmark_best_result(fwts_cpu_benchmark_result *res)
{
	return res->cycles_valid ? res->cycles : res->loops;
}

/*
 *  fwts_cpu_consume_cycles()
 *	eat up CPU cycles
 */
static uint64_t fwts_cpu_consume_cycles(void)
{
	fwts_sig_handler_set(SIGUSR1, fwts_cpu_consume_sighandler, NULL);
	volatile uint64_t i = 0;

	for (;;) {
		fwts_cpu_burn_cycles();
		i++;
	}
	return i;
}

/*
 *  fwts_cpu_consume_complete()
 *	kill all CPU consumes, free up pid info
 */
void fwts_cpu_consume_complete(void)
{
	fwts_cpu_consume_kill();
	free(fwts_cpu_pids);
}

/*
 *  fwts_cpu_consume_start()
 *	kick off per CPU tasks to eat up CPU
 */
int fwts_cpu_consume_start(void)
{
	int i;

	if ((fwts_cpu_num = fwts_cpu_enumerate()) < 0)
		return FWTS_ERROR;

	if ((fwts_cpu_pids = (pid_t*)calloc(fwts_cpu_num, sizeof(pid_t))) == NULL)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGINT, fwts_cpu_sigint_handler, NULL);
	for (i = 0; i < fwts_cpu_num; i++) {
		pid_t pid;

		pid = fork();
		switch (pid) {
		case 0: /* Child */
			(void)fwts_cpu_consume_cycles();
			break;
		case -1:
			/* Went wrong */
			fwts_cpu_consume_complete();
			return FWTS_ERROR;
		default:
			fwts_cpu_pids[i] = pid;
			break;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_cpu_consume()
 *	consume a specified amount of CPU time
 *	on all CPUs
 */
int fwts_cpu_consume(const int seconds)
{
	if (fwts_cpu_consume_start() != FWTS_OK)
		return FWTS_ERROR;

	(void)sleep(seconds);

	fwts_cpu_consume_complete();

	return FWTS_OK;
}
