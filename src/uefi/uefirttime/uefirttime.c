/*
 * Copyright (C) 2012-2026 Canonical
 * Copyright (c) 2021, ARM Limited. All rights reserved.
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

#if defined(FWTS_HAS_UEFI)

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "fwts_uefi.h"
#include "fwts_efi_runtime.h"
#include "fwts_efi_module.h"

#define UEFI_IGNORE_UNSET_BITS	(0)

#define IS_LEAP(year) \
		((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

static int fd;
static const uint32_t dayofmonth[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static uint32_t runtimeservicessupported;

static bool dayvalid(EFI_TIME *Time)
{
	if (Time->Day < 1)
		return false;

	if (Time->Day > dayofmonth[Time->Month - 1])
		return false;

	/* check month 2 */
	if (Time->Month == 2 && (!IS_LEAP(Time->Year) && Time->Day > 28))
		return false;

	return true;
}

static bool checktimefields(fwts_framework *fw, EFI_TIME *Time)
{
	if (Time->Year < 1900 || Time->Year > 9999) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadYear",
			"Time returned an invalid year %" PRIu16
			", should be between 1900 and 9999.",
			Time->Year);
		return false;
	}

	if (Time->Month < 1 || Time->Month > 12) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadMonth",
			"Time returned an invalid month %" PRIu8
			", should be between 1 and 12.",
			Time->Month);
		return false;
	}

	if (!dayvalid(Time)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadDay",
			"Time returned an invalid day %" PRIu8
			", should be between 1 and 28-31 depends on month/year.",
			Time->Day);
		return false;
	}

	if (Time->Hour > 23) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadHour",
			"Time returned an invalid hour %" PRIu8
			", should be between 0 and 23.",
			Time->Hour);
		return false;
	}

	if (Time->Minute > 59) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadMinute",
			"Time returned an invalid minute %" PRIu8
			", should be between 0 and 59.",
			Time->Minute);
		return false;
	}

	if (Time->Second > 59) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadSecond",
			"Time returned an invalid second %" PRIu8
			", should be between 0 and 59.",
			Time->Second);
		return false;
	}

	if (Time->Nanosecond > 999999999) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadNanosecond",
			"Time returned an invalid nanosecond %" PRIu32
			", should be between 0 and 999999999.",
			Time->Nanosecond);
		return false;
	}

	if (!(Time->TimeZone == FWTS_UEFI_UNSPECIFIED_TIMEZONE ||
		(Time->TimeZone >= -1440 && Time->TimeZone <= 1440))) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadTimezone",
			"Time returned an invalid timezone %" PRId16
			", should be between -1440 and 1440 or 2047.",
			Time->TimeZone);
		return false;
	}

	if (Time->Daylight & (~(FWTS_UEFI_TIME_ADJUST_DAYLIGHT |
				FWTS_UEFI_TIME_IN_DAYLIGHT))) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeTimeFieldBadDaylight",
			"Time returned an invalid daylight %" PRIu8
			", all other bits except UEFI_TIME_IN_DAYLIGHT and "
			"UEFI_TIME_ADJUST_DAYLIGHT must be zero.",
			Time->Daylight);
		return false;
	}
	return true;
}


static void addonehour(EFI_TIME *time)
{
	if (time->Hour != 23) {
		time->Hour += 1;
		return;
	}
	time->Hour = 0;

	if ((time->Day != dayofmonth[time->Month - 1]) &&
		(!(time->Month == 2 && (!IS_LEAP(time->Year)
		&& time->Day == 28)))) {
		time->Day += 1;
		return;
	}
	time->Day = 1;

	if (time->Month != 12) {
		time->Month += 1;
		return;
	}
	time->Month = 1;
	time->Year += 1;
	return;
}

static int uefirttime_init(fwts_framework *fw)
{
	if (fwts_lib_efi_runtime_module_init(fw, &fd) == FWTS_ABORTED)
		return FWTS_ABORTED;

	fwts_uefi_rt_support_status_get(fd, &runtimeservicessupported);

	return FWTS_OK;
}

static int uefirttime_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_lib_efi_runtime_close(fd);
	fwts_lib_efi_runtime_unload_module(fw);

	return FWTS_OK;
}


