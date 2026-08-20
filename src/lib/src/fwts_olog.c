/*
 * Copyright (C) 2010-2026 Canonical
 * Some of this work - Copyright (C) 2016-2021 IBM
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
#include <errno.h>

#include "fwts.h"

/*
 *  OLOG pattern matching strings data file, data stored in json format
 */
#define OLOG_DATA_JSON_FILE		"olog.json"
#define MSGLOG_BUFFER_LINE		PATH_MAX

/* SPECIAL CASE USE for OPEN POWER opal Firmware LOGS */
static const char msglog[] = "/sys/firmware/opal/msglog";
static const char msglog_outfile[] = "/var/log/opal_msglog";

/*
 *  fwts_olog_read(fwts_framework *fw)
 *	read olog log and return as list of lines
 */
fwts_list *fwts_olog_read(fwts_framework *fw)
{
	fwts_list *list;
	char *buffer;
	struct stat filestat;
	long len;
	size_t read_actual = 0;
	size_t write_actual = 0;
	FILE *msglog_f;
	FILE *msglog_outfile_f;

	/*
	 * Special file handling to sequentially fread the sysfs msglog into
	 * a static buffer based on inodes in the stat.  The sysfs msglog has
	 * a 0 byte file size since it is a sysfs object.
	 * Real size of the sysfs msglog is not in the stat statistics
	 * Using the st_blksize (the preferred i/o blksize)
	 * st_blocks is zero so must fread block by block
	 */
	if (!(msglog_f = fopen(msglog, "r"))) {
		/* open the sysfs msglog for read only */
		if (errno == ENOENT) {
			/*
			 * If file does not exist, treat as non-fatal
			 * for non PPC devices that don't have the
			 * arch specific sys file.
			 */
			return NULL;
		}
		goto olog_common_exit;
	}

	if (fstat(fileno(msglog_f), &filestat)) {
		/*
		 * stat fails so not PPC with OPAL msglog and
		 * no -o OLOG sent
		 */
		(void)fclose(msglog_f);
		return NULL;
	}

	if ((len = filestat.st_blksize) < 1) {
		/* if any size at all continue */
		goto olog_cleanup_msglog;
	}

	if ((buffer = calloc(1, len + 1)) == NULL)
		goto olog_cleanup_msglog;

	if (!(msglog_outfile_f = fopen(msglog_outfile, "w"))) {
		/* create the output file for the sysfs msglog */
		free(buffer);
		goto olog_cleanup_msglog;
	}

	while (!feof(msglog_f)) {
		read_actual = fread(buffer, 1, len, msglog_f);
		buffer[read_actual] = '\0';

		if (read_actual == (size_t)len) {
			write_actual = fwrite(buffer, 1, len, msglog_outfile_f);
			if (!(write_actual == (size_t)len)) {
				free(buffer);
				goto olog_cleanup_common;
			}
		} else {
			if (feof(msglog_f)) {
				write_actual = fwrite(buffer, 1, read_actual, msglog_outfile_f);
				if (!(write_actual == read_actual)) {
					free(buffer);
					goto olog_cleanup_common;
				}
			} else {
				/*
				 * we didn't get a full read and file did not
				 * test for EOF so bail
				 */
				break;
			}
		}
	}

	free(buffer);
	(void)fclose(msglog_f);
	(void)fclose(msglog_outfile_f);

	/*
	 * Now work on the dumped out msglog as a real file system file
	 */
	if (!(msglog_outfile_f = fopen(msglog_outfile, "r")))
		goto olog_common_exit;

	if (fseek(msglog_outfile_f, 0, SEEK_END))
		goto olog_cleanup_msglog_outfile;

	if ((len = ftell(msglog_outfile_f)) == -1)
		goto olog_cleanup_msglog_outfile;

	if ((fseek(msglog_outfile_f, 0, SEEK_SET)) != 0)
		goto olog_cleanup_msglog_outfile;

	if ((buffer = calloc(1, len+1)) == NULL)
		goto olog_cleanup_msglog_outfile;

	read_actual = fread(buffer, 1, len, msglog_outfile_f);
	buffer[read_actual] = '\0';
	if (read_actual == (size_t)len) {
		list = fwts_list_from_text(buffer);
		free(buffer);
		(void)fclose(msglog_outfile_f);
		return list;
	} else {
		free(buffer);
		goto olog_cleanup_msglog_outfile;
	}

olog_cleanup_msglog_outfile:
	(void)fclose(msglog_outfile_f);
	goto olog_common_exit;

olog_cleanup_msglog:
	(void)fclose(msglog_f);
	goto olog_common_exit;

olog_cleanup_common:
	(void)fclose(msglog_f);
	(void)fclose(msglog_outfile_f);

olog_common_exit:
	fwts_log_error(fw, "Problem with the file handling on the"
		" default dumped OPAL msglog, %s, try running with"
		" sudo first then try using -o to specify a specific"
		" saved OPAL msglog for analysis.", msglog_outfile);
	return NULL;
}


