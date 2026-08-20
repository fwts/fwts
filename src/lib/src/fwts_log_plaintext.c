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
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "fwts.h"

/*
 *  fwts_log_header_plaintext()
 *	format up a tabulated log heading
 */
static int fwts_log_header_plaintext(
	fwts_log_file *log_file,
	char *buffer,
	const int len,
	const fwts_log_field field,
	const fwts_log_level level)
{
	const char *ptr;
	int n = 0;
	struct tm tm;
	time_t now;

	time(&now);
	localtime_r(&now, &tm);

	for (ptr = fwts_log_format; *ptr; ) {
		if (*ptr == '%') {
			ptr++;
			if (!strncmp(ptr, "line", 4)) {
				n += snprintf(buffer + n, len - n,
					"%5.5" PRIu32, log_file->line_number);
				ptr += 4;
			}
			if (!strncmp(ptr, "date", 4)) {
				n += snprintf(buffer + n, len - n,
					"%2.2d/%2.2d/%-2.2d",
					tm.tm_mday, tm.tm_mon + 1, (tm.tm_year+1900) % 100);
				ptr += 4;
			}
			if (!strncmp(ptr, "time", 4)) {
				n += snprintf(buffer + n, len - n,
					"%2.2d:%2.2d:%2.2d",
					tm.tm_hour, tm.tm_min, tm.tm_sec);
				ptr += 4;
			}
			if (!strncmp(ptr, "field", 5)) {
				n += snprintf(buffer + n, len - n, "%s",
					fwts_log_field_to_str(field));
				ptr += 5;
			}
			if (!strncmp(ptr, "level", 5)) {
				n += snprintf(buffer + n, len - n, "%1.1s",
					fwts_log_level_to_str(level));
				ptr += 5;
			}
			if (!strncmp(ptr,"owner", 5) && log_file->log->owner) {
				n += snprintf(buffer + n, len - n, "%-15.15s", log_file->log->owner);
				ptr += 5;
			}
		} else {
			n += snprintf(buffer+n, len-n, "%c", *ptr);
			ptr++;
		}
	}
	return n;
}


/*
 *  fwts_log_print()
 *	print to a log
 */
static int fwts_log_print_plaintext(
	fwts_log_file *log_file,
	const fwts_log_field field,
	const fwts_log_level level,
	const char *status,	/* Ignored */
	const char *label,	/* Ignored */
	const char *prefix,
	const char *buffer)
{
	char tmpbuf[8192];
	int n = 0;
	int header_len;
	int len = 0;

	fwts_list *lines;
	fwts_list_link *item;

	FWTS_UNUSED(status);
	FWTS_UNUSED(label);

	if (!((field & LOG_FIELD_MASK) & fwts_log_filter))
		return 0;

	/* This is a pain, we neen to find out how big the leading log
	   message is, so format one up. */
	n = header_len = fwts_log_header_plaintext(log_file, tmpbuf, sizeof(tmpbuf), field, level);
	n += snprintf(tmpbuf + n, sizeof(tmpbuf) - n, "%s%s", prefix, buffer);

	/* Break text into multi-lines if necessary */
	if (field & LOG_VERBATUM)
		lines = fwts_list_from_text(tmpbuf + header_len);
	else
		lines = fwts_format_text(tmpbuf + header_len, log_file->line_width - header_len);

	if (!lines)
		return 0;
	len = n;

	fwts_list_foreach(item, lines) {
		char *text = fwts_text_list_text(item);

		if (!(field & LOG_NO_FIELDS)) {
			/* Re-format up a log heading with current line number which
	 		   may increment with multiple line log messages */
			fwts_log_header_plaintext(log_file, tmpbuf, sizeof(tmpbuf), field, level);
			fwrite(tmpbuf, 1, header_len, log_file->fp);
		}
		fwrite(text, 1, strlen(text), log_file->fp);
		fwrite("\n", 1, 1, log_file->fp);
		fflush(log_file->fp);
		log_file->line_number++;
		len += strlen(text) + 1;
	}
	fwts_text_list_free(lines);

	return len;
}

/*
 *  fwts_log_underline_plaintext()
 *	write an underline across log, using character ch as the underline
 */
static void fwts_log_underline_plaintext(fwts_log_file *log_file, const int ch)
{
	int n;
	char *buffer;
	size_t width = log_file->line_width + 1;

	if (!((LOG_SEPARATOR & LOG_FIELD_MASK) & fwts_log_filter))
		return;

	buffer = calloc(1, width);
	if (!buffer)
		return;	/* Unlikely, and just abort */

	/* Write in leading optional line prefix */
	n = fwts_log_header_plaintext(log_file, buffer, width, LOG_SEPARATOR, LOG_LEVEL_NONE);

	memset(buffer + n, ch, width  - n);
	buffer[width - 1] = '\n';

	fwrite(buffer, 1, width, log_file->fp);
	fflush(log_file->fp);
	log_file->line_number++;

	free(buffer);
}

/*
 *  fwts_log_newline_plaintext()
 *	write newline to log
 */
static void fwts_log_newline_plaintext(fwts_log_file *log_file)
{
	fwrite("\n", 1, 1, log_file->fp);
	fflush(log_file->fp);
	log_file->line_number++;
}

fwts_log_ops fwts_log_plaintext_ops = {
	.print = 	fwts_log_print_plaintext,
	.underline =	fwts_log_underline_plaintext,
	.newline =	fwts_log_newline_plaintext
};
