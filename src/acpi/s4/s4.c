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
#include "fwts_pm_method.h"

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PM_HIBERNATE	"pm-hibernate"

#define FWTS_TRACING_BUFFER_SIZE	"/sys/kernel/debug/tracing/buffer_size_kb"

static int  s4_multiple = 1;		/* number of S4 multiple tests to run */
static int  s4_min_delay = 0;		/* min time between resume and next hibernate */
static int  s4_max_delay = 30;		/* max time between resume and next hibernate */
static float s4_delay_delta = 0.5;	/* amount to add to delay between each S4 tests */
static int  s4_sleep_delay = 90;	/* delay between initiating S4 and wakeup */
static bool s4_device_check = false;	/* check for device config changes */
static char *s4_quirks = NULL;		/* Quirks to be passed to pm-hibernate */
static int  s4_device_check_delay = 15;	/* Time to sleep after waking up and then running device check */
static bool s4_min_max_delay = false;

static int s4_init(fwts_framework *fw)
{
	fwts_list* swap_devs;

	swap_devs = fwts_file_open_and_read("/proc/swaps");
	if ((fwts_text_list_strstr(swap_devs, "/dev/") == NULL) &&
	    (fwts_text_list_strstr(swap_devs, "file") == NULL)) {
		fwts_list_free(swap_devs, free);
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoSwap",
			"Cannot run hibernate test - machine appears to have NO swap.");
		return FWTS_ERROR;
	}
	fwts_list_free(swap_devs, free);

	if (fwts_wakealarm_test_firing(fw, 1)) {
		fwts_log_error(fw, "Cannot automatically wake machine up - aborting S4 test.");
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "BadWakeAlarmS4",
			"Check if wakealarm works reliably for S4 tests.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static void s4_check_log(fwts_framework *fw,
	fwts_list *klog, int *errors, int *oopses, int *warn_ons)
{
	int error = 0;
	int oops;
	int warn_on;

	/* Check for kernel errors reported in the log */
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
}

/* Detect the best power method available */
static void detect_pm_method(fwts_pm_method_vars *fwts_settings)
{
#if FWTS_ENABLE_LOGIND
	if (fwts_logind_can_hibernate(fwts_settings))
		fwts_settings->fw->pm_method = FWTS_PM_LOGIND;
	else
#endif
	if (fwts_sysfs_can_hibernate(fwts_settings))
		fwts_settings->fw->pm_method = FWTS_PM_SYSFS;
	else
		fwts_settings->fw->pm_method = FWTS_PM_PMUTILS;
}

#if FWTS_ENABLE_LOGIND
static int wrap_logind_do_s4(fwts_pm_method_vars *fwts_settings,
	const int percent,
	int *duration,
	const char *str)
{
	FWTS_UNUSED(str);
	char *action = PM_HIBERNATE_LOGIND;
	fwts_progress_message(fwts_settings->fw, percent, "(Hibernating)");

	/* This blocks by entering a glib mainloop */
	*duration = fwts_logind_wait_for_resume_from_action(fwts_settings, action, s4_min_delay);
	fwts_log_info(fwts_settings->fw, "S4 duration = %d.", *duration);
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	return *duration > 0 ? 0 : 1;
}
#endif

static int wrap_sysfs_do_s4(fwts_pm_method_vars *fwts_settings,
	const int percent,
	int *duration,
	const char *str)
{
	int status;

	FWTS_UNUSED(str);
	fwts_progress_message(fwts_settings->fw, percent, "(Hibernating)");
	time(&(fwts_settings->t_start));
	(void)fwts_klog_write(fwts_settings->fw, "Starting fwts hibernate\n");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_HIBERNATE "\n");
	status = fwts_sysfs_do_hibernate(fwts_settings);
	(void)fwts_klog_write(fwts_settings->fw, FWTS_RESUME "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Finished fwts resume\n");
	time(&(fwts_settings->t_end));
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	*duration = (int)(fwts_settings->t_end - fwts_settings->t_start);

	return status;
}

