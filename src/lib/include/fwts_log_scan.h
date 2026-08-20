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

#ifndef __FWTS_LOG_SCAN_H__
#define __FWTS_LOG_SCAN_H__

#include <regex.h>

#include "fwts_json.h"

typedef enum {
	FWTS_COMPARE_REGEX = 'r',
	FWTS_COMPARE_STRING = 's',
	FWTS_COMPARE_UNKNOWN = 'u'
} fwts_compare_mode;

typedef struct {
	fwts_compare_mode compare_mode;
	fwts_log_level level;
	const char *pattern;
	const char *advice;
	char *label;
	regex_t compiled;
	bool compiled_ok;
} fwts_log_pattern;

typedef void (*fwts_log_progress_func)(fwts_framework *fw, int percent);
typedef void (*fwts_log_scan_func)(fwts_framework *fw, char *line, int repeated, char *prevline, void *private, int *errors);

void       fwts_log_free(fwts_list *list);
fwts_list *fwts_log_find_changes(fwts_list *log_old, fwts_list *log_new);
char      *fwts_log_remove_timestamp(char *text);
int        fwts_log_scan(fwts_framework *fw, fwts_list *log, fwts_log_scan_func callback, fwts_log_progress_func progress, void *private, int *errors, bool remove_timestamp);
char *fwts_log_unique_label(const char *str, const char *label);
void       fwts_log_scan_patterns(fwts_framework *fw, char *line, int repeated, char *prevline, void *private, int *errors, const char *name, const char *advice);
fwts_compare_mode fwts_log_compare_mode_str_to_val(const char *str);
const char *fwts_json_str(fwts_framework *fw, const char *table, int index, json_object *obj, const char *key, bool log_error);
int         fwts_log_check(fwts_framework *fw, const char *table, fwts_log_scan_func fwts_log_scan_patterns, fwts_log_progress_func progress, fwts_list *log, int *errors, const char *json_data_path, const char *label, bool remove_timestamp);
int        fwts_log_regex_find(fwts_framework *fw, fwts_list *log, char *pattern, bool remove_timestamp);

#endif