static int uefirttime_test1(fwts_framework *fw)
{
	long ioret;
	struct efi_gettime gettime;
	EFI_TIME efi_time;

	EFI_TIME_CAPABILITIES efi_time_cap;
	uint64_t status = ~0ULL;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_TIME)) {
		fwts_skipped(fw, "Skipping test, GetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	gettime.Capabilities = &efi_time_cap;
	gettime.Time = &efi_time;
	gettime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
			"Failed to get time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (!checktimefields(fw, gettime.Time))
		return FWTS_ERROR;

	fwts_passed(fw, "UEFI runtime service GetTime interface test passed.");

	return FWTS_OK;
}

static int uefirttime_test_gettime_invalid(
	fwts_framework *fw,
	EFI_TIME *efi_time,
	EFI_TIME_CAPABILITIES *efi_time_cap)
{
	long ioret;
	struct efi_gettime gettime;
	uint64_t status = ~0ULL;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_TIME)) {
		fwts_skipped(fw, "Skipping test, GetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	gettime.Capabilities = efi_time_cap;
	gettime.Time = efi_time;
	gettime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "UEFI runtime service GetTime interface test "
				"passed, returned EFI_INVALID_PARAMETER as expected.");
			return FWTS_OK;
		} else {
			fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
				"Failed to get correct return status from UEFI runtime service, expecting EFI_INVALID_PARAMETER.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
		"Failed to get error return status from UEFI runtime service, expected EFI_INAVLID_PARAMETER.");
	return FWTS_ERROR;
}

static int uefirttime_test2(fwts_framework *fw)
{
	EFI_TIME_CAPABILITIES efi_time_cap;

	return uefirttime_test_gettime_invalid(fw, NULL, &efi_time_cap);
}

static int uefirttime_test3(fwts_framework *fw)
{
	return uefirttime_test_gettime_invalid(fw, NULL, NULL);
}

static int uefirttime_test4(fwts_framework *fw)
{

	long ioret;
	struct efi_settime settime;
	uint64_t status = ~0ULL;
	struct efi_gettime gettime;

	EFI_TIME oldtime;
	EFI_TIME newtime;
	EFI_TIME time;
	EFI_TIME_CAPABILITIES efi_time_cap;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_TIME)) {
		fwts_skipped(fw, "Skipping test, GetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	gettime.Capabilities = &efi_time_cap;
	gettime.Time = &oldtime;
	gettime.status = &status;
	ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
			"Failed to get time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	/* refer to UEFI SCT 2.3 test items */
	/* change year */
	time = oldtime;
	if (time.Year != 2012)
		time.Year = 2012;
	else
		time.Year = 2016;

	/* change month */
	if (time.Month != 1)
		time.Month = 1;
	else
		time.Month = 12;

	/* Change daylight */
	if (time.Daylight & FWTS_UEFI_TIME_ADJUST_DAYLIGHT)
		time.Daylight &= ~FWTS_UEFI_TIME_ADJUST_DAYLIGHT;
	else
		time.Daylight |= FWTS_UEFI_TIME_ADJUST_DAYLIGHT;

	/* Change time zone */
	if (time.TimeZone != 0)
		time.TimeZone = 0;
	else
		/* Unspecified timezone, local time */
		time.TimeZone = 2047;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_TIME)) {
		fwts_skipped(fw, "Skipping test, SetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	settime.Time = &time;
	status = ~0ULL;
	settime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_TIME, &settime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
			"Failed to set time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	sleep(1);

	gettime.Time = &newtime;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
			"Failed to get time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (!((oldtime.Year == 2012) && (newtime.Year == 2016)) &&
	    !((oldtime.Year != 2012) && (newtime.Year == 2012))) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTimeYear",
			"Failed to set year with UEFI runtime service.");
		return FWTS_ERROR;
	}

	if (!((oldtime.Month == 1) && (newtime.Month == 12)) &&
	    !((oldtime.Month != 1) && (newtime.Month == 1))) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTimeMonth",
			"Failed to set month with UEFI runtime service.");
		return FWTS_ERROR;
	}

	if (!((oldtime.Daylight & FWTS_UEFI_TIME_ADJUST_DAYLIGHT) &&
	    (!(newtime.Daylight & FWTS_UEFI_TIME_ADJUST_DAYLIGHT))) &&
	    !((!(oldtime.Daylight & FWTS_UEFI_TIME_ADJUST_DAYLIGHT)) &&
	    (newtime.Daylight & FWTS_UEFI_TIME_ADJUST_DAYLIGHT))) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTimeDaylight",
			"Failed to set daylight with UEFI runtime service.");
		return FWTS_ERROR;
	}

	if (!((oldtime.TimeZone == 0) && (newtime.TimeZone == 2047)) &&
	    !((oldtime.TimeZone != 0) && (newtime.TimeZone == 0))) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTimeTimezone",
			"Failed to set timezone with UEFI runtime service.");
		fwts_advice(fw,
			"Some system firmware may not support the timezone "
			"value EFI_UNSPECIFIED_TIMEZONE(2047) and set the "
			"value 0 directly instead of returning an error which "
			"causes the failure.");
		return FWTS_ERROR;
	}

	/* restore the previous time. */
	settime.Time = &oldtime;
	status = ~0ULL;
	ioret = ioctl(fd, EFI_RUNTIME_SET_TIME, &settime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
			"Failed to set time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	fwts_passed(fw, "UEFI runtime service SetTime interface test passed.");

	return FWTS_OK;
}

