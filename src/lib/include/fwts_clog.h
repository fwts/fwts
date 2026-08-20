/*
 * Copyright (C) 2010-2026 Canonical
 * Copyright (C) 2018-2021 9elements Cyber Security
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

#ifndef __FWTS_CLOG_H__
#define __FWTS_CLOG_H__

#include "fwts.h"

typedef void (*fwts_clog_progress_func)(fwts_framework *fw, int percent);
typedef void (*fwts_clog_scan_func)(fwts_framework *fw, char *line, int repeated, char *prevline, void *private, int *errors);

void       fwts_clog_free(fwts_list *list);
bool       fwts_clog_available(fwts_framework *fw);
fwts_list *fwts_clog_read(fwts_framework *fw);
int        fwts_clog_scan(fwts_framework *fw, fwts_list *clog, fwts_clog_scan_func callback, fwts_clog_progress_func progress, void *private, int *errors);
void       fwts_clog_scan_patterns(fwts_framework *fw, char *line, int repeated, char *prevline, void *private, int *errors);
int        fwts_clog_firmware_check(fwts_framework *fw, fwts_clog_progress_func progress, fwts_list *clog, int *errors);

#endif
