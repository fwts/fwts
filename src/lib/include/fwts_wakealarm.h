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

#ifndef __FWTS_WAKEALARM_H__
#define __FWTS_WAKEALARM_H__

#include <linux/rtc.h>

#include "fwts_framework.h"

int fwts_wakealarm_test_firing(fwts_framework *fw, const uint32_t sleep);
int fwts_wakealarm_trigger(fwts_framework *fw, const uint32_t seconds);
int fwts_wakealarm_cancel(fwts_framework *fw);
int fwts_wakealarm_get(fwts_framework *fw, struct rtc_time *rtc_tm);
int fwts_wakealarm_set(fwts_framework *fw, struct rtc_time *rtc_tm);

#endif
