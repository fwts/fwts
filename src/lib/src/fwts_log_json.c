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
#define LOG_LINE_WIDTH 100

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "fwts.h"

#define MAX_JSON_STACK	(64)

typedef struct {
	json_object	*obj;
	json_object	*log;
} fwts_log_json_stack_t;

static fwts_log_json_stack_t json_stack[MAX_JSON_STACK];
static int json_stack_index = 0;

/*
 *  fwts_log_out_of_memory_json()
 *	handle out of memory from json objects
 */
static void fwts_log_out_of_memory_json(void)
{
	fprintf(stderr, "Out of memory allocating a json object.\n");
	exit(EXIT_FAILURE);
}

/*
 *  fwts_log_printf_json()
 *	print to a log
 */
static int fwts_log_print_json(
	fwts_log_file *log_file,
	const fwts_log_field field,
	const fwts_log_level level,
	const char *status,
	const char *label,
	const char *prefix,
	const char *buffer)
{
	char tmpbuf[4096];
	struct tm tm;
	time_t now;
	json_object *header;
	json_object *json_log = (json_object*)json_stack[json_stack_index-1].log;
	json_object *obj;
	char *str;

	FWTS_UNUSED(prefix);

	if (!((field & LOG_FIELD_MASK) & fwts_log_filter))
		return 0;

	if (field & (LOG_NEWLINE | LOG_SEPARATOR | LOG_DEBUG))
		return 0;

	time(&now);
	localtime_r(&now, &tm);

	if ((header = json_object_new_object()) == NULL)
		fwts_log_out_of_memory_json();

	if ((obj = json_object_new_int(log_file->line_number)) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "line_num", obj);

	snprintf(tmpbuf, sizeof(tmpbuf), "%2.2d/%2.2d/%-2.2d",
		tm.tm_mday, tm.tm_mon + 1, (tm.tm_year+1900) % 100);
	if ((obj = json_object_new_string(tmpbuf)) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "date", obj);

	snprintf(tmpbuf, sizeof(tmpbuf), "%2.2d:%2.2d:%2.2d",
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	if ((obj = json_object_new_string(tmpbuf)) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "time", obj);

	if ((obj = json_object_new_string(fwts_log_field_to_str_full(field))) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "field_type", obj);

	str = fwts_log_level_to_str(level);
	if (!strcmp(str, " "))
		str = "None";
	if ((obj = json_object_new_string(str)) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "level", obj);

	if ((obj = json_object_new_string(*status ? status : "None")) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "status", obj);

	if ((obj = json_object_new_string(label && *label ? label : "None")) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "failure_label", obj);

	/* Redundant really
	if ((obj = json_object_new_string(log->owner)) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "owner", obj);
	*/

	if ((obj = json_object_new_string(buffer)) == NULL)
		fwts_log_out_of_memory_json();
	json_object_object_add(header, "log_text", obj);

	json_object_array_add(json_log, header);
	log_file->line_number++;	/* This is academic really */

	return 0;
}

/*
 *  fwts_log_underline_json()
 *	write an underline across log, using character ch as the underline
 */
static void fwts_log_underline_json(fwts_log_file *log_file, const int ch)
{
	FWTS_UNUSED(log_file);
	FWTS_UNUSED(ch);

	/* No-op for json */
}

/*
 *  fwts_log_newline()
 *	write newline to log
 */
static void fwts_log_newline_json(fwts_log_file *log_file)
{
	FWTS_UNUSED(log_file);

	/* No-op for json */
}

static void fwts_log_section_begin_json(fwts_log_file *log_file, const char *name)
{
	json_object *json_obj;
	json_object *json_log;

	FWTS_UNUSED(log_file);

	if ((json_obj = json_object_new_object()) == NULL)
		fwts_log_out_of_memory_json();

	if ((json_log = json_object_new_array()) == NULL)
		fwts_log_out_of_memory_json();

	/*
	 * unfortunately json_object_object_add can fail on
	 * a strdup, but it doesn't check this and doesn't
	 * tell us about this either. Bit lame really.
	 */
	json_object_object_add(json_obj, name, json_log);


	if (json_stack_index > 0)
		if (json_object_array_add(json_stack[json_stack_index-1].log, json_obj) != 0)
			fwts_log_out_of_memory_json();

	if (json_stack_index < MAX_JSON_STACK) {
		json_stack[json_stack_index].obj = json_obj;
		json_stack[json_stack_index].log = json_log;
		json_stack_index++;
	} else  {
		fprintf(stderr, "json log stack overflow pushing section %s.\n", name);
		exit(EXIT_FAILURE);
	}
}

static void fwts_log_section_end_json(fwts_log_file *log_file)
{
	FWTS_UNUSED(log_file);

	if (json_stack_index > 0)
		json_stack_index--;
	else {
		fprintf(stderr, "json log stack underflow.\n");
		exit(EXIT_FAILURE);
	}
}

static void fwts_log_open_json(fwts_log_file *log_file)
{
	fwts_log_section_begin_json(log_file, "fwts");
}

static void fwts_log_close_json(fwts_log_file *log_file)
{
	char *str;

	fwts_log_section_end_json(log_file);

#ifdef JSON_C_TO_STRING_PRETTY
	str = json_object_to_json_string_ext(
		json_stack[0].obj, JSON_C_TO_STRING_PRETTY);
#else
	str = json_object_to_json_string(json_stack[0].obj);
#endif
	if (str == NULL) {
		/* Let's not make this bail out as user may be logging to other files too */
		fprintf(stderr, "Cannot turn json object to text for output. Empty json output\n");
	} else {
		size_t len = strlen(str);

		fwrite(str, 1, len, log_file->fp);
		fwrite("\n", 1, 1, log_file->fp);
		fflush(log_file->fp);
		log_file->line_number++;
	}
	json_object_put(json_stack[0].obj);
	free(str);
}

fwts_log_ops fwts_log_json_ops = {
	.print = 	 fwts_log_print_json,
	.underline =	 fwts_log_underline_json,
	.newline =	 fwts_log_newline_json,
	.section_begin = fwts_log_section_begin_json,
	.section_end   = fwts_log_section_end_json,
	.open          = fwts_log_open_json,
	.close	       = fwts_log_close_json
};