static int wrap_pmutils_do_s4(fwts_pm_method_vars *fwts_settings,
	const int percent,
	int *duration,
	const char *command)
{
	int status = 0;

	fwts_progress_message(fwts_settings->fw, percent, "(Hibernating)");
	time(&(fwts_settings->t_start));
	(void)fwts_klog_write(fwts_settings->fw, "Starting fwts hibernate\n");
	(void)fwts_klog_write(fwts_settings->fw, FWTS_HIBERNATE "\n");
	(void)fwts_exec(command, &status);
	(void)fwts_klog_write(fwts_settings->fw, FWTS_RESUME "\n");
	(void)fwts_klog_write(fwts_settings->fw, "Finished fwts resume\n");
	time(&(fwts_settings->t_end));
	fwts_progress_message(fwts_settings->fw, percent, "(Resumed)");

	*duration = (int)(fwts_settings->t_end - fwts_settings->t_start);

	return status;
}

static int s4_hibernate(fwts_framework *fw,
	int *klog_errors,
	int *hw_errors,
	int *pm_errors,
	int *klog_oopses,
	int *klog_warn_ons,
	int *failed_alloc_image,
	int percent)
{
	fwts_list *klog_pre, *klog_post, *klog_diff;
	fwts_hwinfo hwinfo1, hwinfo2;
	int status;
	int duration;
	int differences;
	int rc = FWTS_OK;
	char *command = NULL;
	char *quirks = NULL;
	fwts_pm_method_vars *fwts_settings;

	int (*do_s4)(fwts_pm_method_vars *, const int, int*, const char*);

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
			do_s4 = &wrap_logind_do_s4;
			break;
#endif
		case FWTS_PM_PMUTILS:
			fwts_log_info(fw, "Using pm-utils as the default power method.");
			do_s4 = &wrap_pmutils_do_s4;
			break;
		case FWTS_PM_SYSFS:
			fwts_log_info(fw, "Using sysfs as the default power method.");
			do_s4 = &wrap_sysfs_do_s4;
			break;
		default:
			/* This should never happen */
			fwts_log_info(fw, "Using sysfs as the default power method.");
			do_s4 = &wrap_sysfs_do_s4;
			break;
	}

	if (s4_device_check)
		fwts_hwinfo_get(fw, &hwinfo1);

	if (fw->pm_method == FWTS_PM_PMUTILS) {
		/* Format up pm-hibernate command with optional quirking arguments */
		if ((command = fwts_realloc_strcat(NULL, PM_HIBERNATE)) == NULL) {
			rc = FWTS_OUT_OF_MEMORY;
			goto tidy;
		}

		/* For now we only support quirks with pm-utils */
		if (s4_quirks) {
			if ((command = fwts_realloc_strcat(command, " ")) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
			if ((quirks = fwts_args_comma_list(s4_quirks)) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
			if ((command = fwts_realloc_strcat(command, quirks)) == NULL) {
				rc = FWTS_OUT_OF_MEMORY;
				goto tidy;
			}
		}
	}

	fwts_wakealarm_trigger(fw, s4_sleep_delay);

	/* Do s4 here */
	if ((klog_pre = fwts_klog_read()) == NULL)
		fwts_log_error(fw, "S4: hibernate: Cannot read kernel log.");

	status = do_s4(fwts_settings, percent, &duration, command);

	if ((klog_post = fwts_klog_read()) == NULL)
		fwts_log_error(fw, "S4: hibernate: Cannot re-read kernel log.");

	if (s4_device_check) {
		int i;

		for (i = 0; i < s4_device_check_delay; i++) {
			char buffer[80];

			snprintf(buffer, sizeof(buffer), "(Waiting %d/%d seconds)", i+1, s4_device_check_delay);
			fwts_progress_message(fw, percent, buffer);
			sleep(1);
		}
		fwts_progress_message(fw, percent, "(Checking devices)");
		fwts_hwinfo_get(fw, &hwinfo2);
		fwts_hwinfo_compare(fw, &hwinfo1, &hwinfo2, &differences);
		fwts_hwinfo_free(&hwinfo1);
		fwts_hwinfo_free(&hwinfo2);

		if (differences > 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "DevConfigDiffAfterS4",
				"Found %d differences in device configuration during S4 cycle.", differences);
			(*hw_errors)++;
		}
	}

	fwts_progress_message(fw, percent, "(Checking for errors)");

	klog_diff = fwts_klog_find_changes(klog_pre, klog_post);
	s4_check_log(fw, klog_diff, klog_errors, klog_oopses, klog_warn_ons);

	fwts_progress_message(fw, percent, "(Checking for PM errors)");

	/* Add in error check for pm-hibernate status */
	if ((status > 0) && (status < 128)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PMActionFailedPreS4",
			"pm-action failed before trying to put the system "
			"in the requested power saving state.");
		(*pm_errors)++;
	} else if (status == 128) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PMActionPowerStateS4",
			"pm-action tried to put the machine in the requested "
			"power state but failed.");
		(*pm_errors)++;
	} else if (status > 128) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PMActionFailedS4",
			"pm-action encountered an error and also failed to "
			"enter the requested power saving state.");
		(*pm_errors)++;
	}

	if (fwts_klog_regex_find(fw, klog_diff, "Freezing user space processes.*done") < 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UserSpaceTaskFreeze",
			"Failed to freeze user space processes.");
		(*pm_errors)++;
	}

	if (fwts_klog_regex_find(fw, klog_diff, "Freezing remaining freezable tasks.*done") < 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "KernelTaskFreeze",
			"Failed to freeze remaining non-user space processes.");
		(*pm_errors)++;
	}

	if ((fwts_klog_regex_find(fw, klog_diff, "PM: freeze of devices complete") < 1) &&
	    (fwts_klog_regex_find(fw, klog_diff, "PM: late freeze of devices complete") < 1)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "DeviceFreeze",
			"Failed to freeze devices.");
		(*pm_errors)++;
	}

	if (fwts_klog_regex_find(fw, klog_diff, "PM: Allocated.*kbytes") < 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "HibernateImageAlloc",
			"Failed to allocate memory for hibernate image.");
		*failed_alloc_image = 1;
		(*pm_errors)++;
	}

	if (fwts_klog_regex_find(fw, klog_diff, "PM: Image restored successfully") < 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "HibernateImageRestore",
			"Failed to restore hibernate image.");
		(*pm_errors)++;
	}

	fwts_klog_free(klog_pre);
	fwts_klog_free(klog_post);
	fwts_list_free(klog_diff, NULL);
