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
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#include "fwts.h"

#define LOG_LINE_WIDTH		(80)
#define LOG_UNKNOWN_FIELD	"???"

static int log_line_width = 0;

fwts_log_field fwts_log_filter = ~0;

const char *fwts_log_format = "";

/*
 *  fwts_log_set_line_width()
 * 	set width of a log
 */
void fwts_log_set_line_width(const int width)
{
	if ((width >= 80) && (width <= 1024))
		log_line_width = width;
}

/*
 *  fwts_log_field_to_str()
 *	return string name of log field
 */
char *fwts_log_field_to_str(const fwts_log_field field)
{
	switch (field & LOG_FIELD_MASK) {
	case LOG_RESULT:
		return "RES";
	case LOG_ERROR:
		return "ERR";
	case LOG_WARNING:
		return "WRN";
	case LOG_DEBUG:
		return "DBG";
	case LOG_INFO:
		return "INF";
	case LOG_SUMMARY:
		return "SUM";
	case LOG_SEPARATOR:
		return "SEP";
	case LOG_NEWLINE:
		return "NLN";
	case LOG_ADVICE:
		return "ADV";
	case LOG_HEADING:
		return "HED";
	case LOG_PASSED:
		return "PAS";
	case LOG_FAILED:
		return "FAL";
	case LOG_SKIPPED:
		return "SKP";
	case LOG_ABORTED:
		return "ABT";
	case LOG_INFOONLY:
		return "INO";
	default:
		return LOG_UNKNOWN_FIELD;
	}
}

/*
 *  fwts_log_field_to_str_full()
 *	return full string name of log field
 */
char *fwts_log_field_to_str_full(const fwts_log_field field)
{
	switch (field & LOG_FIELD_MASK) {
	case LOG_RESULT:
		return "Result";
	case LOG_ERROR:
		return "Error";
	case LOG_WARNING:
		return "Warning";
	case LOG_DEBUG:
		return "Debug";
	case LOG_INFO:
		return "Info";
	case LOG_SUMMARY:
		return "Summary";
	case LOG_SEPARATOR:
		return "Separator";
	case LOG_NEWLINE:
		return "Newline";
	case LOG_ADVICE:
		return "Advice";
	case LOG_HEADING:
		return "Heading";
	case LOG_PASSED:
		return "Passed";
	case LOG_FAILED:
		return "Failed";
	case LOG_SKIPPED:
		return "Skipped";
	case LOG_ABORTED:
		return "Aborted";
	case LOG_INFOONLY:
		return "Info Only";
	default:
		return "Unknown";
	}
}

/*
 *  fwts_log_field_to_str_full()
 *	return full string name of log field
 */
char *fwts_log_field_to_str_upper(const fwts_log_field field)
{
	static char str[16];
	char *ptr1;
	char *ptr2 = fwts_log_field_to_str_full(field);

	for (ptr1 = str; *ptr2; ptr1++, ptr2++)
		*ptr1 = toupper(*ptr2);

	*ptr1 = '\0';

	return str;
}

/*
 *  fwts_log_str_to_level()
 *	return log level mapped from the given string
 */
int fwts_log_str_to_level(const char *str)
{
	if (str) {
		if (strstr(str, "CRITICAL"))
			return LOG_LEVEL_CRITICAL;
		if (strstr(str, "HIGH"))
			return LOG_LEVEL_HIGH;
		if (strstr(str, "MEDIUM"))
			return LOG_LEVEL_MEDIUM;
		if (strstr(str, "LOW"))
			return LOG_LEVEL_LOW;
		if (strstr(str, "INFO"))
			return LOG_LEVEL_INFO;
	}

	return LOG_LEVEL_MEDIUM;	/* Default */
}

/*
 *  fwts_log_level_to_str()
 *	return string name of log level
 */
char *fwts_log_level_to_str(const fwts_log_level level)
{
	switch (level) {
	case LOG_LEVEL_CRITICAL:
		return "CRITICAL";
	case LOG_LEVEL_HIGH:
		return "HIGH";
	case LOG_LEVEL_MEDIUM:
		return "MEDIUM";
	case LOG_LEVEL_LOW:
		return "LOW";
	case LOG_LEVEL_INFO:
		return "INFO";
	case LOG_LEVEL_NONE:
	default:
		return " ";
	}
}

