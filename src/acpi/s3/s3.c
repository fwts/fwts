/*
 * Copyright (C) 2010-2024 Canonical
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
#include <getopt.h>

#include "fwts.h"
#include "fwts_pm_method.h"

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define PM_SUSPEND_PMUTILS		"pm-suspend"
#define PM_SUSPEND_HYBRID_PMUTILS	"pm-suspend-hybrid"
#define PM_SUSPEND_PATH			"/sys/power/mem_sleep"
#define PM_SUSPEND_LAST_HW_SLEEP	"/sys/power/suspend_stats/last_hw_sleep"
#define PM_SUSPEND_TOTAL_HW_SLEEP	"/sys/power/suspend_stats/total_hw_sleep"
#define WAKEUP_SOURCE_PATH		"/sys/kernel/debug/wakeup_sources"
#define INTEL_PM_S2IDLE_SLP_S0		"/sys/kernel/debug/pmc_core/slp_s0_residency_usec"
#define INTEL_PM_S0IX_WARN		"/sys/module/intel_pmc_core/parameters/warn_on_s0ix_failures"

static char sleep_type[7];
static char sleep_type_orig[7];

static bool s0ix2_warn_enable_orig = false;

static int  s3_multiple = 1;		/* number of s3 multiple tests to run */
static int  s3_min_delay = 0;		/* min time between resume and next suspend */
static int  s3_max_delay = 30;		/* max time between resume and next suspend */
static float s3_delay_delta = 0.5;	/* amount to add to delay between each S3 tests */
static int  s3_sleep_delay = 30;	/* time between start of suspend and wakeup */
static bool s3_device_check = false;	/* check for device config changes */
static char *s3_quirks = NULL;		/* Quirks to be passed to pm-suspend */
static int  s3_device_check_delay = 15;	/* Time to sleep after waking up and then running device check */
static bool s3_min_max_delay = false;
static float s3_suspend_time = 15.0;	/* Maximum allowed suspend time */
static float s3_resume_time = 15.0;	/* Maximum allowed resume time */
static bool s3_hybrid = false;
static char *s3_hook = NULL;		/* Hook to run after each S3 */
static char *s3_sleep_type = NULL;	/* The sleep type(s3 or s2idle) */
static bool s3_wakeup_src = false;	/* dump wakeup source for debug */

typedef struct {
	char		name[32];
	unsigned long	active_count;
	unsigned long	event_count;
	unsigned long	wakeup_count;
	unsigned long	expire_count;
	long int	active_since;
	long int	total_time;
	long int	max_time;
	long int	last_change;
	long int	prevent_suspend_time;
} wakeup_source;

static int read_wakeup_source(fwts_list *source)
{
	FILE		*fp;
	char		name[32];
	unsigned long	active_count;
	unsigned long	event_count;
	unsigned long	wakeup_count;
	unsigned long	expire_count;
	long int	active_since;
	long int	total_time;
	long int	max_time;
	long int	last_change;
	long int	prevent_suspend_time;
	int		c;

	fwts_list_init(source);

	if ((fp = fopen(WAKEUP_SOURCE_PATH, "r")) == NULL)
		return FWTS_ERROR;

	/* skip first line */
	while (c = fgetc(fp), c != '\n' && c != EOF);

	/* NB: important to specify the max len fscanf reads for name to avoid stack smashing */
	while (fscanf(fp, "%31s\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%ld\t\t%ld\t\t%ld\t\t%ld\t\t%ld\n",
			name, &active_count, &event_count,
			&wakeup_count, &expire_count, &active_since,
			&total_time, &max_time, &last_change,
			&prevent_suspend_time) == 10) {
		wakeup_source *wakeup_source_data;

		wakeup_source_data  = calloc(1, sizeof(wakeup_source));
		if (wakeup_source_data == NULL) {
			fwts_list_free_items(source, free);
			(void)fclose(fp);
			return FWTS_ERROR;
		}

		memcpy(wakeup_source_data->name , name, sizeof(name));
		wakeup_source_data->active_count = active_count;
		wakeup_source_data->event_count = event_count;
		wakeup_source_data->wakeup_count = wakeup_count;
		wakeup_source_data->expire_count = expire_count;
		wakeup_source_data->active_since = active_since;
		wakeup_source_data->total_time = total_time;
		wakeup_source_data->max_time = max_time;
		wakeup_source_data->last_change = last_change;
		wakeup_source_data->prevent_suspend_time = prevent_suspend_time;
	
		fwts_list_append(source, wakeup_source_data);

	}

	(void)fclose(fp);

	return FWTS_OK;
}

