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

#ifndef __FWTS_BINPATHS_H__
#define __FWTS_BINPATHS_H__

#define	FWTS_LSPCI_PATH		"/usr/bin/lspci"
#define	FWTS_DMIDECODE_PATH	"/usr/sbin/dmidecode"
#define FWTS_IASL_PATH		"/usr/bin/iasl"

#include "fwts_framework.h"

int fwts_check_executable(fwts_framework *fw, const char *path, const char *name);

#endif