/*
 *  fwts_log_print_fields()
 *	dump out available log field names
 */
void fwts_log_print_fields(void)
{
	fwts_log_field field;

	printf("Available fields: ");
	for (field = 1; field <= LOG_NO_FIELDS; field <<= 1) {
		char *str = fwts_log_field_to_str(field);
		if (strcmp(str, LOG_UNKNOWN_FIELD) == 0)
			break;
		printf("%s%s", field == 1 ? "" : ",", str);
	}
	printf("\n");
}

/*
 *  fwts_log_str_to_field()
 *	return log field of a given string, 0 if not matching
 */
fwts_log_field fwts_log_str_to_field(const char *text)
{
	int i;

	static const struct mapping {
		char *text;
		fwts_log_field field;
	} mappings[] = {
		{ "RES", LOG_RESULT },
		{ "ERR", LOG_ERROR },
		{ "WRN", LOG_WARNING },
		{ "DBG", LOG_DEBUG },
		{ "INF", LOG_INFO },
		{ "SUM", LOG_SUMMARY },
		{ "SEP", LOG_SEPARATOR },
		{ "NLN", LOG_NEWLINE },
		{ "ADV", LOG_ADVICE },
		{ "HED", LOG_HEADING },
		{ "PAS", LOG_PASSED },
		{ "FAL", LOG_FAILED },
		{ "SKP", LOG_SKIPPED },
		{ "ABT", LOG_ABORTED },
		{ "INO", LOG_INFOONLY },
		{ "ALL", ~0 },
		{ NULL, 0 }
	};

	for (i = 0; mappings[i].text != NULL; i++)
		if (strcmp(mappings[i].text, text) == 0)
			return mappings[i].field;
	return LOG_NO_FIELD;
}

/*
 *  fwts_log_filter_set_field
 *	set a filter bit map
 */
void fwts_log_filter_set_field(const fwts_log_field filter)
{
	fwts_log_filter |= filter;
}

/*
 *  fwts_log_filter_unset_field
 *	clear a filter bit map
 */
void fwts_log_filter_unset_field(const fwts_log_field filter)
{
	fwts_log_filter &= ~filter;
}

/*
 *  fwts_log_set_field_filter
 *	set/clear filter bit map based on a comma separated
 *	list of field names. ~ or ^ prefix to a field name
 *	clears a bit.
 */
void fwts_log_set_field_filter(char *str)
{
	char *saveptr;
	fwts_log_field field;

	for (;; str=NULL) {
		char *token;

		if ((token = strtok_r(str, ",|", &saveptr)) == NULL)
			break;
		if (*token == '^' || *token == '~') {
			field = fwts_log_str_to_field(token+1);
			if (field != LOG_NO_FIELD)
				fwts_log_filter_unset_field(field);
		}
		else {
			field = fwts_log_str_to_field(token);
			if (field != LOG_NO_FIELD)
				fwts_log_filter_set_field(field);
		}
	}
}

/*
 *  fwts_log_set_format()
 *	set the log format string
 */
void fwts_log_set_format(const char *str)
{
	fwts_log_format = str;
}

/*
 *  fwts_log_type_filename_suffix()
 *	return a filename suffix on a given log type
 */
static char *fwts_log_type_filename_suffix(const fwts_log_type type)
{
	switch (type) {
	case LOG_TYPE_JSON:
		return ".json";
	case LOG_TYPE_XML:
		return ".xml";
	case LOG_TYPE_HTML:
		return ".html";
	case LOG_TYPE_NONE:
	case LOG_TYPE_PLAINTEXT:
	default:
		return ".log";
	}
}

/*
 *  fwts_log_filename_new_suffix()
 *	return the log name with suffix based on log type
 */