static void dump_wakeup_source(fwts_framework *fw, fwts_list *source)
{
	if (fwts_list_len(source) > 0) {
		fwts_list_link *item;


		fwts_list_foreach(item, source) {
			wakeup_source *wakeup_source_data =
				fwts_list_data(wakeup_source *, item);

			fwts_log_info_verbatim(fw, "%s\t%lu\t%lu\t%lu\t%lu\t%ld\t%ld\t%ld\t%ld\t%ld\n",
			wakeup_source_data->name, wakeup_source_data->active_count, wakeup_source_data->event_count,
			wakeup_source_data->wakeup_count, wakeup_source_data->expire_count, wakeup_source_data->active_since,
			wakeup_source_data->total_time, wakeup_source_data->max_time, wakeup_source_data->last_change,
			wakeup_source_data->prevent_suspend_time);
		}
	} else
		fwts_log_info(fw, "No wakeup source found.");
}

static void wakeup_source_cmp(fwts_framework *fw, fwts_list *suspend_source, fwts_list *resume_source)
{

	fwts_list_link *item1;
	fwts_list_link *item2;

	if (fwts_list_len(suspend_source) != fwts_list_len(resume_source)) {
		fwts_log_info_verbatim(fw, "wakeup source list length differ, cannot get wakeup sources.\n");
		return;
	}

	item1 = fwts_list_head(suspend_source);
	item2 = fwts_list_head(resume_source);

	while ((item1 != NULL) && (item2 != NULL)) {
		wakeup_source *data1 = fwts_list_data(wakeup_source *, item1);
		wakeup_source *data2 = fwts_list_data(wakeup_source *, item2);
		if (data1->event_count < data2->event_count) {
			fwts_log_info_verbatim(fw, "wakeup source name: \"%s\" wakeup event was signaled.\n", data1->name);
			if (s3_wakeup_src) {
				fwts_log_info_verbatim(fw, "%s %lu %lu %lu %lu %ld %ld %ld %ld %ld\n",
				data1->name, data1->active_count, data1->event_count,
				data1->wakeup_count, data1->expire_count, data1->active_since,
				data1->total_time, data1->max_time, data1->last_change,
				data1->prevent_suspend_time);

				fwts_log_info_verbatim(fw, "%s %lu %lu %lu %lu %ld %ld %ld %ld %ld\n",
				data2->name, data2->active_count, data2->event_count,
				data2->wakeup_count, data2->expire_count, data2->active_since,
				data2->total_time, data2->max_time, data2->last_change,
				data2->prevent_suspend_time);
			}
		}
		item1 = fwts_list_next(item1);
		item2 = fwts_list_next(item2);
	}
}

static int s3_init(fwts_framework *fw)
{
	char *str;

	/*
	 *  Pre-init - make sure wakealarm works so that we can
	 *  wake up after suspend
	 */
	if (fwts_wakealarm_test_firing(fw, 1) != FWTS_OK) {
		fwts_log_error(fw, "Cannot automatically wake machine up - aborting Sleep test.");
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "BadWakeAlarmSleep",
			"Check if wakealarm works reliably for Sleep tests.");
		return FWTS_ERROR;
	}

	str = fwts_get(PM_SUSPEND_PATH);
	if (str && strstr(str, "[s2idle]")) {
		strncpy(sleep_type_orig, "s2idle", strlen("s2idle") + 1);
	} else {
		strncpy(sleep_type_orig, "deep", strlen("deep") + 1);
	}
	if (str)
		free(str);

	if (s3_sleep_type) {
		fwts_log_info(fw, "Override system suspend default by '%s'\n", s3_sleep_type);
		if (fwts_set(PM_SUSPEND_PATH, s3_sleep_type) != FWTS_OK)
			fwts_log_error(fw, "Cannot set the sleep type to %s, test with default type.", s3_sleep_type);
	}

	str = fwts_get(PM_SUSPEND_PATH);
	if (str && strstr(str, "[s2idle]")) {
		strncpy(sleep_type, "s2idle", strlen("s2idle") + 1);
	} else {
		strncpy(sleep_type, "S3", strlen("S3") + 1);
	}
	if (str)
		free(str);

	str = fwts_get(INTEL_PM_S0IX_WARN);
	if (str && strstr(str, "N"))
		(void)fwts_set(INTEL_PM_S0IX_WARN, "Y");
	else if (str && strstr(str, "Y"))
		s0ix2_warn_enable_orig = true;
	if (str)
		free(str);

	return FWTS_OK;
}

static int s3_deinit(fwts_framework *fw)
{
	char *str;

	FWTS_UNUSED(fw);
	(void)fwts_set(PM_SUSPEND_PATH, sleep_type_orig);

	str = fwts_get(INTEL_PM_S0IX_WARN);
	if (str && strstr(str, "N") && s0ix2_warn_enable_orig)
		(void)fwts_set(INTEL_PM_S0IX_WARN, "Y");
	else if (str && strstr(str, "Y") && !s0ix2_warn_enable_orig)
		(void)fwts_set(INTEL_PM_S0IX_WARN, "N");
	if (str)
		free(str);

	return FWTS_OK;
}

/*
 *  s3_hook_exec()
 *	run a given hook script
 */