static int fwts_olog_check(fwts_framework *fw,
	const char *table,
	fwts_olog_progress_func progress,
	fwts_list *olog,
	int *errors)
{
	int n, i, fd, ret = FWTS_ERROR;
	json_object *olog_objs, *olog_table;
	fwts_log_pattern *patterns;
	char json_data_path[PATH_MAX];

	if (fw->json_data_file) {
		snprintf(json_data_path, sizeof(json_data_path), "%s/%s",
			fw->json_data_path,(fw->json_data_file));
	} else {
		/* use the hard coded OLOG JSON as default */
		snprintf(json_data_path, sizeof(json_data_path), "%s/%s",
			fw->json_data_path, OLOG_DATA_JSON_FILE);
	}

	/*
	 * json_object_from_file() can fail when files aren't readable
	 * so check if we can open for read before calling json_object_from_file()
	 */
	if ((fd = open(json_data_path, O_RDONLY)) < 0) {
		fwts_log_error(fw, "Cannot read file %s, check the path and "
			"check that the file exists, you may need to specify "
			"-j or -J.", json_data_path);
		return FWTS_ERROR;
	}
	(void)close(fd);

	olog_objs = json_object_from_file(json_data_path);
	if (FWTS_JSON_ERROR(olog_objs)) {
		fwts_log_error(fw, "Cannot load olog data from %s.", json_data_path);
		return FWTS_ERROR;
	}

#if JSON_HAS_GET_EX
	if (!json_object_object_get_ex(olog_objs, table, &olog_table)) {
		fwts_log_error(fw, "Cannot fetch olog table object '%s' from %s.",
			table, json_data_path);
		goto fail_put;
	}
#else
	olog_table = json_object_object_get(olog_objs, table);
	if (FWTS_JSON_ERROR(olog_table)) {
		fwts_log_error(fw, "Cannot fetch olog table object '%s' from %s.",
			table, json_data_path);
		goto fail_put;
	}
#endif

	n = json_object_array_length(olog_table);

	/* Last entry is null to indicate end, so alloc n+1 items */
	if ((patterns = calloc(n+1, sizeof(fwts_log_pattern))) == NULL) {
		fwts_log_error(fw, "Cannot allocate pattern table.");
		goto fail_put;
	}

	/* Now fetch json objects and compile regex */
	for (i = 0; i < n; i++) {
		const char *str;
		json_object *obj;

		obj = json_object_array_get_idx(olog_table, i);
		if (FWTS_JSON_ERROR(obj)) {
			fwts_log_error(fw, "Cannot fetch %d item from table %s.",
				i, table);
			goto fail;
		}
		if ((str = fwts_json_str(fw, table, i, obj, "compare_mode", true)) == NULL)
			goto fail;
		patterns[i].compare_mode = fwts_klog_compare_mode_str_to_val(str);

		if ((str = fwts_json_str(fw, table, i, obj, "log_level", true)) == NULL)
			goto fail;
		patterns[i].level   = fwts_log_str_to_level(str);

		if ((patterns[i].pattern = fwts_json_str(fw, table, i, obj, "pattern", true)) == NULL)
			goto fail;

		if ((patterns[i].advice = fwts_json_str(fw, table, i, obj, "advice", true)) == NULL)
			goto fail;

		/* Labels appear in fwts 0.26.0, so are optional with older versions */
		str = fwts_json_str(fw, table, i, obj, "label", false);
		if (str) {
			patterns[i].label = strdup(str);
		} else {
			/* if not specified, auto-magically generate */
			patterns[i].label = strdup(fwts_klog_unique_label(patterns[i].pattern));
		}
		if (patterns[i].label == NULL)
			goto fail;

		if (patterns[i].compare_mode == FWTS_COMPARE_REGEX) {
			int rc;

			rc = regcomp(&patterns[i].compiled, patterns[i].pattern, REG_EXTENDED);
			if (rc) {
				fwts_log_error(fw, "Regex %s failed to compile: %d.", patterns[i].pattern, rc);
				patterns[i].compiled_ok = false;
			} else {
				patterns[i].compiled_ok = true;
			}
		}
	}
	/* We've now collected up the scan patterns, lets scan the log for errors */
	ret = fwts_klog_scan(fw, olog, fwts_klog_scan_patterns, progress, patterns, errors);

fail:
	for (i = 0; i < n; i++) {
		if (patterns[i].compiled_ok)
			regfree(&patterns[i].compiled);
		if (patterns[i].label)
			free(patterns[i].label);
	}
	free(patterns);
fail_put:
	json_object_put(olog_objs);

	return ret;
}

int fwts_olog_firmware_check(
	fwts_framework *fw,
	fwts_olog_progress_func progress,
	fwts_list *olog,
	int *errors)
{
	return fwts_olog_check(fw, "olog_error_warning_patterns",
		progress, olog, errors);
}