static char *fwts_log_filename(const char *filename, const fwts_log_type type)
{
	char *ptr;
	char *new_name;
	char *suffix;
	size_t suffix_len;
	size_t filename_len;
	struct stat stat_buf;

	/*
	 *  If the user specified a char special file, like /dev/null
	 *  or a named pipe, socket or symlink we should just return
	 * that instead.
	 */
	if (stat(filename, &stat_buf) == 0) {
		if (S_ISCHR(stat_buf.st_mode) ||
		    S_ISFIFO(stat_buf.st_mode) ||
		    S_ISSOCK(stat_buf.st_mode) ||
		    S_ISLNK(stat_buf.st_mode))
			return strdup(filename);
	}

	suffix = fwts_log_type_filename_suffix(type);
	suffix_len = strlen(suffix);

	/*
	 * Locate an existing suffix, if it is one we recognise
	 * then remove it and append the appropriate one
	 */
	ptr = rindex(filename, '.');
	if (ptr &&
		(!strcmp(ptr, ".log") ||
		 !strcmp(ptr, ".json") ||
		 !strcmp(ptr, ".xml") ||
		 !strcmp(ptr, ".html"))) {
		size_t trunc_len = ptr - filename;

		if ((new_name = calloc(trunc_len + suffix_len + 1, 1)) == NULL) {
			fprintf(stderr, "Cannot allocate log name.\n");
			return NULL;
		}
		strncpy(new_name, filename, trunc_len);
		strcat(new_name, suffix); /* strcat OK because calloc zero'd all of new_name */
		return new_name;
	}

	/*
	 * We didn't find a suffix or a known suffix, so append
	 * the appropriate one to the given log filename
	 */
	filename_len = strlen(filename);
	if ((new_name = calloc(filename_len + suffix_len + 1, 1)) == NULL) {
		fprintf(stderr, "Cannot allocate log name.\n");
		return NULL;
	}

	strcpy(new_name, filename);
	strcat(new_name, suffix);

	return new_name;
}

/*
 *  fwts_log_printf()
 *	printf to a log
 */
int fwts_log_printf(
	const fwts_framework *fw,
	const fwts_log_field field,
	const fwts_log_level level,
	const char *status,
	const char *label,
	const char *prefix,
	const char *fmt, ...)

{
	int ret = 0;
	fwts_log *log = fw->results;

	if (!((field & LOG_FIELD_MASK) & fwts_log_filter))
		return ret;
	if (FWTS_LEVEL_IGNORE(fw, level))
		return ret;

	if (log && log->magic == LOG_MAGIC) {
		char buffer[LOG_MAX_BUF_SIZE];
		va_list	args;

		/*
		 * With the possibility of having multiple logs being written
		 * to per call of fwts_log_printf() it is more efficient to
		 * vsnprintf() here and then pass the formatted output down to
		 * each log handler rather than re-formatting each time in each
		 * handler
		 */
		va_start(args, fmt);
		ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
		if (ret >= 0) {
			fwts_list_link *item;

			fwts_list_foreach(item, &log->log_files) {
				fwts_log_file *log_file = fwts_list_data(fwts_log_file *, item);

				if (log_file->ops && log_file->ops->print)
					log_file->ops->print(log_file, field, level,
						status, label, prefix, buffer);
			}
		}
		va_end(args);
	}
	return ret;
}


/*
 *  _fwts_log_info_simp_int()
 *	simplify printing integers with different sizes
 */
void _fwts_log_info_simp_int(
	const fwts_framework *fw,
	const char *message,
	uint8_t size,
	uint64_t value)
{
	switch (size) {
		case sizeof(uint8_t):
			fwts_log_info_verbatim(fw, "%s0x%2.2" PRIx8, message, (uint8_t)value);
			break;
		case sizeof(uint16_t):
			fwts_log_info_verbatim(fw, "%s0x%4.4" PRIx16, message, (uint16_t)value);
			break;
		case sizeof(uint32_t):
			fwts_log_info_verbatim(fw, "%s0x%8.8" PRIx32, message, (uint32_t)value);
			break;
		case sizeof(uint64_t):
			fwts_log_info_verbatim(fw, "%s0x%16.16" PRIx64, message, value);
			break;
	}
}