static int s3_hook_exec(fwts_framework *fw, char *hook)
{
	pid_t pid;
	int status, ret;

	pid = fork();
	if (pid < 0) {
		fwts_log_error(fw, "Failed to fork (to run hook script), "
			"errno=%d (%s)\n", errno, strerror(errno));
		return FWTS_ERROR;
	} else if (pid == 0) {
		/* Child */

		(void)execl(hook, "", NULL);

		/* We only get here if execl failed */
		fwts_log_error(fw, "Failed to execl '%s', "
			"errno=%d (%s)\n", hook, errno, strerror(errno));
		return FWTS_ERROR;
	}

	/* Parent */

	ret = waitpid(pid, &status, 0);
	if (ret < 0) {
		fwts_log_error(fw, "Failed waitpid on hook script, "
			"errno=%d (%s)\n", errno, strerror(errno));

		/* Nuke child to be double sure it's gone */
		(void)kill(pid, SIGKILL);
	} else if (WIFEXITED(status)) {
		fwts_log_info(fw, "Hook script '%s' returned %d\n",
			hook, WEXITSTATUS(status));
		return WEXITSTATUS(status) ? FWTS_ERROR : FWTS_OK;
	} else {
		fwts_log_error(fw, "Hook script exited abnormally\n");
	}
	return FWTS_ERROR;
}

/* Detect the best available power method */
static void detect_pm_method(fwts_pm_method_vars *fwts_settings)
{
#if FWTS_ENABLE_LOGIND
	if (s3_hybrid ?
		fwts_logind_can_hybrid_suspend(fwts_settings) :
		fwts_logind_can_suspend(fwts_settings))
		fwts_settings->fw->pm_method = FWTS_PM_LOGIND;
	else
#endif
	if (s3_hybrid ?
		fwts_sysfs_can_hybrid_suspend(fwts_settings) :
		fwts_sysfs_can_suspend(fwts_settings))
		fwts_settings->fw->pm_method = FWTS_PM_SYSFS;
	else
		fwts_settings->fw->pm_method = FWTS_PM_PMUTILS;
}

#if FWTS_ENABLE_LOGIND
static int wrap_logind_do_suspend(fwts_pm_method_vars *fwts_settings,
	const int percent,
	int *duration,
	const char *str)
{
	FWTS_UNUSED(str);
	char *action = s3_hybrid ? PM_SUSPEND_HYBRID_LOGIND : PM_SUSPEND_LOGIND;

	fwts_progress_message(fwts_settings->fw, percent, "(Suspending)");
	/* This blocks by entering a glib mainloop */
	*duration = fwts_logind_wait_for_resume_from_action(fwts_settings, action, s3_min_delay);
	fwts_log_info(fwts_settings->fw, "%s duration = %d.", sleep_type, *duration);
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	return *duration > 0 ? 0 : 1;
}
#endif

static int wrap_sysfs_do_suspend(fwts_pm_method_vars *fwts_settings,
	const int percent,
	int *duration,
	const char *str)
{
	int status;

	FWTS_UNUSED(str);
	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	fwts_progress_message(fwts_settings->fw, percent, "(Suspending)");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Starting fwts suspend\n");
	time(&(fwts_settings->t_start));
	status = fwts_sysfs_do_suspend(fwts_settings, s3_hybrid);
	(void)fwts_klog_write(fwts_settings->fw, FWTS_RESUME "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Finished fwts resume\n");
	time(&(fwts_settings->t_end));
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	*duration = (int)(fwts_settings->t_end - fwts_settings->t_start);

	return status;
}

static int wrap_pmutils_do_suspend(fwts_pm_method_vars *fwts_settings,
	const int percent,
	int *duration,
	const char *command)
{
	int status = 0;

	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	fwts_progress_message(fwts_settings->fw, percent, "(Suspending)");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Starting fwts suspend\n");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	time(&(fwts_settings->t_start));
	(void)fwts_exec(command, &status);
	(void)fwts_klog_write(fwts_settings->fw, FWTS_RESUME "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Finished fwts resume\n");
	time(&(fwts_settings->t_end));
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	*duration = (int)(fwts_settings->t_end - fwts_settings->t_start);

	return status;
}

static uint64_t get_uint64_sysfs(const char *path)
{
	uint64_t val;
	char *str;

	str = fwts_get(path);
	if (!str)
		return 0;

	val = atoll(str);
	free(str);

	return val;
}

/*
 *  get_last_s2idle_residency()
 *
 *  Returns:
 *  - Hardware sleep residency from the last sleep cycle
 *  - 0 if it is not available
 *
 */
static uint64_t get_last_s2idle_residency(void)
{
	if (access(PM_SUSPEND_LAST_HW_SLEEP, F_OK) != 0)
		return 0;

	return get_uint64_sysfs(PM_SUSPEND_LAST_HW_SLEEP);
}

/*
 *  get_total_s2idle_residency()
 *  @fname: Optional parameter to set the filename used to check residency
 *
 *  Returns:
 *  - Total hardware sleep residency since the system was booted
 *  - 0 if it is not available
 *
 */