static int uefirttime_test_settime_invalid(
	fwts_framework *fw,
	struct efi_settime *settime)
{
	long ioret;
	static uint64_t status;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_TIME)) {
		fwts_skipped(fw, "Skipping test, SetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	status = ~0ULL;
	settime->status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_TIME, settime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "UEFI runtime service SetTime interface test "
				"passed, returned EFI_INVALID_PARAMETER as expected.");
			return FWTS_OK;
		} else {
			fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
				"Failed to get correct return status from UEFI runtime service, expecting EFI_INVALID_PARAMETER.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
		"Failed to get error return status from UEFI runtime service, expected EFI_INVALID_PARAMETER.");
	return FWTS_ERROR;
}

static int uefirttime_test_settime_invalid_time(
	fwts_framework *fw,
	EFI_TIME *time)
{
	struct efi_gettime gettime;
	struct efi_settime settime;
	EFI_TIME oldtime, newtime;
	uint64_t status = ~0ULL;
	int ret, ioret;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_TIME)) {
		fwts_skipped(fw, "Skipping test, GetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	gettime.Time = &oldtime;
	gettime.status = &status;
	gettime.Capabilities = NULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
			"Failed to get time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	memcpy(&newtime, &oldtime, sizeof(EFI_TIME));
	if (time->Year != 0xffff)
		newtime.Year = time->Year;
	if (time->Month != 0xff)
		newtime.Month = time->Month;
	if (time->Day != 0xff)
		newtime.Day = time->Day;
	if (time->Hour != 0xff)
		newtime.Hour = time->Hour;
	if (time->Minute != 0xff)
		newtime.Minute = time->Minute;
	if (time->Second != 0xff)
		newtime.Second = time->Second;
	if (time->Nanosecond != 0xffffffff)
		newtime.Nanosecond = time->Nanosecond;
	if ((uint16_t)time->TimeZone != 0xffff)
		newtime.TimeZone = time->TimeZone;

	settime.Time = &newtime;
	settime.status = &status;

	ret = uefirttime_test_settime_invalid(fw, &settime);

	if (ret == FWTS_SKIP)
		return ret;

	/* Restore original time */
	settime.Time = &oldtime;
	status = ~0ULL;
	settime.status = &status;
	ioret = ioctl(fd, EFI_RUNTIME_SET_TIME, &settime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
			"Failed to set time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	return ret;
}

static int uefirttime_test5(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Year = 1899;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test6(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Year = 10000;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test7(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Month = 0;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test8(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Month = 13;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test9(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Day = 0;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test10(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Day = 32;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test11(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Hour = 24;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test12(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Minute = 60;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test13(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Second = 60;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test14(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Nanosecond = 1000000000;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test15(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.TimeZone = -1441;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

static int uefirttime_test16(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.TimeZone = 1441;
	return uefirttime_test_settime_invalid_time(fw, &time);
}

#if UEFI_IGNORE_UNSET_BITS
/*
 *  The UEFI spec states that just two bits are allowed, and all other bits
 *  must be zero, but I have yet to find a compliant implementation that
 *  flags up an invalid parameter if the bits are non zero. It seems that
 *  implementations just examine the bits they expect and don't care. For
 *  now, we will ignore this test
 */
static int uefirttime_test17(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	/* Section 7.3, Daylight states all other bits must be zero */
	time.Daylight = ~(FWTS_UEFI_TIME_ADJUST_DAYLIGHT | FWTS_UEFI_TIME_IN_DAYLIGHT);
	return uefirttime_test_settime_invalid_time(fw, &time);
}
#endif

static int uefirttime_test18(fwts_framework *fw)
{
	long ioret;
	struct efi_getwakeuptime getwakeuptime;
	uint64_t status = ~0ULL;
	uint8_t enabled, pending;
	EFI_TIME efi_time;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_WAKEUP_TIME)) {
		fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getwakeuptime.Enabled = &enabled;
	getwakeuptime.Pending = &pending;
	getwakeuptime.Time = &efi_time;
	getwakeuptime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_WAKETIME, &getwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
			"Failed to get wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (!checktimefields(fw, getwakeuptime.Time))
		return FWTS_ERROR;

	fwts_passed(fw, "UEFI runtime service GetWakeupTime interface test passed.");

	return FWTS_OK;
}

static int uefirttime_test_getwaketime_invalid(
	fwts_framework *fw,
	struct efi_getwakeuptime *getwakeuptime)
{
	long ioret;
	static uint64_t status;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_WAKEUP_TIME)) {
		fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	status = ~0ULL;
	getwakeuptime->status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_WAKETIME, getwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTimeWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "UEFI runtime service GetTimeWakeupTime interface test "
				"passed, returned EFI_INVALID_PARAMETER as expected.");
			return FWTS_OK;
		} else {
			fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
				"Failed to get correct return status from UEFI "
				"runtime service, expecting EFI_INVALID_PARAMETER.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
		"Failed to get error return status from UEFI runtime service, expected EFI_INVALID_PARAMETER.");
	return FWTS_ERROR;
}

static int uefirttime_test19(fwts_framework *fw)
{
	struct efi_getwakeuptime getwakeuptime;
	EFI_TIME efi_time;
	uint8_t pending;

	getwakeuptime.Enabled = NULL;
	getwakeuptime.Pending = &pending;
	getwakeuptime.Time = &efi_time;

	return uefirttime_test_getwaketime_invalid(fw, &getwakeuptime);
}

static int uefirttime_test20(fwts_framework *fw)
{
	struct efi_getwakeuptime getwakeuptime;
	EFI_TIME efi_time;
	uint8_t enabled;

	getwakeuptime.Enabled = &enabled;
	getwakeuptime.Pending = NULL;
	getwakeuptime.Time = &efi_time;

	return uefirttime_test_getwaketime_invalid(fw, &getwakeuptime);
}

static int uefirttime_test21(fwts_framework *fw)
{
	struct efi_getwakeuptime getwakeuptime;
	uint8_t enabled, pending;

	getwakeuptime.Enabled = &enabled;
	getwakeuptime.Pending = &pending;
	getwakeuptime.Time = NULL;

	return uefirttime_test_getwaketime_invalid(fw, &getwakeuptime);
}

static int uefirttime_test22(fwts_framework *fw)
{
	struct efi_getwakeuptime getwakeuptime;

	getwakeuptime.Enabled = NULL;
	getwakeuptime.Pending = NULL;
	getwakeuptime.Time = NULL;

	return uefirttime_test_getwaketime_invalid(fw, &getwakeuptime);
}

static int uefirttime_test23(fwts_framework *fw)
{
	long ioret;
	struct efi_setwakeuptime setwakeuptime;
	uint64_t status = ~0ULL;
	EFI_TIME oldtime;
	EFI_TIME newtime;

	struct efi_gettime gettime;
	EFI_TIME_CAPABILITIES efi_time_cap;

	struct efi_getwakeuptime getwakeuptime;
	uint8_t enabled, pending;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_TIME)) {
		fwts_skipped(fw, "Skipping test, GetTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	gettime.Capabilities = &efi_time_cap;
	gettime.Time = &oldtime;
	gettime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
			"Failed to get time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_WAKEUP_TIME)) {
		fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	/* change the hour, add 1 hour*/
	addonehour(&oldtime);

	setwakeuptime.Time = &oldtime;
	status = ~0ULL;
	setwakeuptime.status = &status;
	setwakeuptime.Enabled = true;

	ioret = ioctl(fd, EFI_RUNTIME_SET_WAKETIME, &setwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
			"Failed to set wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	sleep(1);

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_WAKEUP_TIME)) {
		fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getwakeuptime.Enabled = &enabled;
	getwakeuptime.Pending = &pending;
	getwakeuptime.Time = &newtime;
	status = ~0ULL;
	getwakeuptime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_WAKETIME, &getwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
			"Failed to get wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (*getwakeuptime.Enabled != true) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTimeEnable",
			"Failed to set wakeup alarm clock, wakeup timer should be enabled.");
		return FWTS_ERROR;
	}

	if (*getwakeuptime.Pending != false) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTimePending",
			"Get error alarm signal status.");
		return FWTS_ERROR;
	}

	if ((oldtime.Year != newtime.Year) || (oldtime.Month != newtime.Month) ||
		(oldtime.Day != newtime.Day) || (oldtime.Hour != newtime.Hour)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTimeVerify",
			"Failed to verify wakeup time after change.");
		return FWTS_ERROR;
	}

	setwakeuptime.Enabled = false;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_WAKETIME, &setwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
			"Failed to set wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	sleep(1);
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_WAKETIME, &getwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
			"Failed to get wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (*getwakeuptime.Enabled != false) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTimeEnable",
			"Failed to set wakeup alarm clock, wakeup timer should be disabled.");
		return FWTS_ERROR;
	}

	fwts_passed(fw, "UEFI runtime service SetWakeupTime interface test passed.");

	return FWTS_OK;
}

static int uefirttime_test_setwakeuptime_invalid(
	fwts_framework *fw,
	struct efi_setwakeuptime *setwakeuptime
)
{
	long ioret;
	static uint64_t status;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_WAKEUP_TIME)) {
		fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	status = ~0ULL;
	setwakeuptime->status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_WAKETIME, setwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "UEFI runtime service SetTimeWakeupTime interface test "
				"passed, returned EFI_INVALID_PARAMETER as expected.");
			return FWTS_OK;
		} else {
			fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
				"Failed to get correct return status from UEFI runtime service, "
				"expecting EFI_INVALID_PARAMETER.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
		"Failed to get error return status from UEFI runtime service, expected EFI_INVALID_PARAMETER.");
	return FWTS_ERROR;
}

