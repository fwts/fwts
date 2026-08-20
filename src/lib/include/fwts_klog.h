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

#ifndef __FWTS_KLOG_H__
#define __FWTS_KLOG_H__

#include <sys/types.h>
#include <regex.h>

#include "fwts_list.h"
#include "fwts_framework.h"
#include "fwts_log.h"
#include "fwts_json.h"

#define KERN_WARNING            0x00000001
#define KERN_ERROR              0x00000002


typedef void (*fwts_klog_progress_func)(fwts_framework *fw, int percent);
typedef void (*fwts_klog_scan_func)(fwts_framework *fw, char *line, int repeated, char *prevline, void *private, int *errors);

int        fwts_klog_scan(fwts_framework *fw, fwts_list *klog, fwts_klog_scan_func callback, fwts_klog_progress_func progress, void *private, int *errors);
void       fwts_klog_scan_patterns(fwts_framework *fw, char *line, int repeated, char *prevline, void *private, int *errors);
fwts_list *fwts_klog_read(void);
fwts_list *fwts_klog_find_changes(fwts_list *klog_old, fwts_list *klog_new);
void       fwts_klog_free(fwts_list *list);


int        fwts_klog_firmware_check(fwts_framework *fw, fwts_klog_progress_func progress, fwts_list *klog, int *errors);
int        fwts_klog_pm_check(fwts_framework *fw, fwts_klog_progress_func progress, fwts_list *klog, int *errors);
int	   fwts_klog_regex_find(fwts_framework *fw, fwts_list *klog, char *pattern);
char      *fwts_klog_remove_timestamp(char *text);
int        fwts_klog_write(fwts_framework *fw, const char *msg);

fwts_compare_mode fwts_klog_compare_mode_str_to_val(const char *str);
char *fwts_klog_unique_label(const char *str);

#endif
