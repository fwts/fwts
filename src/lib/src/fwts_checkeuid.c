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


#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>

#include "fwts.h"

/*
 *  fwts_check_root_euid()
 *	Check if user has privileges to access ports, memory, etc
 */
int fwts_check_root_euid(fwts_framework *fw, const bool warn)
{
	if (geteuid() != 0) {
		if (warn)
			fwts_log_error(fw, "Must be run as root or sudo to be able to read system information.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}