static int uefirttime_test24(fwts_framework *fw)
{
	struct efi_setwakeuptime setwakeuptime;

	setwakeuptime.Enabled = true;
	setwakeuptime.Time = NULL;
	return uefirttime_test_setwakeuptime_invalid(fw, &setwakeuptime);
}

static int uefirttime_test_setwakeuptime_invalid_time(
	fwts_framework *fw,
	EFI_TIME *time)
{
	struct efi_getwakeuptime getwakeuptime;
	struct efi_setwakeuptime setwakeuptime;
	EFI_TIME oldtime, newtime;
	uint64_t status = ~0ULL;
	uint8_t pending, enabled;
	int ret, ioret;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_WAKEUP_TIME)) {
		fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getwakeuptime.Enabled = &enabled;
	getwakeuptime.Pending = &pending;
	getwakeuptime.Time = &oldtime;
	getwakeuptime.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_WAKETIME, &getwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
			"Failed to get wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	memcpy(&newtime, &oldtime, sizeof(EFI_TIME));
	if (time->Year != 0xffff)
		newtime.Year = time->Year;
	if (time->Month != 0xff)
		newtime.Month = time->Month;
	if (time->Day != 0xff)
		newtime.Day = time->Day;
	if (time->Hour != 0xff)
		newtime.Hour = time->Hour;
	if (time->Minute != 0xff)
		newtime.Minute = time->Minute;
	if (time->Second != 0xff)
		newtime.Second = time->Second;
	if (time->Nanosecond != 0xffffffff)
		newtime.Nanosecond = time->Nanosecond;
	if ((uint16_t)time->TimeZone != 0xffff)
		newtime.TimeZone = time->TimeZone;

	setwakeuptime.Time = &newtime;
	setwakeuptime.status = &status;
	setwakeuptime.Enabled = true;

	ret = uefirttime_test_setwakeuptime_invalid(fw, &setwakeuptime);

	if (ret == FWTS_SKIP)
		return ret;

	/* Restore original time */
	setwakeuptime.Time = &oldtime;
	status = ~0ULL;
	setwakeuptime.status = &status;
	setwakeuptime.Enabled = true;
	ioret = ioctl(fd, EFI_RUNTIME_SET_WAKETIME, &setwakeuptime);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetWakeupTime runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
			"Failed to set wakeup time with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	return ret;
}

