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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "fwts.h"

#define MAX_XML_STACK	(64)
#define XML_INDENT	(4)

typedef struct {
	const char *name;
} fwts_log_xml_stack_t;

static fwts_log_xml_stack_t xml_stack[MAX_XML_STACK];
static int xml_stack_index = 0;

/*
 *  fwts_log_print_xml()
 *	print to a log
 */
static int fwts_log_print_xml(
	fwts_log_file *log_file,
	const fwts_log_field field,
	const fwts_log_level level,
	const char *status,
	const char *label,
	const char *prefix,
	const char *buffer)
{
	struct tm tm;
	time_t now;
	char *str;

	FWTS_UNUSED(prefix);

	if (!((field & LOG_FIELD_MASK) & fwts_log_filter))
		return 0;

	if (field & (LOG_NEWLINE | LOG_SEPARATOR | LOG_DEBUG))
		return 0;

	time(&now);
	localtime_r(&now, &tm);

	fprintf(log_file->fp, "%*s<logentry>\n", xml_stack_index * XML_INDENT, "");

	fprintf(log_file->fp, "%*s<line_num>%" PRIu32 "</line_num>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", log_file->line_number);

	fprintf(log_file->fp, "%*s<date>%2.2d/%2.2d/%-2.2d</date>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", tm.tm_mday, tm.tm_mon + 1, (tm.tm_year+1900) % 100);

	fprintf(log_file->fp, "%*s<time>%2.2d:%2.2d:%2.2d</time>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", tm.tm_hour, tm.tm_min, tm.tm_sec);

	fprintf(log_file->fp, "%*s<field_type>%s</field_type>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", fwts_log_field_to_str_full(field));

	str = fwts_log_level_to_str(level);
	if (!strcmp(str, " "))
		str = "None";

	fprintf(log_file->fp, "%*s<level>%s</level>\n",
		(xml_stack_index + 1) * XML_INDENT, "", str);

	fprintf(log_file->fp, "%*s<status>%s</status>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", *status ? status : "None");

	fprintf(log_file->fp, "%*s<failure_label>%s</failure_label>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", label && *label ? label : "None");

	fprintf(log_file->fp, "%*s<log_text>%s</log_text>\n",
		(xml_stack_index + 1) * XML_INDENT,
		"", buffer);

	fprintf(log_file->fp, "%*s</logentry>\n", xml_stack_index * XML_INDENT, "");
	fflush(log_file->fp);
	log_file->line_number++;

	return 0;
}

/*
 *  fwts_log_underline_xml()
 *	write an underline across log, using character ch as the underline
 */
static void fwts_log_underline_xml(fwts_log_file *log_file, const int ch)
{
	FWTS_UNUSED(log_file);
	FWTS_UNUSED(ch);

	/* No-op for xml */
}

/*
 *  fwts_log_newline()
 *	write newline to log
 */
static void fwts_log_newline_xml(fwts_log_file *log_file)
{
	FWTS_UNUSED(log_file);

	/* No-op for xml */
}

static void fwts_log_section_begin_xml(fwts_log_file *log_file, const char *name)
{
	fprintf(log_file->fp, "%*s<%s>\n", xml_stack_index * XML_INDENT, "", name);
	fflush(log_file->fp);

	if (xml_stack_index < MAX_XML_STACK) {
		xml_stack[xml_stack_index].name = name;
		xml_stack_index++;
	} else  {
		fprintf(stderr, "xml log stack overflow pushing section %s.\n", name);
		exit(EXIT_FAILURE);
	}
}

static void fwts_log_section_end_xml(fwts_log_file *log_file)
{
	if (xml_stack_index > 0) {
		xml_stack_index--;
		fprintf(log_file->fp, "%*s</%s>\n", xml_stack_index * XML_INDENT,
			"", xml_stack[xml_stack_index].name);
		fflush(log_file->fp);
	} else {
		fprintf(stderr, "xml log stack underflow.\n");
		exit(EXIT_FAILURE);
	}

}

static void fwts_log_open_xml(fwts_log_file *log_file)
{
	char *xml_header = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";

	fwrite(xml_header, 1, strlen(xml_header), log_file->fp);
	fflush(log_file->fp);

	fwts_log_section_begin_xml(log_file, "fwts");
}

static void fwts_log_close_xml(fwts_log_file *log_file)
{
	fwts_log_section_end_xml(log_file);

	fwrite("\n", 1, 1, log_file->fp);
	fflush(log_file->fp);
	log_file->line_number++;
}

fwts_log_ops fwts_log_xml_ops = {
	.print = 	 fwts_log_print_xml,
	.underline =	 fwts_log_underline_xml,
	.newline =	 fwts_log_newline_xml,
	.section_begin = fwts_log_section_begin_xml,
	.section_end   = fwts_log_section_end_xml,
	.open          = fwts_log_open_xml,
	.close	       = fwts_log_close_xml
};
