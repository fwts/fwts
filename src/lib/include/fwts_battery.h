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
#ifndef __FWTS_BATTERY_H__
#define __FWTS_BATTERY_H__

#define FWTS_PROC_ACPI_BATTERY          "/proc/acpi/battery"

typedef enum {
	FWTS_BATTERY_DESIGN_CAPACITY	= 0,
	FWTS_BATTERY_REMAINING_CAPACITY = 1,
	FWTS_BATTERY_ALL		= ~0
} fwts_battery_type;

int fwts_battery_get_count(fwts_framework *fw, uint32_t *count);
int fwts_battery_get_cycle_count(fwts_framework *fw, const uint32_t index, uint32_t *cycle_count);
bool fwts_battery_check_trip_point_support(fwts_framework *fw, const uint32_t index);
int fwts_battery_set_trip_point(fwts_framework *fw, const uint32_t index, const uint32_t trip_point);
int fwts_battery_get_trip_point(fwts_framework *fw, const uint32_t index, uint32_t *trip_point);
int fwts_battery_get_capacity(fwts_framework *fw, const fwts_battery_type type,
	const uint32_t index, uint32_t *capacity_mAh, uint32_t *capacity_mWh);
int fwts_battery_get_name(fwts_framework *fw, const uint32_t index, char *name, const size_t name_len);

#endif