static uint64_t get_total_s2idle_residency(const char **fname)
{
	const char *check;
	uint64_t val;

	if (access(PM_SUSPEND_TOTAL_HW_SLEEP, F_OK) == 0)
		check = PM_SUSPEND_TOTAL_HW_SLEEP;
	else
		check = INTEL_PM_S2IDLE_SLP_S0;

	val = get_uint64_sysfs(check);

	if (fname)
		*fname = check;

	return val;
}

static int s3_do_suspend_resume(fwts_framework *fw,
	int *hw_errors,
	int *pm_errors,
	int *hook_errors,
	int *s2idle_errors,
	uint64_t *total_s2idle_residency,
	int delay,
	int percent)
{
	fwts_hwinfo hwinfo1, hwinfo2;
	int status;
	int duration = 0;
	int differences;
	int rc = FWTS_OK;
	char *command = NULL;
	char *quirks = NULL;
	fwts_pm_method_vars *fwts_settings;
	fwts_list suspend_wakeup_soure;
	fwts_list resume_wakeup_soure;
	bool wk_src_found = false;

	int (*do_suspend)(fwts_pm_method_vars *, const int, int*, const char*);

	fwts_settings = calloc(1, sizeof(fwts_pm_method_vars));
	if (fwts_settings == NULL)
		return FWTS_OUT_OF_MEMORY;
	fwts_settings->fw = fw;

	if (fw->pm_method == FWTS_PM_UNDEFINED) {
		/* Autodetection */
		fwts_log_info(fw, "Detecting the power method.");
		detect_pm_method(fwts_settings);
	}

	switch (fw->pm_method) {
#if FWTS_ENABLE_LOGIND
		case FWTS_PM_LOGIND:
			fwts_log_info(fw, "Using logind as the default power method.");
			if (fwts_logind_init_proxy(fwts_settings) != 0) {
				fwts_log_error(fw, "Failure to connect to Logind.");
				rc = FWTS_ERROR;
				goto tidy;
			}
			do_suspend = &wrap_logind_do_suspend;
			break;
#endif
		case FWTS_PM_PMUTILS:
			fwts_log_info(fw, "Using pm-utils as the default power method.");
			do_suspend = &wrap_pmutils_do_suspend;
			break;
		case FWTS_PM_SYSFS:
			fwts_log_info(fw, "Using sysfs as the default power method.");
			do_suspend = &wrap_sysfs_do_suspend;
			break;
		default:
			/* This should never happen */
			fwts_log_info(fw, "Using sysfs as the default power method.");
			do_suspend = &wrap_sysfs_do_suspend;
			break;
	}

	if (s3_device_check)
		fwts_hwinfo_get(fw, &hwinfo1);

	/* Format up pm-suspend command with optional quirking arguments */
	if (fw->pm_method == FWTS_PM_PMUTILS) {
		if (s3_hybrid) {
			if ((command = fwts_realloc_strcat(NULL, PM_SUSPEND_HYBRID_PMUTILS)) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
		} else {
			if ((command = fwts_realloc_strcat(NULL, PM_SUSPEND_PMUTILS)) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
		}

		/* For now we only support quirks with pm-utils */
		if (s3_quirks) {
			if ((command = fwts_realloc_strcat(command, " ")) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
			if ((quirks = fwts_args_comma_list(s3_quirks)) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
			if ((command = fwts_realloc_strcat(command, quirks)) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
		}
	}

	fwts_wakealarm_trigger(fw, delay);

	if (read_wakeup_source(&suspend_wakeup_soure) != FWTS_ERROR) {
		wk_src_found = true;
	}

	/* Do S3 / S2idle here */
	status = do_suspend(fwts_settings, percent, &duration, command);

	if (wk_src_found) {
		if (read_wakeup_source(&resume_wakeup_soure) != FWTS_ERROR) {
			(void)wakeup_source_cmp(fw, &suspend_wakeup_soure, &resume_wakeup_soure);
			if (s3_wakeup_src) {
				(void)dump_wakeup_source(fw, &suspend_wakeup_soure);
				fwts_log_info(fw, "versus after:");
				(void)dump_wakeup_source(fw, &resume_wakeup_soure);
			}
		}
	}

	fwts_log_info(fw, "pm-action returned %d after %d seconds.", status, duration);

	if (s3_device_check) {
		int i;

		for (i = 0; i < s3_device_check_delay; i++) {
			char buffer[80];

			snprintf(buffer, sizeof(buffer), "(Waiting %d/%d seconds)", i+1,s3_device_check_delay);
			fwts_progress_message(fw, percent, buffer);
			sleep(1);
		}
		fwts_progress_message(fw, percent, "(Checking devices)");
		fwts_hwinfo_get(fw, &hwinfo2);
		fwts_hwinfo_compare(fw, &hwinfo1, &hwinfo2, &differences);
		fwts_hwinfo_free(&hwinfo1);
		fwts_hwinfo_free(&hwinfo2);

		if (differences > 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "DevConfigDiffAfterSleep",
				"Found %d differences in device configuration during %s cycle.", differences, sleep_type);
			(*hw_errors)++;
		}
	}

	if (s3_hook && (s3_hook_exec(fw, s3_hook) != FWTS_OK)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "HookScriptFailed",
			"Error executing hook script '%s', %s cycles "
			"will be aborted.", s3_hook, sleep_type);
		(*hook_errors)++;
	}

	if (!strncmp(sleep_type, "s2idle", strlen("s2idle"))) {
		const char *fname;
		uint64_t residency = get_total_s2idle_residency(&fname);

		if (residency <= *total_s2idle_residency) {
			(*s2idle_errors)++;
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "S2idleNotDeepest",
				    "Expected %s to increase from %" PRIu64 ", got %" PRIu64 ".",
				    fname, *total_s2idle_residency, residency);
		}
		*total_s2idle_residency = residency;

		residency = get_last_s2idle_residency();
		if (duration > 10 && residency) {
			float pct = (float)residency / ((float)duration * 1000000) * 100.0;
			if (pct < 70) {
				(*s2idle_errors)++;
				fwts_failed(fw, LOG_LEVEL_HIGH, "S2idleNotDeepest",
					    "Expected %s to be at least 70%% of the last sleep cycle, got %.2f%%.",
					    PM_SUSPEND_LAST_HW_SLEEP, pct);
				} else
					fwts_log_info(fw, "Spent %.2f%% of %ds in hardware sleep state", pct, duration);
		}
	}

	if (duration < delay) {
		(*pm_errors)++;
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ShortSuspend",
			"Unexpected: %s slept for %d seconds, less than the expected %d seconds.", sleep_type, duration, delay);
	}
	fwts_progress_message(fw, percent, "(Checking for errors)");
	if (duration > (delay*2)) {
		int s3_C1E_enabled;
		(*pm_errors)++;
		fwts_failed(fw, LOG_LEVEL_HIGH, "LongSuspend",
			"Unexpected: %s much longer than expected (%d seconds).", sleep_type, duration);

		s3_C1E_enabled = fwts_cpu_has_c1e();
		if (s3_C1E_enabled == -1)
			fwts_log_error(fw, "Cannot read C1E bit\n");
		else if (s3_C1E_enabled == 1)
			fwts_advice(fw,
				"Detected AMD with C1E enabled. The AMD C1E idle wait can sometimes "
				"produce long delays on resume.  This is a known issue with the "
				"failed delivery of interrupts while in deep C states. "
				"If you have a BIOS option to disable C1E please disable this and retry. "
				"Alternatively, re-test with the kernel parameter \"idle=mwait\". ");
	}

	/* Add in error check for pm-suspend status */
	if ((status > 0) && (status < 128)) {
		(*pm_errors)++;
		fwts_failed(fw, LOG_LEVEL_HIGH, "PMActionFailedPreSleep",
			"pm-action failed before trying to put the system "
			"in the requested power saving state.");
	} else if (status == 128) {
		(*pm_errors)++;
		fwts_failed(fw, LOG_LEVEL_HIGH, "PMActionPowerStateSleep",
			"pm-action tried to put the machine in the requested "
			"power state but failed.");
	} else if (status > 128) {
		(*pm_errors)++;
		fwts_failed(fw, LOG_LEVEL_HIGH, "PMActionFailedSleep",
			"pm-action encountered an error and also failed to "
			"enter the requested power saving state.");
	}

	fwts_list_free_items(&suspend_wakeup_soure, free);
	fwts_list_free_items(&resume_wakeup_soure, free);

tidy:
	free(command);
	free(quirks);
	free_pm_method_vars(fwts_settings);

	return rc;
}

static int s3_scan_times(
	fwts_framework *fw,
	fwts_list *klog,
	int *suspend_too_long,
	int *resume_too_long)
{
	fwts_list_link *item;

	double s3_suspend_start = -1.0, s3_suspend_finish = -1.0;
	double s3_resume_start = -1.0, s3_resume_finish = -1.0;
	double previous_ts = -1.0, ts = 0.0;

	fwts_list_foreach(item, klog) {
		char *txt = (char *)item->data;
		char *bracket = strchr(txt, '[');
		size_t len = strlen(txt);

		if (len < 15 || bracket == NULL)
			continue;

		previous_ts = ts;

		/* Get log time stamp */
		sscanf(bracket + 1, "%lf", &ts);
		if (strstr(txt, FWTS_SUSPEND) ||
		    strstr(txt, "Starting fwts suspend")) {
			s3_suspend_start = ts;
			continue;
		}

		/* Update log time if this is available */
		if (strstr(txt, "PM: suspend entry")) {
			s3_suspend_start = ts;
			continue;
		}

		if (strstr(txt, FWTS_RESUME)) {
			s3_resume_finish = ts;
			continue;
		}
		/* This may be the last message we see from the kernel */
		if (strstr(txt, "PM: Saving platform NVS memory")) {
			s3_suspend_finish = ts;
			continue;
		}
		/* And this may appear even later */
		if (strstr(txt, "smpboot: CPU") && strstr(txt, "is now offline")) {
			s3_suspend_finish = ts;
			continue;
		}

		if (strstr(txt, "ACPI: Low-level resume complete")) {
			s3_resume_start = ts;
			if (s3_suspend_finish < 0.0)
				s3_suspend_finish = previous_ts;
			continue;
		}
		if (s3_resume_start < 0.0 && strstr(txt, "ACPI: Waking up from system sleep state S3")) {
			s3_resume_start = ts;
			if (s3_suspend_finish < 0.0)
				s3_suspend_finish = previous_ts;
			continue;
		}

		/* get log time for s2idle */
		if (strstr(txt, "PM: suspend-to-idle")) {
			s3_suspend_finish = ts;
			continue;
		}
		if (strstr(txt, "Timekeeping suspended") || strstr(txt, "resume from suspend-to-idle")) {
			s3_resume_start = ts;
			if (s3_suspend_finish < 0.0)
				s3_suspend_finish = previous_ts;
			continue;
		}

		if (strstr(txt, "Restarting tasks ... done")) {
			s3_resume_finish = ts;
			break;
		}
	}

	fwts_log_info(fw, "Suspend/Resume Timings:");
	if (s3_suspend_start > 0.0 && s3_suspend_finish > 0.0) {
		fwts_log_info_verbatim(fw, "  Suspend: %.3f seconds.",
			s3_suspend_finish - s3_suspend_start);
		if (s3_suspend_finish - s3_suspend_start > s3_suspend_time)
			(*suspend_too_long)++;
	} else
		fwts_log_info_verbatim(fw, "  Could not determine time to suspend.");

	if (s3_resume_start > 0.0 && s3_resume_finish > 0.0) {
		fwts_log_info_verbatim(fw, "  Resume:  %.3f seconds.",
			s3_resume_finish - s3_resume_start);
		if (s3_resume_finish - s3_resume_start > s3_resume_time)
			(*resume_too_long)++;
	} else
		fwts_log_info_verbatim(fw, "  Could not determine time to resume.");

	return FWTS_OK;
}

static int s3_check_log(
	fwts_framework *fw,
	fwts_list *klog,
	int *errors,
	int *oopses,
	int *warn_ons,
	int *suspend_too_long,
	int *resume_too_long)
{
	int error = 0;
	int oops;
	int warn_on;

	if (fwts_klog_pm_check(fw, NULL, klog, &error))
		fwts_log_error(fw, "Error parsing kernel log.");
	*errors += error;

	if (fwts_klog_firmware_check(fw, NULL, klog, &error))
		fwts_log_error(fw, "Error parsing kernel log.");
	*errors += error;

	if (fwts_oops_check(fw, klog, &oops, &warn_on))
		fwts_log_error(fw, "Error parsing kernel log.");

	*oopses += oops;
	*warn_ons += warn_on;

	s3_scan_times(fw, klog, suspend_too_long, resume_too_long);

	return FWTS_OK;
}

static int s3_test_multiple(fwts_framework *fw)
{
	int i;
	int klog_errors = 0;
	int hw_errors = 0;
	int pm_errors = 0;
	int hook_errors = 0;
	int klog_oopses = 0;
	int klog_warn_ons = 0;
	int s2idle_errors = 0;
	int suspend_too_long = 0;
	int resume_too_long = 0;
	int awake_delay = s3_min_delay * 1000;
	int delta = (int)(s3_delay_delta * 1000.0);
	uint64_t total_s2idle_residency = get_total_s2idle_residency(NULL);
	int pm_debug;

#if FWTS_ENABLE_LOGIND
#if !GLIB_CHECK_VERSION(2,35,0)
	/* This is for backward compatibility with old glib versions */
	g_type_init();
#endif
#endif

	(void)fwts_pm_debug_get(&pm_debug);
	(void)fwts_pm_debug_set(1);

	if (s3_multiple == 1)
		fwts_log_info(fw, "Defaulted to 1 test, use --s3-multiple=N to run more %s cycles\n", sleep_type);

	for (i = 0; i < s3_multiple; i++) {
		struct timeval tv;
		int ret, percent = (i * 100) / s3_multiple;
		fwts_list *klog_pre, *klog_post, *klog_diff;
		fwts_log_info(fw, "%s cycle %d of %d\n", sleep_type, i+1, s3_multiple);

		if ((klog_pre = fwts_klog_read()) == NULL)
			fwts_log_error(fw, "Cannot read kernel log.");

		ret = s3_do_suspend_resume(fw, &hw_errors, &pm_errors, &hook_errors,
					   &s2idle_errors, &total_s2idle_residency,
					   s3_sleep_delay, percent);
		if (ret == FWTS_OUT_OF_MEMORY) {
			fwts_log_error(fw, "%s cycle %d failed - out of memory error.", sleep_type, i+1);
			fwts_klog_free(klog_pre);
			break;
		}
		if (hook_errors > 0) {
			fwts_klog_free(klog_pre);
			break;
		}

		if ((klog_post = fwts_klog_read()) == NULL)
			fwts_log_error(fw, "Cannot re-read kernel log.");

		fwts_progress_message(fw, percent, "(Checking logs for errors)");
		klog_diff = fwts_klog_find_changes(klog_pre, klog_post);
		if (klog_diff)
			s3_check_log(fw, klog_diff, &klog_errors, &klog_oopses, &klog_warn_ons,
				&suspend_too_long, &resume_too_long);

		fwts_klog_free(klog_pre);
		fwts_klog_free(klog_post);
		fwts_list_free(klog_diff, NULL);

		if (!s3_device_check) {
			char buffer[80];
			int j;

			tv.tv_sec  = 0;
			tv.tv_usec = (awake_delay % 1000)*1000;
			select(0, NULL, NULL, NULL, &tv);

			for (j = 0; j < awake_delay / 1000; j++) {
				snprintf(buffer, sizeof(buffer), "(Waiting %d/%d seconds)",
					j + 1, awake_delay / 1000);
				fwts_progress_message(fw, percent, buffer);
				sleep(1);
			}

			awake_delay += delta;
			if (awake_delay > (s3_max_delay * 1000))
				awake_delay = s3_min_delay * 1000;
		}
	}

	/* Restore pm debug value */
	if (pm_debug != -1)
		(void)fwts_pm_debug_set(pm_debug);

	fwts_log_info(fw, "Completed %s cycle(s)\n", sleep_type);

	if (klog_errors > 0)
		fwts_log_info(fw, "Found %d errors in kernel log.", klog_errors);
	else
		fwts_passed(fw, "No kernel log errors detected.");

	if (pm_errors > 0)
		fwts_log_info(fw, "Found %d PM related suspend issues.", pm_errors);
	else
		fwts_passed(fw, "No PM related suspend issues detected.");

	if (hw_errors > 0)
		fwts_log_info(fw, "Found %d device errors.", hw_errors);
	else
		fwts_passed(fw, "No device errors detected.");

	if (klog_oopses > 0)
		fwts_log_info(fw, "Found %d kernel oopses.", klog_oopses);
	else
		fwts_passed(fw, "No kernel oopses detected.");

	if (klog_warn_ons > 0)
		fwts_log_info(fw, "Found %d kernel WARN_ON warnings.", klog_warn_ons);
	else
		fwts_passed(fw, "No kernel WARN_ON warnings detected.");

	if (s2idle_errors > 0)
		fwts_log_info(fw, "Found %d s2idle errors.", s2idle_errors);
	else
		fwts_passed(fw, "No s2idle errors detected.");

	if ((klog_errors + pm_errors + hw_errors + klog_oopses) > 0) {
		fwts_log_info(fw, "Found %d errors and %d oopses doing %d suspend/resume cycle(s).",
			klog_errors + pm_errors + hw_errors, klog_oopses, s3_multiple);
	} else
		fwts_passed(fw, "Found no errors doing %d suspend/resume cycle(s).", s3_multiple);

	if (suspend_too_long > 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SuspendTooLong",
			"%d suspend%s took longer than %.2f seconds.",
			suspend_too_long, resume_too_long == 1 ? "" : "s",
			s3_suspend_time);
	else
		fwts_passed(fw, "All suspends took less than %.2f seconds.",
			s3_suspend_time);

	if (resume_too_long > 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ResumeTooLong",
			"%d resume%s took longer than %.2f seconds.",
			resume_too_long, resume_too_long == 1 ? "" : "s",
			s3_resume_time);
	else
		fwts_passed(fw, "All resumes took less than %.2f seconds.",
			s3_resume_time);

	return FWTS_OK;
}

static int s3_options_check(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if ((s3_multiple < 0) || (s3_multiple > 100000)) {
		fprintf(stderr, "--s3-multiple is %d, it should be 1..100000\n", s3_multiple);
		return FWTS_ERROR;
	}
	if ((s3_min_delay < 0) || (s3_min_delay > 3600)) {
		fprintf(stderr, "--s3-min-delay is %d, it cannot be less than zero or more than 1 hour!\n", s3_min_delay);
		return FWTS_ERROR;
	}
	if (s3_max_delay < s3_min_delay || s3_max_delay > 3600)  {
		fprintf(stderr, "--s3-max-delay is %d, it cannot be less than --s3-min-delay or more than 1 hour!\n", s3_max_delay);
		return FWTS_ERROR;
	}
	if (s3_delay_delta <= 0.001) {
		fprintf(stderr, "--s3-delay-delta is %f, it cannot be less than 0.001\n", s3_delay_delta);
		return FWTS_ERROR;
	}
	if ((s3_sleep_delay < 5) || (s3_sleep_delay > 3600)) {
		fprintf(stderr, "--s3-sleep-delay is %d, it cannot be less than 5 seconds or more than 1 hour!\n", s3_sleep_delay);
		return FWTS_ERROR;
	}
	if ((s3_device_check_delay < 1) || (s3_device_check_delay > 3600)) {
		fprintf(stderr, "--s3-device-check-delay is %d, it cannot be less than 1 second or more than 1 hour!\n", s3_device_check_delay);
		return FWTS_ERROR;
	}
	if (s3_min_max_delay & s3_device_check) {
		fprintf(stderr, "Cannot use --s3-min-delay, --s3-max-delay, --s3-delay-delta as well as --s3-device-check, --s3-device-check-delay.\n");
		return FWTS_ERROR;
	}
	if (s3_suspend_time < 0.0) {
		fprintf(stderr, "--s3-suspend-time too small.\n");
		return FWTS_ERROR;
	}
	if (s3_resume_time < 0.0) {
		fprintf(stderr, "--s3-resume-time too small.\n");
		return FWTS_ERROR;
	}
	if (s3_hook) {
		struct stat statbuf;
		int ret;

		ret = lstat(s3_hook, &statbuf);
		if (ret < 0) {
			fprintf(stderr, "--s3-resume-hook file '%s' cannot "
				"be checked, errno=%d (%s)\n",
				s3_hook, errno, strerror(errno));
			return FWTS_ERROR;
		}
		if ((statbuf.st_mode & S_IXUSR) == 0) {
			fprintf(stderr, "--s3-resume-hook file '%s' is not "
				"executable\n", s3_hook);
			return FWTS_ERROR;
		}
	}
	return FWTS_OK;
}

static int s3_options_handler(fwts_framework *fw, int argc, char * const argv[], int option_char, int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);

	switch (option_char) {
	case 0:
		switch (long_index) {
		case 0:
			s3_multiple = atoi(optarg);
			break;
		case 1:
			s3_min_delay = atoi(optarg);
			s3_min_max_delay = true;
			break;
		case 2:
			s3_max_delay = atoi(optarg);
			s3_min_max_delay = true;
			break;
		case 3:
			s3_delay_delta = atof(optarg);
			s3_min_max_delay = true;
			break;
		case 4:
			s3_sleep_delay = atoi(optarg);
			break;
		case 5:
			s3_device_check = true;
			break;
		case 6:
			s3_quirks = optarg;
			break;
		case 7:
			s3_device_check_delay = atoi(optarg);
			s3_device_check = true;
			break;
		case 8:
			s3_suspend_time = atof(optarg);
			break;
		case 9:
			s3_resume_time = atof(optarg);
			break;
		case 10:
			s3_hybrid = true;
			break;
		case 11:
			s3_hook = optarg;
			break;
		case 12:
			s3_sleep_type = optarg;
			break;
		case 13:
			s3_wakeup_src = true;
			break;
		}
	}
	return FWTS_OK;
}