tidy:
	free(command);
	free(quirks);
	free_pm_method_vars(fwts_settings);

	return rc;
}

static int s4_test_multiple(fwts_framework *fw)
{
	int i;
	int klog_errors = 0;
	int hw_errors = 0;
	int pm_errors = 0;
	int klog_oopses = 0;
	int klog_warn_ons = 0;
	int awake_delay = s4_min_delay * 1000;
	int delta = (int)(s4_delay_delta * 1000.0);
	int tracing_buffer_size = -1;
	int ret = FWTS_OK;
	int pm_debug;
	bool retried = false;

#if FWTS_ENABLE_LOGIND
#if !GLIB_CHECK_VERSION(2,35,0)
	/* This is for backward compatibility with old glib versions */
	g_type_init();
#endif
#endif

	(void)fwts_pm_debug_get(&pm_debug);
	(void)fwts_pm_debug_set(1);

        if (s4_multiple == 1)
                fwts_log_info(fw, "Defaulted to run 1 test, run --s4-multiple=N to run more S4 cycles\n");

	for (i = 0; i < s4_multiple; i++) {
		struct timeval tv;
		int failed_alloc_image = 0;
		int percent = (i * 100) / s4_multiple;

		fwts_log_info(fw, "S4 cycle %d of %d\n",i+1,s4_multiple);

		if (s4_hibernate(fw,
			&klog_errors, &hw_errors, &pm_errors,
			&klog_oopses, &klog_warn_ons,
			&failed_alloc_image, percent) != FWTS_OK) {
			fwts_log_error(fw, "Aborting S4 multiple tests.");
			return FWTS_ERROR;
		}

		/* Sometimes we just fail at the first S4 cycle, so
		   shrink tracing buffer size and retry */
		if (failed_alloc_image) {
			if (fwts_get_int(FWTS_TRACING_BUFFER_SIZE, &tracing_buffer_size) != FWTS_OK) {
				fwts_log_error(fw, "Could not get size from %s.", FWTS_TRACING_BUFFER_SIZE);
			} else {
				if ((!retried) && (tracing_buffer_size > 4096)) {
					retried = true;

					fwts_failed(fw, LOG_LEVEL_HIGH,
						"TracingBufferTooBig",
						"/sys/kernel/debug/tracing/buffer_size_kb is set to %d Kbytes which "
						"may cause hibernate to fail. Programs such as ureadahead may have "
						"set this enable fast boot and not freed up the tracing buffer.", tracing_buffer_size);

					fwts_log_info(fw, "Setting tracing buffer size to 1K for subsequent tests.");

					(void)fwts_set("1", FWTS_TRACING_BUFFER_SIZE);
					failed_alloc_image = 0;

					if (s4_hibernate(fw,
						&klog_errors, &hw_errors, &pm_errors,
						&klog_oopses, &klog_warn_ons,
						&failed_alloc_image, percent) != FWTS_OK) {
						fwts_log_error(fw, "Aborting S4 multiple tests.");
						ret = FWTS_ABORTED;
						break;
					};

					if (failed_alloc_image) {
						ret = FWTS_ABORTED;
						break;
					}
				}
			}
		}

		if (!s4_device_check) {
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
			if (awake_delay > (s4_max_delay * 1000))
				awake_delay = s4_min_delay * 1000;
		}
	}

	/* Restore pm debug value */
	if (pm_debug != -1)
		(void)fwts_pm_debug_set(pm_debug);

	if (tracing_buffer_size > 0) {
		char tmp[32];

		/* Restore tracking buffer size */
		snprintf(tmp, sizeof(tmp), "%d", tracing_buffer_size);
		(void)fwts_set(tmp, FWTS_TRACING_BUFFER_SIZE);
	}

	if (klog_errors > 0)
		fwts_log_info(fw, "Found %d errors in kernel log.", klog_errors);
	else
		fwts_passed(fw, "No kernel log errors detected.");

	if (pm_errors > 0)
		fwts_log_info(fw, "Found %d PM related hibernate issues.", pm_errors);
	else
		fwts_passed(fw, "No PM related hibernate issues detected.");

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


	/* Really passed or failed? */
	if ((klog_errors + pm_errors + hw_errors + klog_oopses) > 0) {
                fwts_log_info(fw, "Found %d errors and %d oopses doing %d hibernate/resume cycle(s).",
			klog_errors + pm_errors + hw_errors,
			klog_oopses, s4_multiple);
	} else
		fwts_passed(fw, "Found no errors and no oopses  doing %d hibernate/resume cycle(s).", s4_multiple);

	return ret;
}

