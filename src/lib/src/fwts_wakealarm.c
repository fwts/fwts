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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/rtc.h>

#include "fwts.h"

static const char *fwts_rtc = "/dev/rtc0";

/*
 *  fwts_wakealarm_get()
 *	get wakealarm
 */
int fwts_wakealarm_get(fwts_framework *fw, struct rtc_time *rtc_tm)
{
	int fd;
	int ret = FWTS_OK;

	(void)memset(rtc_tm, 0, sizeof(*rtc_tm));
	if ((fd = open(fwts_rtc, O_RDWR)) < 0) {
		fwts_log_error(fw, "Cannot access Real Time Clock device %s.", fwts_rtc);
		return FWTS_ERROR;
	}

	if (ioctl(fd, RTC_ALM_READ, rtc_tm) < 0) {
		fwts_log_error(fw, "Cannot read Real Time Clock Alarm with ioctl RTC_ALM_READ %s.", fwts_rtc);
		ret = FWTS_ERROR;
	}
	(void)close(fd);

	return ret;
}

/*
 *  fwts_wakealarm_set()
 *	set wakealarm
 */
int fwts_wakealarm_set(fwts_framework *fw, struct rtc_time *rtc_tm)
{
	int fd;
	int ret = FWTS_OK;

	if ((fd = open(fwts_rtc, O_RDWR)) < 0) {
		fwts_log_error(fw, "Cannot access Real Time Clock device %s.", fwts_rtc);
		return FWTS_ERROR;
	}

	if (ioctl(fd, RTC_ALM_SET, rtc_tm) < 0) {
		fwts_log_error(fw, "Cannot set Real Time Clock Alarm with ioctl RTC_ALM_SET %s.", fwts_rtc);
		ret = FWTS_ERROR;
	}
	(void)close(fd);

	return ret;
}

/*
 *  fwts_wakealarm_trigger()
 *	trigger the RTC wakealarm to fire in 'seconds' seconds from now.
 */
int fwts_wakealarm_trigger(fwts_framework *fw, const uint32_t seconds)
{
	int fd, ret = FWTS_OK;
	struct rtc_time rtc_tm;

	if ((fd = open(fwts_rtc, O_RDWR)) < 0) {
		fwts_log_error(fw, "Cannot access Real Time Clock device %s.", fwts_rtc);
		return FWTS_ERROR;
	}

	(void)memset(&rtc_tm, 0, sizeof(rtc_tm));
	if (ioctl(fd, RTC_RD_TIME, &rtc_tm) < 0) {
		fwts_log_error(fw, "Cannot read Real Time Clock with ioctl RTC_RD_TIME %s.", fwts_rtc);
		ret = FWTS_ERROR;
		goto out;
	}

	rtc_tm.tm_sec += seconds;
	if (rtc_tm.tm_sec >= 60) {
		rtc_tm.tm_min += rtc_tm.tm_sec / 60;
		rtc_tm.tm_sec %= 60;
	}
	if (rtc_tm.tm_min >= 60) {
		rtc_tm.tm_hour += rtc_tm.tm_min / 60;
		rtc_tm.tm_min %= 60;
	}
	if (rtc_tm.tm_hour >= 24) {
		rtc_tm.tm_hour %= 24;
	}
	errno = 0;
	if (ioctl(fd, RTC_ALM_SET, &rtc_tm) < 0) {
		if (errno == ENOTTY) {
			fwts_log_error(fw, "Real Time Clock device %s does not support alarm interrupts.", fwts_rtc);
			ret = FWTS_ERROR;
			goto out;
		}
	}
	if (ioctl(fd, RTC_AIE_ON, 0) < 0) {
		fwts_log_error(fw, "Cannot enable alarm interrupts on Real Time Clock device %s.", fwts_rtc);
		ret = FWTS_ERROR;
	}
out:
	(void)close(fd);

	return ret;
}

int fwts_wakealarm_cancel(fwts_framework *fw)
{
	int fd, ret = FWTS_OK;

	if ((fd = open(fwts_rtc, O_RDWR)) < 0) {
		fwts_log_error(fw, "Cannot access Real Time Clock device %s.", fwts_rtc);
		return FWTS_ERROR;
	}

	if (ioctl(fd, RTC_AIE_OFF, 0) < 0) {
		fwts_log_error(fw, "Cannot cancel Real Time Clock Alarm with ioctl RTC_AIE_OFF %s.", fwts_rtc);
		ret = FWTS_ERROR;
	}
	(void)close(fd);

	return ret;
}

/*
 *  fwts_wakealarm_check_fired()
 *	check if wakealarm fires
 */
int fwts_wakealarm_check_fired(fwts_framework *fw, const uint32_t seconds)
{
	int fd, rc, ret = FWTS_OK;
	fd_set rfds;
	struct timeval tv;

	if ((fd = open(fwts_rtc, O_RDWR)) < 0) {
		fwts_log_error(fw, "Cannot access Real Time Clock device %s.", fwts_rtc);
		return FWTS_ERROR;
	}

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	/* Wait for 1 second longer than the alarm */
	tv.tv_sec = seconds + 1;
	tv.tv_usec = 0;

	/* Wait for data to be available or timeout */
	rc = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (rc == -1) {
		fwts_log_error(fw, "Select failed waiting for Real Time Clock device %s to fire.\n", fwts_rtc);
		ret = FWTS_ERROR;
		goto out;
	}

	/* Timed out, no data available, so alarm did not fire */
	if (rc == 0) {
		fwts_log_error(fw, "Wakealarm Real Time Clock device %s did not fire", fwts_rtc);
		ret = FWTS_ERROR;
	}
out:
	(void)close(fd);

	return ret;
}


/*
 *  fwts_wakealarm_test_firing()
 *	test RTC wakealarm trigger and firing from 'seconds' seconds time
 * 	from now.  returns FWTS_OK if passed, otherwise FWTS_ERROR.
 */
int fwts_wakealarm_test_firing(fwts_framework *fw, const uint32_t seconds)
{
	int ret = FWTS_OK;

	if (fwts_wakealarm_trigger(fw, seconds + 2) != FWTS_OK)
		return FWTS_ERROR;

	if (fwts_wakealarm_check_fired(fw, seconds + 2) != FWTS_OK)
		ret = FWTS_ERROR;

	fwts_wakealarm_cancel(fw);

	return ret;
}
