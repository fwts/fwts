/*
 * Copyright (C) 2012-2026 Canonical
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

#ifndef __FWTS_AC_ADAPTER_H__
#define __FWTS_AC_ADAPTER_H__

#define FWTS_SYS_CLASS_POWER_SUPPLY  "/sys/class/power_supply"
#define FWTS_PROC_ACPI_AC_ADAPTER    "/proc/acpi/ac_adapter"

#define FWTS_AC_ADAPTER_ANY	(0x0)
#define FWTS_AC_ADAPTER_ONLINE	(0x1)
#define FWTS_AC_ADAPTER_OFFLINE	(0x2)

int fwts_ac_adapter_get_state(const int state, int *matching, int *not_matching);

#endif