static int uefirttime_test25(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Year = 1899;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test26(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Year = 10000;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test27(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Month = 0;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test28(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Month = 13;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test29(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Day = 0;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test30(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Day = 32;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test31(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Hour = 24;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test32(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Minute = 60;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test33(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Second = 60;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test34(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.Nanosecond = 1000000000;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test35(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.TimeZone = -1441;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

static int uefirttime_test36(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	time.TimeZone = 1441;
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}

#if UEFI_IGNORE_UNSET_BITS
/*
 *  The UEFI spec states that just two bits are allowed, and all other bits
 *  must be zero, but I have yet to find a compliant implementation that
 *  flags up an invalid parameter if the bits are non zero. It seems that
 *  implementations just examine the bits they expect and don't care. For
 *  now, we will ignore this test
 */
static int uefirttime_test37(fwts_framework *fw)
{
	EFI_TIME time;

	memset(&time, 0xff, sizeof(EFI_TIME));
	/* Section 7.3, Daylight states all other bits must be zero */
	time.Daylight = ~(FWTS_UEFI_TIME_ADJUST_DAYLIGHT | FWTS_UEFI_TIME_IN_DAYLIGHT);
	return uefirttime_test_setwakeuptime_invalid_time(fw, &time);
}
#endif

static int uefirttime_test38(fwts_framework *fw)
{
	long ioret;

	struct efi_settime settime;
	uint64_t status = ~0ULL;
	struct efi_gettime gettime;
	struct efi_getwakeuptime getwakeuptime;
	uint8_t enabled, pending;
	struct efi_setwakeuptime setwakeuptime;

	EFI_TIME efi_time;
	EFI_TIME_CAPABILITIES efi_time_cap;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_TIME)) {
		gettime.Capabilities = &efi_time_cap;
		gettime.Time = &efi_time;
		gettime.status = &status;

		ioret = ioctl(fd, EFI_RUNTIME_GET_TIME, &gettime);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
				fwts_passed(fw, "UEFI GetTime runtime service "
					"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
						"Get the GetTime runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS )
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetTime",
					"Get the GetTime runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
		}
	} else
		fwts_skipped(fw, "GetTime runtime service supported, skip test.");

	/* set test time for settime and setwakeuptime tests */
	memset(&efi_time, 0, sizeof(efi_time));
	efi_time.Year = 2000;
	efi_time.Month = 1;
	efi_time.Day = 1;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_TIME)) {
		settime.Time = &efi_time;
		status = ~0ULL;
		settime.status = &status;

		ioret = ioctl(fd, EFI_RUNTIME_SET_TIME, &settime);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_passed(fw, "UEFI SetTime runtime service "
					"unsupported status test passed.");
			} else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
						"Get the SetTime runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS )
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetTime",
					"Get the SetTime runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
		}
	} else
		fwts_skipped(fw, "SetTime runtime service supported, skip test.");

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_WAKEUP_TIME)) {
		setwakeuptime.Time = &efi_time;
		status = ~0ULL;
		setwakeuptime.status = &status;
		setwakeuptime.Enabled = false;

		ioret = ioctl(fd, EFI_RUNTIME_SET_WAKETIME, &setwakeuptime);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
						fwts_passed(fw, "UEFI SetWakeupTime runtime service "
							"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
						"Get the SetWakeupTime runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS )
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetWakeupTime",
					"Get the SetWakeupTime runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
		}
	}
	else
		fwts_skipped(fw, "SetWakeupTime runtime service supported, skip test.");

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_WAKEUP_TIME)) {
		getwakeuptime.Enabled = &enabled;
		getwakeuptime.Pending = &pending;
		getwakeuptime.Time = &efi_time;
		getwakeuptime.status = &status;

		ioret = ioctl(fd, EFI_RUNTIME_GET_WAKETIME, &getwakeuptime);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
					fwts_passed(fw, "UEFI GetWakeupTime runtime service "
						"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
						"Get the GetWakeupTime runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware");
			}
		} else {
			if (status != EFI_SUCCESS )
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetWakeupTime",
					"Get the GetWakeupTime runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware");
		}
	} else
		fwts_skipped(fw, "GetWakeupTime runtime service supported, skip test.");

	return FWTS_OK;
}

