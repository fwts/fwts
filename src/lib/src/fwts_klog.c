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

#include <sys/klog.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <fcntl.h>

#include "fwts.h"

/*
 *  klog pattern matching strings data file, data stored in json format
 */
#define KLOG_DATA_JSON_FILE		"klog.json"

/*
 *  match unique strings in the kernel log
 */
#define UNIQUE_KLOG_LABEL		"Klog"

/*
 *  fwts_klog_free()
 *	free kernel log list
 */
void fwts_klog_free(fwts_list *klog)
{
	fwts_log_free(klog);
}

/*
 *  fwts_klog_find_changes()
 *	find new lines added to kernel log, clone them from new list
 *	must be freed with fwts_list_free(klog_diff, NULL);
 */
fwts_list *fwts_klog_find_changes(fwts_list *klog_old, fwts_list *klog_new)
{
	return fwts_log_find_changes(klog_old, klog_new);
}

/*
 *  fwts_klog_read()
 *	read kernel log and return as list of lines
 */
fwts_list *fwts_klog_read(void)
{
	int len;
	char *buffer;
	fwts_list *list;

	if ((len = klogctl(10, NULL, 0)) < 0)
		return NULL;

	if ((buffer = calloc(1, len)) == NULL)
		return NULL;

	if (klogctl(3, buffer, len) <= 0) {
		free(buffer);
		return NULL;
	}

	list = fwts_list_from_text(buffer);
	free(buffer);

	return list;
}

char *fwts_klog_remove_timestamp(char *text)
{
	return fwts_log_remove_timestamp(text);
}

int fwts_klog_scan(fwts_framework *fw,
	fwts_list *klog,
	fwts_klog_scan_func scan_func,
	fwts_klog_progress_func progress_func,
	void *private,
	int *match)
{
	return fwts_log_scan(fw, klog, scan_func, progress_func, private, match, true);
}

char *fwts_klog_unique_label(const char *str)
{
	return fwts_log_unique_label(str, UNIQUE_KLOG_LABEL);
}

void fwts_klog_scan_patterns(fwts_framework *fw,
	char *line,
	int  repeated,
	char *prevline,
	void *private,
	int *errors)
{
    static char *advice =
        "This is a bug picked up by the kernel, but as yet, the "
        "firmware test suite has no diagnostic advice for this particular problem.";

    fwts_log_scan_patterns(fw, line, repeated, prevline, private, errors, "Kernel", advice);
}

/*
 *  fwts_klog_compare_mode_str_to_val()
 *	convert compare mode strings (from json database) to compare_mode values
 */
fwts_compare_mode fwts_klog_compare_mode_str_to_val(const char *str)
{
	return fwts_log_compare_mode_str_to_val(str);
}

static int fwts_klog_check(fwts_framework *fw,
	const char *table,
	fwts_klog_progress_func progress,
	fwts_list *klog,
	int *errors)
{
	char json_data_path[PATH_MAX];

	if (fw->json_data_file) {
		snprintf(json_data_path, sizeof(json_data_path), "%s/%s", fw->json_data_path,(fw->json_data_file));
	}
	else { /* use the hard coded KLOG JSON as default */
		snprintf(json_data_path, sizeof(json_data_path), "%s/%s", fw->json_data_path, KLOG_DATA_JSON_FILE);
	}
	return fwts_log_check(fw, table, fwts_klog_scan_patterns, progress, klog, errors, json_data_path, UNIQUE_KLOG_LABEL, true);
}

int fwts_klog_firmware_check(fwts_framework *fw, fwts_klog_progress_func progress,
	fwts_list *klog, int *errors)
{
	return fwts_klog_check(fw, "firmware_error_warning_patterns",
		progress, klog, errors);
}

int fwts_klog_pm_check(fwts_framework *fw, fwts_klog_progress_func progress,
	fwts_list *klog, int *errors)
{
	return fwts_klog_check(fw, "pm_error_warning_patterns",
		progress, klog, errors);
}

/*
 * fwts_klog_regex_find()
 * 	scan a kernel log list of lines for a given regex pattern
 *	uses fwts_klog_regex_find_callback() callback
 */
int fwts_klog_regex_find(fwts_framework *fw, fwts_list *klog, char *pattern)
{
    return fwts_log_regex_find(fw, klog, pattern, true);
}

/*
 * fwts_klog_write()
 *	write a message to the kernel log
 */
int fwts_klog_write(fwts_framework *fw, const char *msg)
{
	FILE *fp;

	if ((fp = fopen("/dev/kmsg", "w")) == NULL) {
		fwts_log_info(fw, "Cannot write to kernel log /dev/kmsg.");
		return FWTS_ERROR;
	}

	fprintf(fp, "<7>fwts: %s", msg);
	fflush(fp);
	(void)fclose(fp);

	return FWTS_OK;
}