/*
 *  fwts_log_underline()
 *	write an underline across log, using character ch as the underline
 */
void fwts_log_underline(fwts_log *log, const int ch)
{
	if (log && log->magic == LOG_MAGIC) {
		fwts_list_link *item;

		fwts_list_foreach(item, &log->log_files) {
			fwts_log_file *log_file = fwts_list_data(fwts_log_file *, item);

			if (log_file->ops && log_file->ops->underline)
				log_file->ops->underline(log_file, ch);
		}
	}
}

/*
 *  fwts_log_newline()
 *	write newline to log
 */
void fwts_log_newline(fwts_log *log)
{
	if (log && log->magic == LOG_MAGIC) {
		fwts_list_link *item;

		fwts_list_foreach(item, &log->log_files) {
			fwts_log_file *log_file = fwts_list_data(fwts_log_file *, item);

			if (log_file->ops && log_file->ops->newline)
				log_file->ops->newline(log_file);
		}
	}
}

int fwts_log_set_owner(fwts_log *log, const char *owner)
{
	if (log && (log->magic == LOG_MAGIC)) {
		char *newowner = strdup(owner);
		if (newowner) {
			free(log->owner);
			log->owner = newowner;
			return FWTS_OK;
		}
	}
	return FWTS_ERROR;
}


/*
 *  fwts_log_section_begin()
 *	mark a start of a named section.  For structured logging
 *	such as XML and JSON this pushes a new named tagged section
 */
void fwts_log_section_begin(fwts_log *log, const char *name)
{
	if (log && log->magic == LOG_MAGIC) {
		fwts_list_link *item;

		fwts_list_foreach(item, &log->log_files) {
			fwts_log_file *log_file = fwts_list_data(fwts_log_file *, item);

			if (log_file->ops && log_file->ops->section_begin)
				log_file->ops->section_begin(log_file, name);
		}
	}
}

/*
 *  fwts_log_section_end()
 *	mark end of a named section.  For structured logging
 *	such as XML and JSON this pops the end of a tagged section
 */
void fwts_log_section_end(fwts_log *log)
{
	if (log && log->magic == LOG_MAGIC) {
		fwts_list_link *item;

		fwts_list_foreach(item, &log->log_files) {
			fwts_log_file *log_file = fwts_list_data(fwts_log_file *, item);

			if (log_file->ops && log_file->ops->section_end)
				log_file->ops->section_end(log_file);
		}
	}
}

/*
 *  fwts_log_get_ops()
 *	return log ops basedon log type
 */
static fwts_log_ops *fwts_log_get_ops(const fwts_log_type type)
{
	switch (type) {
	case LOG_TYPE_JSON:
		return &fwts_log_json_ops;
	case LOG_TYPE_PLAINTEXT:
		return &fwts_log_plaintext_ops;
	case LOG_TYPE_XML:
		return &fwts_log_xml_ops;
	case LOG_TYPE_HTML:
		return &fwts_log_html_ops;
	case LOG_TYPE_NONE:
	default:
		return &fwts_log_plaintext_ops;
	}
}

/*
 *  fwts_log_get_filename_type()
 *	determine the filename type
 */
fwts_log_filename_type fwts_log_get_filename_type(const char *filename)
{
	if (!strcmp(filename, "stderr"))
		return LOG_FILENAME_TYPE_STDERR;
	else if (!strcmp(filename, "stdout"))
		return LOG_FILENAME_TYPE_STDOUT;
	else
		return LOG_FILENAME_TYPE_FILE;
}

/*
 *  fwts_log_filenames()
 *	return string of all the log filenames that will be used
 */