static fwts_option s3_options[] = {
	{ "s3-multiple", 	"", 1, "Run S3 tests multiple times, e.g. --s3-multiple=10." },
	{ "s3-min-delay", 	"", 1, "Minimum time between S3 iterations, e.g. --s3-min-delay=10" },
	{ "s3-max-delay", 	"", 1, "Maximum time between S3 iterations, e.g. --s3-max-delay=20" },
	{ "s3-delay-delta", 	"", 1, "Time to be added to delay between S3 iterations. Used in conjunction with --s3-min-delay and --s3-max-delay, e.g. --s3-delay-delta=2.5" },
	{ "s3-sleep-delay",	"", 1, "Sleep N seconds between start of suspend and wakeup, e.g. --s3-sleep-delay=60" },
	{ "s3-device-check",	"", 0, "Check differences between device configurations over a S3 cycle. Note we add a default of 15 seconds to allow wifi to re-associate.  Cannot be used with --s3-min-delay, --s3-max-delay and --s3-delay-delta." },
	{ "s3-quirks",		"", 1, "Comma separated list of quirk arguments to pass to pm-suspend." },
	{ "s3-device-check-delay", "", 1, "Sleep N seconds before we run a device check after waking up from suspend. Default is 15 seconds, e.g. --s3-device-check-delay=20" },
	{ "s3-suspend-time",	"", 1, "Maximum expected suspend time in seconds, e.g. --s3-suspend-time=3.5" },
	{ "s3-resume-time", 	"", 1, "Maximum expected resume time in seconds, e.g. --s3-resume-time=5.1" },
	{ "s3-hybrid",		"", 0, "Run S3 with hybrid sleep, i.e. saving system states as S4 does." },
	{ "s3-resume-hook hook","", 1, "Run a hook script after each S3 resume, 0 exit indicates success." },
	{ "s3-sleep-type",	"", 1, "Set the sleep type for testing S3 or s2idle." },
	{ "s3-dump-wakeup-src",	"", 0, "dump the all device wakeup sources suspend/resume.(For debug)"}, 
	{ NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test s3_tests[] = {
	{ s3_test_multiple, "Sleep suspend/resume test." },
	{ NULL, NULL }
};

static fwts_framework_ops s3_ops = {
	.description = "Sleep suspend/resume test.",
	.init        = s3_init,
	.deinit      = s3_deinit,
	.minor_tests = s3_tests,
	.options     = s3_options,
	.options_handler = s3_options_handler,
	.options_check = s3_options_check,
};

FWTS_REGISTER("s3", &s3_ops, FWTS_TEST_LATE, FWTS_FLAG_POWER_STATES | FWTS_FLAG_ROOT_PRIV)

#endif
