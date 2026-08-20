/*
 * Copyright (C) 2011-2026 Canonical
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
#include <getopt.h>

#include "fwts.h"
#include "fwts_pm_method.h"

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#define PM_SUSPEND "pm-suspend"

static int 	s3power_sleep_delay = 600;	/* time between start of suspend and wakeup */
static uint32_t battery_capacity_mAh;
static uint32_t battery_capacity_mWh;

static int s3power_adapter_offline(fwts_framework *fw, bool *offline)
{
	int matching = 0;
	int not_matching = 0;

	if (fwts_ac_adapter_get_state(FWTS_AC_ADAPTER_ONLINE, &matching, &not_matching) != FWTS_OK) {
		fwts_log_error(fw, "Cannot detect if running on AC or battery.");
		return FWTS_ERROR;
	}

	/* Any online, then we're not totally offlined */

	*offline = matching > 0 ? false : true;

	return FWTS_OK;
}

static int s3power_wait_for_adapter_offline(fwts_framework *fw, bool *offline)
{
	int fd;
	size_t len;
	int i;

	if (s3power_adapter_offline(fw, offline) == FWTS_ERROR)
		return FWTS_ERROR;
	if (*offline)
		return FWTS_OK;	/* Already offline, so no need to wait */

	if ((fd = fwts_acpi_event_open()) < 0) {
		fwts_log_error(fw, "Cannot connect to acpid.");
		return FWTS_ERROR;
	}

	fwts_printf(fw, "==== Please unplug the laptop power. ====\n");

	for (i = 0; (i <= 20) && !*offline; i++) {
		char *buffer;

		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL) {
			if (strstr(buffer, "ac_adapter") != NULL)
				s3power_adapter_offline(fw, offline);
			free(buffer);
		}
		fwts_printf(fw, "Waiting %2.2d/20\r", 20-i);
	}
	fwts_acpi_event_close(fd);

	return FWTS_OK;
}

static int s3power_get_design_capacity(fwts_framework *fw,
	uint32_t *capacity_mAh, uint32_t *capacity_mWh)
{
	return fwts_battery_get_capacity(fw, FWTS_BATTERY_DESIGN_CAPACITY, FWTS_BATTERY_ALL, capacity_mAh, capacity_mWh);
}

static int s3power_get_remaining_capacity(fwts_framework *fw,
	uint32_t *capacity_mAh, uint32_t *capacity_mWh)
{
	return fwts_battery_get_capacity(fw, FWTS_BATTERY_REMAINING_CAPACITY, FWTS_BATTERY_ALL, capacity_mAh, capacity_mWh);
}

static int s3power_init(fwts_framework *fw)
{
	if (fwts_wakealarm_test_firing(fw, 1) != FWTS_OK) {
		fwts_log_error(fw, "Cannot automatically wake machine up - aborting S3power test.");
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "BadWakeAlarmS3Power",
			"Wakealarm does not work reliably for s3power test.");
		return FWTS_ABORTED;
	}

	if (s3power_get_design_capacity(fw, &battery_capacity_mAh, &battery_capacity_mWh) != FWTS_OK) {
		fwts_log_error(fw, "Cannot determine battery capacity, aborting test.");
		return FWTS_ABORTED;
	}

	return FWTS_OK;
}

/* Detect the best available power method */
static void detect_pm_method(fwts_pm_method_vars *fwts_settings)
{
#if FWTS_ENABLE_LOGIND
	if (fwts_logind_can_suspend(fwts_settings))
		fwts_settings->fw->pm_method = FWTS_PM_LOGIND;
	else
#endif
	if (fwts_sysfs_can_suspend(fwts_settings))
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

	fwts_progress_message(fwts_settings->fw, percent, "(Suspending)");
	/* This blocks by entering a glib mainloop */
	*duration = fwts_logind_wait_for_resume_from_action(fwts_settings, PM_SUSPEND_LOGIND, 0);
	fwts_log_info(fwts_settings->fw, "S3 duration = %d.", *duration);
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
	fwts_progress_message(fwts_settings->fw, percent, "(Suspending)");
	time(&(fwts_settings->t_start));
	(void)fwts_klog_write(fwts_settings->fw, "Starting fwts suspend\n");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	status = fwts_sysfs_do_suspend(fwts_settings, false);
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
	int status = FWTS_OK;

	fwts_progress_message(fwts_settings->fw, percent, "(Suspending)");
	time(&(fwts_settings->t_start));
	(void)fwts_klog_write(fwts_settings->fw, "Starting fwts suspend\n");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_SUSPEND "\n");
	(void)fwts_exec(command, &status);
	(void)fwts_klog_write(fwts_settings->fw, FWTS_RESUME "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Finished fwts resume\n");
	time(&(fwts_settings->t_end));
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	*duration = (int)(fwts_settings->t_end - fwts_settings->t_start);

	return status;
}