char *fwts_log_get_filenames(const char *filename, const fwts_log_type type)
{
	unsigned int i;
	char *filenames = NULL;
	char *tmp;
	size_t len = 0;

	for (i = 0; i < 32; i++) {
		fwts_log_type mask = 1UL << i;
		if (type & mask) {
			if ((tmp = fwts_log_filename(filename, mask)) == NULL) {
				free(filenames);
				return NULL;
			}

			if (filenames) {
				char *new_filenames;

				len += strlen(tmp) + 2;
				if ((new_filenames = realloc(filenames, len)) == NULL) {
					free(filenames);
					free(tmp);
					return NULL;
				} else
					filenames = new_filenames;

				strcat(filenames, " ");
				strcat(filenames, tmp);
			} else {
				len = strlen(tmp) + 1;
				if ((filenames = malloc(len)) == NULL) {
					free(tmp);
					return NULL;
				}
				strcpy(filenames, tmp);
			}
			free(tmp);
		}
	}

	return filenames;
}

/*
 *  fwts_log_open()
 *	open a log file. if name is stderr or stdout, then attach log to these
 *	streams.
 */
fwts_log *fwts_log_open(
	const char *owner,	/* Creator of the log */
	const char *filename,	/* Log file name */
	const char *mode,	/* open mode, see fopen() modes */
	const fwts_log_type type) /* Log type */
{
	fwts_log *newlog;
	unsigned int i;
	char *newname;

	if ((newlog = calloc(1, sizeof(fwts_log))) == NULL)
		return NULL;

	newlog->magic = LOG_MAGIC;

	fwts_log_set_owner(newlog, owner);
	fwts_list_init(&newlog->log_files);

	/*
	 *  Scan through and see which log types have been specified
	 *  and open the log file with the appropriate ops to perform
	 *  the logging
	 */
	for (i = 0; i < 32; i++) {
		fwts_log_type mask = 1UL << i;	/* The log type for this iteration */

		/* If set then go and open up a log for this log type */
		if (type & mask) {
			fwts_log_file *log_file;

			if ((log_file = calloc(1, sizeof(fwts_log_file))) == NULL) {
				fwts_log_close(newlog);
				return NULL;
			}

			log_file->type = mask;
			log_file->ops  = fwts_log_get_ops(mask);
			log_file->log  = newlog;
			log_file->filename_type = fwts_log_get_filename_type(filename);

			/*
			 *  To complicate matters we can have logs being
			 *  written to stderr, stdout or two a named file
			 */
			switch(log_file->filename_type) {
			case LOG_FILENAME_TYPE_STDERR:
				log_file->fp = stderr;
				break;
			case LOG_FILENAME_TYPE_STDOUT:
				log_file->fp = stdout;
				break;
			case LOG_FILENAME_TYPE_FILE:
				if ((newname = fwts_log_filename(filename, mask)) == NULL) {
					fwts_log_close(newlog);
					free(log_file);
					return NULL;
				}
				log_file->fp = fopen(newname, mode);
				free(newname);

				if (log_file->fp == NULL) {
					fwts_log_close(newlog);
					free(log_file);
					return NULL;
				}
			}

			/* Fix up the log specific line width */
			if (log_line_width) {
				/* User has specified width, so use it */
				log_file->line_width = log_line_width;
			} else {
				log_file->line_width =
					fwts_tty_width(fileno(log_file->fp), LOG_LINE_WIDTH);
			}

			/* ..and add the log file to the list of logs */
			fwts_list_append(&newlog->log_files, log_file);

			/* ..and do the log specific opening set up */
			if (log_file->ops && log_file->ops->open)
				log_file->ops->open(log_file);
		}
	}

	return newlog;
}

/*
 *  fwts_log_close()
 *	close any opened log files, free up memory
 */
int fwts_log_close(fwts_log *log)
{
	if (log && (log->magic == LOG_MAGIC)) {
		fwts_list_link *item;

		fwts_list_foreach(item, &log->log_files) {
			fwts_log_file *log_file = fwts_list_data(fwts_log_file *, item);

			/* Do the log type specific close */
			if (log_file->ops && log_file->ops->close)
				log_file->ops->close(log_file);

			/* Close opened log file */
			if (log_file->fp &&
			    log_file->filename_type == LOG_FILENAME_TYPE_FILE)
				(void)fclose(log_file->fp);
		}

		/* ..and free log files */
		fwts_list_free_items(&log->log_files, free);

		free(log->owner);
		free(log);
	}
	return FWTS_OK;
}