static fwts_framework_minor_test uefirttime_tests[] = {
	{ uefirttime_test1, "Test UEFI RT service get time interface." },
	{ uefirttime_test2, "Test UEFI RT service get time interface, NULL time parameter." },
	{ uefirttime_test3, "Test UEFI RT service get time interface, NULL time and NULL capabilities parameters." },

	{ uefirttime_test4, "Test UEFI RT service set time interface." },
	{ uefirttime_test5, "Test UEFI RT service set time interface, invalid year 1899." },
	{ uefirttime_test6, "Test UEFI RT service set time interface, invalid year 10000." },
	{ uefirttime_test7, "Test UEFI RT service set time interface, invalid month 0." },
	{ uefirttime_test8, "Test UEFI RT service set time interface, invalid month 13." },
	{ uefirttime_test9, "Test UEFI RT service set time interface, invalid day 0." },
	{ uefirttime_test10, "Test UEFI RT service set time interface, invalid day 32." },
	{ uefirttime_test11, "Test UEFI RT service set time interface, invalid hour 24." },
	{ uefirttime_test12, "Test UEFI RT service set time interface, invalid minute 60." },
	{ uefirttime_test13, "Test UEFI RT service set time interface, invalid second 60." },
	{ uefirttime_test14, "Test UEFI RT service set time interface, invalid nanosecond 1000000000." },
	{ uefirttime_test15, "Test UEFI RT service set time interface, invalid timezone -1441." },
	{ uefirttime_test16, "Test UEFI RT service set time interface, invalid timezone 1441." },
#if UEFI_IGNORE_UNSET_BITS
	{ uefirttime_test17, "Test UEFI RT service set time interface, invalid daylight 0xfc." },
#endif

	{ uefirttime_test18, "Test UEFI RT service get wakeup time interface." },
	{ uefirttime_test19, "Test UEFI RT service get wakeup time interface, NULL enabled parameter." },
	{ uefirttime_test20, "Test UEFI RT service get wakeup time interface, NULL pending parameter." },
	{ uefirttime_test21, "Test UEFI RT service get wakeup time interface, NULL time parameter." },
	{ uefirttime_test22, "Test UEFI RT service get wakeup time interface, NULL enabled, pending and time parameters." },

	{ uefirttime_test23, "Test UEFI RT service set wakeup time interface." },
	{ uefirttime_test24, "Test UEFI RT service set wakeup time interface, NULL time parameter." },
	{ uefirttime_test25, "Test UEFI RT service set wakeup time interface, invalid year 1899." },
	{ uefirttime_test26, "Test UEFI RT service set wakeup time interface, invalid year 10000." },
	{ uefirttime_test27, "Test UEFI RT service set wakeup time interface, invalid month 0." },
	{ uefirttime_test28, "Test UEFI RT service set wakeup time interface, invalid month 13." },
	{ uefirttime_test29, "Test UEFI RT service set wakeup time interface, invalid day 0." },
	{ uefirttime_test30, "Test UEFI RT service set wakeup time interface, invalid day 32." },
	{ uefirttime_test31, "Test UEFI RT service set wakeup time interface, invalid hour 24." },
	{ uefirttime_test32, "Test UEFI RT service set wakeup time interface, invalid minute 60." },
	{ uefirttime_test33, "Test UEFI RT service set wakeup time interface, invalid second 60." },
	{ uefirttime_test34, "Test UEFI RT service set wakeup time interface, invalid nanosecond 1000000000." },
	{ uefirttime_test35, "Test UEFI RT service set wakeup time interface, invalid timezone -1441." },
	{ uefirttime_test36, "Test UEFI RT service set wakeup time interface, invalid timezone 1441." },
#if UEFI_IGNORE_UNSET_BITS
	{ uefirttime_test37, "Test UEFI RT service set wakeup time interface, invalid daylight 0xfc." },
#endif
	{ uefirttime_test38, "Test UEFI RT time services unsupported status." },
	{ NULL, NULL }
};

static fwts_framework_ops uefirttime_ops = {
	.description = "UEFI Runtime service time interface tests.",
	.init        = uefirttime_init,
	.deinit      = uefirttime_deinit,
	.minor_tests = uefirttime_tests
};

FWTS_REGISTER("uefirttime", &uefirttime_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_XBBR)

#endif
