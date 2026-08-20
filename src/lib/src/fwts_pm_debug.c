/*
 * Copyright (C) 2020-2026 Canonical
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

static const char pm_debug[] = "/sys/power/pm_debug_messages";

/*
 *  fwts_pm_debug_get
 *	get the current pm_debug_messages setting, value
 *	is also set to -1 if there is an error
 */
int fwts_pm_debug_get(int *value)
{
	int ret;

	ret = fwts_get_int(pm_debug, value);
	if (ret != FWTS_OK)
		*value = -1;

	return ret;
}

/*
 *  fwts_pm_debug_set
 *	set the pm_debug_messages setting
 */
int fwts_pm_debug_set(const int value)
{
	return fwts_set_int(pm_debug, value);
}