static int s4_options_check(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if ((s4_multiple < 0) || (s4_multiple > 100000)) {
		fprintf(stderr, "--s4-multiple is %d, it should be 1..100000\n", s4_multiple);
		return FWTS_ERROR;
	}
	if ((s4_min_delay < 0) || (s4_min_delay > 3600)) {
		fprintf(stderr, "--s4-min-delay is %d, it cannot be less than zero or more than 1 hour!\n", s4_min_delay);
		return FWTS_ERROR;
	}
	if (s4_max_delay < s4_min_delay || s4_max_delay > 3600)  {
		fprintf(stderr, "--s4-max-delay is %d, it cannot be less than --s4-min-delay or more than 1 hour!\n", s4_max_delay);
		return FWTS_ERROR;
	}
	if (s4_delay_delta <= 0.001) {
		fprintf(stderr, "--s4-delay-delta is %f, it cannot be less than 0.001\n", s4_delay_delta);
		return FWTS_ERROR;
	}
	if ((s4_sleep_delay < 10) || (s4_sleep_delay > 3600)) {
		fprintf(stderr, "--s4-sleep-delay is %d, it cannot be less than 10 seconds or more than 1 hour!\n", s4_sleep_delay);
		return FWTS_ERROR;
	}
	if ((s4_device_check_delay < 1) || (s4_device_check_delay > 3600)) {
		fprintf(stderr, "--s4-device-check-delay is %d, it cannot be less than 1 second or more than 1 hour!\n", s4_device_check_delay);
		return FWTS_ERROR;
	}
	if (s4_min_max_delay & s4_device_check) {
		fprintf(stderr, "Cannot use --s4-min-delay, --s4-max-delay, --s4-delay-delta as well as --s4-device-check, --s4-device-check-delay.\n");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int s4_options_handler(fwts_framework *fw, int argc, char * const argv[], int option_char, int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);

        switch (option_char) {
        case 0:
                switch (long_index) {
		case 0:
			s4_multiple = atoi(optarg);
			break;
		case 1:
			s4_min_delay = atoi(optarg);
			s4_min_max_delay = true;
			break;
		case 2:
			s4_max_delay = atoi(optarg);
			s4_min_max_delay = true;
			break;
		case 3:
			s4_delay_delta = atof(optarg);
			s4_min_max_delay = true;
			break;
		case 4:
			s4_sleep_delay = atoi(optarg);
			break;
		case 5:
			s4_device_check = true;
			break;
		case 6:
			s4_quirks = optarg;
			break;
		case 7:
			s4_device_check_delay = atoi(optarg);
			s4_device_check = true;
		}
	}
	return FWTS_OK;
}

static fwts_option s4_options[] = {
	{ "s4-multiple",        "", 1, "Run S4 tests multiple times, e.g. --s4-multiple=10." },
	{ "s4-min-delay",       "", 1, "Minimum time between S4 iterations." },
	{ "s4-max-delay",       "", 1, "Maximum time between S4 iterations." },
	{ "s4-delay-delta",     "", 1, "Time to be added to delay between S4 iterations. Used in conjunction with --s4-min-delay and --s4-max-delay." },
	{ "s4-sleep-delay",	"", 1, "Sleep N seconds between start of hibernate and wakeup." },
	{ "s4-device-check",	"", 0, "Check differences between device configurations over a S4 cycle. Note we add a default of 15 seconds to allow wifi to re-associate.  Cannot be used with --s4-min-delay, --s4-max-delay and --s4-delay-delta." },
	{ "s4-quirks",		"", 1, "Comma separated list of quirk arguments to pass to pm-hibernate." },
	{ "s4-device-check-delay", "", 1, "Sleep N seconds before we run a device check after waking up from hibernate. Default is 15 seconds." },
        { NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test s4_tests[] = {
	{ s4_test_multiple, "S4 hibernate/resume test." },
	{ NULL, NULL }
};

static fwts_framework_ops s4_ops = {
	.description = "S4 hibernate/resume test.",
	.init        = s4_init,
	.minor_tests = s4_tests,
	.options     = s4_options,
	.options_handler = s4_options_handler,
	.options_check = s4_options_check
};

FWTS_REGISTER("s4", &s4_ops, FWTS_TEST_LAST, FWTS_FLAG_POWER_STATES | FWTS_FLAG_ROOT_PRIV)

#endif
