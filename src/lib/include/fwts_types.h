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

#ifndef __FWTS_TYPES_H__
#define __FWTS_TYPES_H__

#include <stdint.h>

typedef enum {
	FWTS_OK       = 0,

	FWTS_ERROR = -1,
	FWTS_SKIP  = -2,
	FWTS_ABORTED = -3,
	FWTS_ERROR_NO_PRIV = -4,
	FWTS_NULL_POINTER = -5,
	FWTS_NO_TABLE = -6,
	FWTS_NOT_EXIST = -7,
	FWTS_COMPLETE = -8,
	FWTS_OUT_OF_MEMORY = -9,
	FWTS_NO_EBDA = -10
} fwts_status;

#define	FWTS_MAP_FAILED 	((void *)-1)

typedef enum {
	FWTS_FALSE = 0,
	FWTS_TRUE  = 1,
	FWTS_BOOL_ERROR = -1
} fwts_bool;

typedef enum {
	FWTS_PM_LOGIND,
	FWTS_PM_PMUTILS,
	FWTS_PM_SYSFS,
	FWTS_PM_UNDEFINED
} fwts_pm_method;

#endif
