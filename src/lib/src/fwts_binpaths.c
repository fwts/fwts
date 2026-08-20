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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fwts_types.h"
#include "fwts_framework.h"

#define FLAGS	(S_IROTH | S_IXOTH)

/*
 *  fwts_check_executable()
 *	check if given file is an executable
 */
int fwts_check_executable(fwts_framework *fw, const char *path, const char *name)
{
	struct stat statbuf;

	if (stat(path, &statbuf)) {
		fwts_log_error(fw, "ERROR: Cannot find %s, make sure %s is installed.", path, name);
		return FWTS_ERROR;
	}
	if ((statbuf.st_mode & FLAGS) != FLAGS) {
		fwts_log_error(fw, "ERROR: Cannot read/execute %s.", path);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}
