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

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <linux/rtc.h>

static struct rtc_time rtc_tm;
static bool skip_suite = false;

static int wakealarm_test1(fwts_framework *fw)
{
	if (fwts_wakealarm_get(fw, &rtc_tm) == FWTS_OK) {
		fwts_passed(fw, "RTC with a RTC alarm ioctl() interface found.");
	} else {
#ifdef FWTS_ARCH_INTEL
		/* For x86 devices, this is considered a failure */
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "NoWakeAlarmTest1",
			"Could not find an RTC with an alarm ioctl() interface.");
		fwts_advice(fw,
			"x86 devices generally should have an RTC wake alarm that "
			"is normally controlled by the RTC alarm ioctl() interface. This interface "
			"does not exist, so the wake alarm tests will be aborted.");
		return FWTS_ABORTED;
#else
		skip_suite = true;

		fwts_log_info(fw,
			"non-x86 devices sometimes do not have an RTC wake alarm that "
			"is normally controlled by the RTC alarm ioctl() interface. This "
			"interface does not exist, so the wake alarm tests will be skipped.");
		return FWTS_SKIP;
#endif
	}
	return FWTS_OK;
}

static int wakealarm_test2(fwts_framework *fw)
{
	if (skip_suite) {
		return FWTS_SKIP;
	}

	fwts_log_info(fw, "Trigger wakealarm for 1 seconds in the future.");
	if (fwts_wakealarm_trigger(fw, 1)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "WakeAlarmNotTriggeredTest2",
			"RTC wakealarm did not trigger.");
		return FWTS_OK;
	}

	(void)fwts_wakealarm_cancel(fw);

	fwts_passed(fw, "RTC wakealarm was triggered successfully.");

	return FWTS_OK;
}

static int wakealarm_test3(fwts_framework *fw)
{
	int ret;

	if (skip_suite) {
		return FWTS_SKIP;
	}

	ret = fwts_wakealarm_test_firing(fw, 2);
	if (ret < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "WakeAlarmNotTriggeredTest3",
			"Failed to trigger and fire wakealarm.");
		return FWTS_ERROR;	/* Really went wrong */
	}
	if (ret == 0)
		fwts_passed(fw, "RTC wakealarm triggered and fired successfully.");
	else {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "WakeAlarmNotFiredTest3",
			"RTC wakealarm was triggered but did not fire.");
	}

	return FWTS_OK;
}

static int wakealarm_test4(fwts_framework *fw)
{
	uint32_t i;
	int failed = 0;

	if (skip_suite) {
		return FWTS_SKIP;
	}

	for (i = 1; i < 5; i++) {
		fwts_log_info(fw, "Trigger wakealarm for %" PRIu32 " seconds in the future.", i);
		int ret = fwts_wakealarm_test_firing(fw, i);
		if (ret < 0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"WakeAlarmNotTriggeredTest4",
				"Failed to trigger and fire wakealarm.");
			return FWTS_ERROR;	/* Really went wrong */
		}
		if (ret != 0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"WakeAlarmNotFiredTest4",
				"RTC wakealarm was triggered but did not fire.");
			failed++;
		}
		fwts_progress(fw, 25 * i);
	}
	if (failed == 0)
		fwts_passed(fw, "RTC wakealarm triggered and fired successfully.");

	return FWTS_OK;
}

static int wakealarm_test5(fwts_framework *fw)
{
	struct rtc_time rtc_now;

	if (skip_suite) {
		return FWTS_SKIP;
	}

	if (fwts_wakealarm_set(fw, &rtc_tm) == FWTS_OK) {
		fwts_passed(fw, "RTC wakealarm set.");
	} else {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"WakeAlarmNotResetTest5",
			"RTC wakealarm failed to be reset back to original state.");
		return FWTS_ERROR;
	}

	if (fwts_wakealarm_get(fw, &rtc_now) == FWTS_OK) {
		if (rtc_now.tm_year == rtc_tm.tm_year &&
		    rtc_now.tm_mon  == rtc_tm.tm_mon &&
		    rtc_now.tm_mday == rtc_tm.tm_mday &&
		    rtc_now.tm_hour == rtc_tm.tm_hour &&
		    rtc_now.tm_min  == rtc_tm.tm_min &&
		    rtc_now.tm_sec  == rtc_tm.tm_sec) {
			fwts_passed(fw, "RTC wakealarm reset correctly back to "
				"%d/%d/%d %2.2d:%2.2d:%2.2d.",
					rtc_tm.tm_mday, rtc_tm.tm_mon + 1,
					rtc_tm.tm_year + 1900, rtc_tm.tm_hour,
					rtc_tm.tm_min, rtc_tm.tm_sec);
		} else if (rtc_now.tm_hour == rtc_tm.tm_hour &&
		    rtc_now.tm_min == rtc_tm.tm_min &&
		    rtc_now.tm_sec == rtc_tm.tm_sec) {
			fwts_skipped(fw,
				"The kernel adjusted the RTC_ALM_SET alarm by up to 24 hours "
				"because the requested time was earlier than the current time. "
				"This behavior ensures the alarm always points to a future time.");
			return FWTS_SKIP;
		} else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"WakeAlarmNotResetTest5",
				"RTC wakealarm failed to be reset back to original time.");
			return FWTS_ERROR;
		}
	} else {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"WakeAlarmNotReadTest5",
			"RTC wakealarm failed to be read to verify original time.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static fwts_framework_minor_test wakealarm_tests[] = {
	{ wakealarm_test1, "Test existence of RTC with alarm interface." },
	{ wakealarm_test2, "Trigger wakealarm for 1 seconds in the future." },
	{ wakealarm_test3, "Test if wakealarm is fired." },
	{ wakealarm_test4, "Multiple wakealarm firing tests." },
	{ wakealarm_test5, "Reset wakealarm time." },
	{ NULL, NULL }
};

static fwts_framework_ops wakealarm_ops = {
	.description = "ACPI Wakealarm tests.",
	.minor_tests = wakealarm_tests
};

FWTS_REGISTER("wakealarm", &wakealarm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
