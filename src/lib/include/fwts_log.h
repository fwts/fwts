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

#ifndef __FWTS_LOG_H__
#define __FWTS_LOG_H__

#include <stdio.h>
#include <stdarg.h>

#include "fwts_framework.h"
#include "fwts_list.h"

#define LOG_MAGIC		(0xfe23ab13cb1ef754ULL)
#define LOG_MAX_BUF_SIZE	(4096)		/* Max output per log line */

typedef enum {
	LOG_NO_FIELD        = 0x00000000,
	LOG_RESULT	    = 0x00000001,
	LOG_ERROR           = 0x00000002,
	LOG_WARNING         = 0x00000004,
	LOG_DEBUG           = 0x00000008,
	LOG_INFO            = 0x00000010,
	LOG_SUMMARY         = 0x00000020,
	LOG_SEPARATOR       = 0x00000040,
	LOG_NEWLINE         = 0x00000080,
	LOG_ADVICE          = 0x00000100,
	LOG_HEADING	    = 0x00000200,
	LOG_PASSED	    = 0x00000400,
	LOG_FAILED	    = 0x00000800,
	LOG_SKIPPED	    = 0x00001000,
	LOG_ABORTED	    = 0x00002000,
	LOG_INFOONLY	    = 0x00004000, /* no-op */

	LOG_FIELD_MASK	    = 0x0000ffff,

	LOG_VERBATUM	    = 0x10000000,
	LOG_NO_FIELDS	    = 0x20000000
} fwts_log_field;

typedef enum {
	LOG_LEVEL_NONE	    = 0x00000000,
	LOG_LEVEL_CRITICAL  = 0x00000001,
	LOG_LEVEL_HIGH      = 0x00000002,
	LOG_LEVEL_MEDIUM    = 0x00000004,
	LOG_LEVEL_LOW       = 0x00000008,
	LOG_LEVEL_INFO      = 0x00000010,
	LOG_LEVEL_ALL       = 0x0000001f
} fwts_log_level;

/*
 *  different types of log file
 */
typedef enum {
	LOG_TYPE_NONE       = 0x00000000,
	LOG_TYPE_PLAINTEXT  = 0x00000001,
	LOG_TYPE_JSON       = 0x00000002,
	LOG_TYPE_XML        = 0x00000004,
	LOG_TYPE_HTML       = 0x00000008
} fwts_log_type;

/*
 *   different types of output log
 */
typedef enum {
	LOG_FILENAME_TYPE_STDOUT = 0x00000001,	/* log output to stdout */
	LOG_FILENAME_TYPE_STDERR = 0x00000002,	/* log output to stderr */
	LOG_FILENAME_TYPE_FILE   = 0x00000003	/* log output to a file */
} fwts_log_filename_type;

/*
 *  top level log descriptor
 */
typedef struct log_t {
	uint64_t magic;				/* magic ID of the log */
	fwts_list log_files;			/* list of fwts_log_file */
	char *owner;				/* who is writing to this log */
} fwts_log;

/*
 *  info for a specific log type
 */
typedef struct {
	FILE *fp;				/* file descriptor for log */
	uint32_t line_number;			/* keeps track of the line numbering */
	fwts_log *log;				/* parent log struct */
	fwts_log_type type;			/* log type */
	fwts_log_filename_type filename_type;	/* log filename type */
	struct fwts_log_ops_t *ops;		/* log operators */
	int line_width;				/* width of log in chars */
} fwts_log_file;

typedef struct fwts_log_ops_t {
	int (*print)(fwts_log_file *log_file, const fwts_log_field field, const fwts_log_level level, const char *status, const char *label, const char *prefix, const char *buffer);
	void (*underline)(fwts_log_file *log_file, int ch);
	void (*newline)(fwts_log_file *log_file);
	void (*section_begin)(fwts_log_file *log_file, const char *tag);
	void (*section_end)(fwts_log_file *log_file);
	void (*open)(fwts_log_file *log_file);
	void (*close)(fwts_log_file *log_file);
} fwts_log_ops;

extern fwts_log_ops fwts_log_plaintext_ops;
extern fwts_log_ops fwts_log_json_ops;
extern fwts_log_ops fwts_log_xml_ops;
extern fwts_log_ops fwts_log_html_ops;

extern fwts_log_field fwts_log_filter;
extern const char *fwts_log_format;

fwts_log *fwts_log_open(const char* owner, const char *name, const char *mode, const fwts_log_type);
int       fwts_log_close(fwts_log *log);
int       fwts_log_printf(const fwts_framework *fw, const fwts_log_field field, const fwts_log_level level, const char *status, const char *label, const char *prefix, const char *fmt, ...)
	__attribute__((format(printf, 7, 8)));
void      fwts_log_newline(fwts_log *log);
void      fwts_log_underline(fwts_log *log, const int ch);
void      fwts_log_set_field_filter(char *str);
int       fwts_log_set_owner(fwts_log *log, const char *owner);
void      fwts_log_set_format(const char *str);
void      fwts_log_print_fields(void);
void      fwts_log_filter_set_field(const fwts_log_field filter);
void      fwts_log_filter_unset_field(const fwts_log_field filter);
int       fwts_log_str_to_level(const char *str);
fwts_log_field fwts_log_str_to_field(const char *text);
char     *fwts_log_level_to_str(const fwts_log_level level);
char	 *fwts_log_field_to_str(const fwts_log_field field);
char     *fwts_log_field_to_str_full(const fwts_log_field field);
char	 *fwts_log_field_to_str_upper(const fwts_log_field field);
void	  fwts_log_set_line_width(const int width);
void	  fwts_log_section_begin(fwts_log *log, const char *name);
void	  fwts_log_section_end(fwts_log *log);
char     *fwts_log_get_filenames(const char *filename, const fwts_log_type type);
fwts_log_filename_type fwts_log_get_filename_type(const char *name);
void _fwts_log_info_simp_int(const fwts_framework *fw, const char *message, uint8_t size, uint64_t value);

static inline int fwts_log_type_count(fwts_log_type type)
{
	return __builtin_popcount(type);
}

#define fwts_log_result(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_RESULT, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_warning(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_WARNING, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_warning_verbatim(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_WARNING | LOG_VERBATUM, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_error(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_ERROR, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_error_verbatim(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_ERROR | LOG_VERBATUM, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_info(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_INFO, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_info_verbatim(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_INFO | LOG_VERBATUM, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_summary(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_SUMMARY, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_summary_verbatim(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_SUMMARY | LOG_VERBATUM, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_advice(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_ADVICE, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_heading(fw, fmt, args...)	\
	fwts_log_printf(fw, LOG_HEADING, LOG_LEVEL_NONE, "", "", "", fmt, ## args)

#define fwts_log_nl(fw) \
	fwts_log_printf(fw, LOG_NEWLINE, LOG_LEVEL_NONE, "", "", "", "%s", "")

#define fwts_log_info_simp_int(fw, message, value) \
	_fwts_log_info_simp_int(fw, message, sizeof(value), value)

#endif