static void s3power_difference(fwts_framework *fw,
	uint32_t before, uint32_t after,
	uint32_t battery_capacity, char *units)
{
	int32_t diff = before - after;

	if (before != 0) {
		float hourly_loss;

		fwts_log_info(fw, "Change in capacity: %" PRId32 " %s\n", diff, units);
		hourly_loss = ((float)diff * 3600.0) / (float)s3power_sleep_delay;
		if (diff < 0) {
			fwts_log_error(fw, "Negative loss of power, are you sure the machine was not charging?");
		}
		fwts_log_info(fw, "Loss of %7.4f %s per hour.", hourly_loss, units);
		if ((diff > 0) && (battery_capacity > 0)) {
			float duration = (float)battery_capacity / hourly_loss;
			fwts_log_info(fw, "The %" PRIu32 " %s battery will provide %5.2f hours of suspend time.",
				battery_capacity, units, duration);

			if (duration < 24.0) {
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"ShortSuspendLife24hrs",
					"Machine cannot remain suspended for 1 day.");
			} else if (duration < 36.0) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"ShortSuspendLife36hrs",
					"Machine cannot remain suspended for 1.5 days.");
			} else if (duration < 48.0) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"ShortSuspendLife48hrs",
					"Machine cannot remain suspended for 2 days.");
			} else if (duration < 60.0) {
				fwts_failed(fw, LOG_LEVEL_LOW,
					"ShortSuspendLife60hrs",
					"Machine cannot remain suspended for 2.5 days.");
			} else {
				fwts_passed(fw, "Machine can remain suspended for %5.2f hours.", duration);
			}

			fwts_log_info(fw, "Note: Accuracy of results are increased with longer sleep delay durations.");
		}
	}
}

static int s3power_test(fwts_framework *fw)
{
	int status;
	int duration;
	int rc = FWTS_OK;
	int pm_debug;

	bool offline;

	uint32_t capacity_before_mAh;
	uint32_t capacity_after_mAh;
	uint32_t capacity_before_mWh;
	uint32_t capacity_after_mWh;

	fwts_pm_method_vars *fwts_settings;

	int (*do_suspend)(fwts_pm_method_vars *, const int, int*, const char*);

#if FWTS_ENABLE_LOGIND
#if !GLIB_CHECK_VERSION(2,35,0)
	/* This is for backward compatibility with old glib versions */
	g_type_init();
#endif
#endif

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

	if (s3power_wait_for_adapter_offline(fw, &offline) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot check if machine is running on battery, aborting test.");
		rc = FWTS_ABORTED;
		goto tidy;
	}
	if (!offline) {
		fwts_log_error(fw, "Machine needs to be running on battery to run test, aborting test.");
		rc = FWTS_ABORTED;
		goto tidy;
	}

	s3power_get_remaining_capacity(fw, &capacity_before_mAh, &capacity_before_mWh);

	fwts_wakealarm_trigger(fw, s3power_sleep_delay);

	(void)fwts_pm_debug_get(&pm_debug);
	(void)fwts_pm_debug_set(1);

	/* Do S3 here */
	status = do_suspend(fwts_settings, 100, &duration, PM_SUSPEND);

	/* Restore pm debug value */
	if (pm_debug != -1)
		(void)fwts_pm_debug_set(pm_debug);

	s3power_get_remaining_capacity(fw, &capacity_after_mAh, &capacity_after_mWh);

	s3power_difference(fw, capacity_before_mAh, capacity_after_mAh, battery_capacity_mAh, "mAh");
	s3power_difference(fw, capacity_before_mWh, capacity_after_mWh, battery_capacity_mWh, "mWh");

	fwts_log_info(fw, "pm-suspend returned %d after %d seconds.", status, duration);

	if (duration < s3power_sleep_delay)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ShortSuspend",
			"Unexpected: S3 slept for %d seconds, less than the expected %d seconds.",
			duration, s3power_sleep_delay);
	if (duration > (s3power_sleep_delay*2))
		fwts_failed(fw, LOG_LEVEL_HIGH, "LongSuspend",
			"Unexpected: S3 much longer than expected (%d seconds).", duration);

	/* Add in error check for pm-suspend status */
	if ((status > 0) && (status < 128)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "PMActionFailedPreS3",
			"pm-action failed before trying to put the system "
			"in the requested power saving state.");
	} else if (status == 128) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "PMActionPowerStateS3",
			"pm-action tried to put the machine in the requested "
			"power state but failed.");
	} else if (status > 128) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "PMActionFailedS3",
			"pm-action encountered an error and also failed to "
			"enter the requested power saving state.");
	}
tidy:
	free_pm_method_vars(fwts_settings);

	return rc;
}

static int s3power_options_check(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if ((s3power_sleep_delay < 600) || (s3power_sleep_delay > 24*60*60)) {
		fprintf(stderr, "--s3power-sleep-delay is %d, it cannot be less than 600 seconds or more than 24 hours!\n", s3power_sleep_delay);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int s3power_options_handler(fwts_framework *fw, int argc, char * const argv[], int option_char, int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);

        switch (option_char) {
        case 0:
                switch (long_index) {
		case 0:
			s3power_sleep_delay = atoi(optarg);
			break;
		}
	}
	return FWTS_OK;
}

static fwts_option s3power_options[] = {
	{ "s3power-sleep-delay","", 1, "Sleep N seconds between start of suspend and wakeup, e.g. --s3power-sleep-delay=600" },
	{ NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test s3power_tests[] = {
	{ s3power_test, "S3 power loss during suspend test." },
	{ NULL, NULL }
};

static fwts_framework_ops s3power_ops = {
	.description = "S3 power loss during suspend test (takes minimum of 10 minutes to run).",
	.init        = s3power_init,
	.minor_tests = s3power_tests,
	.options     = s3power_options,
	.options_handler = s3power_options_handler,
	.options_check = s3power_options_check,
};

FWTS_REGISTER("s3power", &s3power_ops, FWTS_TEST_LATE, FWTS_FLAG_POWER_STATES | FWTS_FLAG_ROOT_PRIV)

#endif
