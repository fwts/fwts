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

#ifndef __FWTS_FILEIO_H__
#define __FWTS_FILEIO_H__

#include <stdio.h>
#include <zlib.h>

fwts_list* fwts_file_read(FILE *fp);
fwts_list* fwts_file_open_and_read(const char *file);
fwts_list* fwts_gzfile_read(gzFile *fp);
fwts_list* fwts_gzfile_open_and_read(const char *file);

#endif
