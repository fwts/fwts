/*
 * Copyright (C) 2011-2026 Canonical
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdbool.h>
#include <getopt.h>

#include "fwts.h"

#define FWTS_ARGS_WIDTH         (28)
#define FWTS_MIN_TTY_WIDTH      (50)

/*
 *  Internal options table, we keep a list of all of the added options and keep a tally
 *  of how many options there are in each table.
 */
typedef struct {
	fwts_option *options;			/* options array */
	int             num_options;		/* number of options */
	fwts_args_optarg_handler optarg_handler;/* options handler */
	fwts_args_optarg_check   optarg_check;	/* options checker */
} fwts_options_table;

static fwts_list options_list;
static int	 total_options;
static bool	 options_init = false;

/*
 *  fwts_args_init()
 *	initialise.
 */
int fwts_args_init(void)
{
	if (!options_init) {
		fwts_list_init(&options_list);
		total_options = 0;
		options_init = true;
	}

	return FWTS_OK;
}

/*
 *  fwts_args_add_options()
 *	add a table of options and handler for these options
 */
int fwts_args_add_options(
	fwts_option *options,
	const fwts_args_optarg_handler handler,
	const fwts_args_optarg_check check)
{
	int n;
	fwts_options_table *options_table;

	if (!options_init)
		(void)fwts_args_init();

	if ((options_table = calloc(1, sizeof(fwts_options_table))) == NULL)
		return FWTS_ERROR;

	for (n = 0; options[n].long_name != NULL; n++)
		;

	total_options += n;
	options_table->num_options = n;
	options_table->options = options;
	options_table->optarg_handler = handler;
	options_table->optarg_check = check;

	fwts_list_append(&options_list, options_table);

	return FWTS_OK;
}

/*
 *  fwts_args_parse()
 *	parse options
 */
int fwts_args_parse(fwts_framework *fw, const int argc, char * const argv[])
{
	fwts_list_link *item;
	fwts_options_table *options_table;
	struct option *long_options;
	int n = 0;
	int i;
	int c;
	int option_index;
	int translated_long_option_index;
	int ret = FWTS_OK;
	char *short_options = NULL;
	size_t short_options_len = 0;

	long_options = calloc(1, (total_options + 1) * sizeof(struct option));
	if (long_options == NULL) {
		fwts_log_error(fw, "Out of memory allocating long options.");
		return FWTS_ERROR;
	}

	/*
	 *  Build a getopt_long options table from all the options tables
	 */
	fwts_list_foreach(item, &options_list) {
		options_table = fwts_list_data(fwts_options_table *, item);

		for (i = 0; i < options_table->num_options; i++, n++) {
			const char *short_name = options_table->options[i].short_name;
			size_t len;

			long_options[n].name    = options_table->options[i].long_name;
			long_options[n].has_arg = options_table->options[i].has_arg;
			long_options[n].flag    = 0;
			long_options[n].val     = 0;

			if (short_name && (len = strlen(short_name)) > 0) {
				if (short_options) {
					char *new_short_options;

					new_short_options = realloc(short_options,
						short_options_len + len + 1);
					if (new_short_options == NULL) {
						free(short_options);
						free(long_options);
						fwts_log_error(fw,
							"Out of memory "
							"allocating options.");
						return FWTS_ERROR;
					} else
						short_options = new_short_options;

					strcat(short_options, short_name);
					short_options_len += (len + 1);
				} else {
					short_options = calloc(1, len + 1);
					if (short_options == NULL) {
						free(short_options);
						free(long_options);
						fwts_log_error(fw,
							"Out of memory "
							"allocating options.");
						return FWTS_ERROR;
					}
					strcpy(short_options, short_name);
					short_options_len += (len + 1);
				}
			}
		}
	}

	/*
	 *  Although unlikely, short_options must not be null otherwise
	 *  it could potentially break getopt_long() so to be totally
	 *  safe, allocate a empty string.
	 *  Cleans up CoverityScan CID#1381432 ("Explicit Null dereference")
	 */
	if (!short_options) {
		short_options = strdup("");
		if (!short_options) {
			free(short_options);
			free(long_options);
			fwts_log_error(fw, "Out of memory allocating options.");
			return FWTS_ERROR;
		}
	}

	for (;;) {
		translated_long_option_index = 0;
		c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		fwts_list_foreach(item, &options_list) {
			options_table = fwts_list_data(fwts_options_table *, item);
			bool found = false;

			if (c != 0) {
				for (i = 0; i < options_table->num_options; i++) {
					const char *short_name = options_table->options[i].short_name;

					if (short_name && index(short_name, c) != NULL) {
						found = true;
						break;
					}
				}
			} else {  /* c is zero for long option cases but we need the right optarg_handler set */
				for (i = 0; i < options_table->num_options; i++) {
					const char *long_name = options_table->options[i].long_name;
					const char *name = long_options[option_index].name;

					if (long_name && name && !strcmp(long_name, name)) {
						translated_long_option_index = i;
						found = true;
						break;
					}
				}
			}

			/*  Found an option, then run the appropriate handler */
			if (found) {
				ret = options_table->optarg_handler(fw, argc, argv, c, translated_long_option_index);
				if (ret != FWTS_OK)
					goto exit;
				break;
			}

		}
	}

	/* We've collected all the args, now sanity check the values */

	fwts_list_foreach(item, &options_list) {
		options_table = fwts_list_data(fwts_options_table *, item);
		if (options_table->optarg_check != NULL) {
			ret = options_table->optarg_check(fw);
			if (ret != FWTS_OK)
				break;
		}
	}
exit:
	free(short_options);
	free(long_options);

	return ret;
}


/*
 *  fwts_args_compare_options()
 *	helper to enable sorting on long options name
 */
static int fwts_args_compare_options(void *data1, void *data2)
{
	fwts_option *opt1 = (fwts_option*)data1;
	fwts_option *opt2 = (fwts_option*)data2;

	return strcmp(opt1->long_name, opt2->long_name);
}

/*
 *  fwts_args_show_option()
 *	pretty print an option
 */
static void fwts_args_show_option(
	const int width,
	const char *option,
	const char *explanation)
{
	fwts_list *text;
	fwts_list_link *item;
	int lineno = 0;

	text = fwts_format_text(explanation,
		width < 0 ? (FWTS_MIN_TTY_WIDTH - FWTS_ARGS_WIDTH-1) : width);
	if (!text)
		return;

	fwts_list_foreach(item, text) {
		printf("%-*.*s %s\n",
			FWTS_ARGS_WIDTH, FWTS_ARGS_WIDTH,
			lineno++ == 0 ? option : "",
			fwts_list_data(char *, item));
	}
	fwts_list_free(text, free);
}

/*
 *  fwts_args_show_options()
 *	show all options and brief explanation
 */
void fwts_args_show_options(void)
{
	int i;
	int width;

	fwts_list_link *item;
	fwts_list sorted_options;

	fwts_list_init(&sorted_options);

	width = fwts_tty_width(fileno(stdin), FWTS_MIN_TTY_WIDTH);
	if ((width - (FWTS_ARGS_WIDTH + 1)) < 0)
		width = FWTS_MIN_TTY_WIDTH;

	width -= (FWTS_ARGS_WIDTH + 1);

	fwts_list_foreach(item, &options_list) {
		fwts_options_table *options_table;
		options_table = fwts_list_data(fwts_options_table *, item);

		for (i = 0; i < options_table->num_options; i++) {
			fwts_list_add_ordered(&sorted_options,
				&options_table->options[i], fwts_args_compare_options);
		}
	}

	fwts_list_foreach(item, &sorted_options) {
		char buffer[80];
		char *ptr = buffer;
		fwts_option *option = fwts_list_data(fwts_option *, item);
		size_t n = sizeof(buffer) - 1;

		/* Format up short name, skip over : fields */
		*ptr = '\0';
		if ((option->short_name != NULL) && *(option->short_name)) {
			char ch;
			for (i = 0; (ch = option->short_name[i]) != '\0'; i++) {
				if (ch != ':') {
					*ptr++ = '-';
					*ptr++ = ch;
					*ptr++ = ',';
					*ptr++ = ' ';
					*ptr = '\0';
					n -= 4;
				}
			}
		}
		*ptr++ = '-';
		*ptr++ = '-';
		*ptr = '\0';
		strncat(ptr, option->long_name, n - 2);

		fwts_args_show_option(width, buffer, option->explanation);
	}

	fwts_list_free_items(&sorted_options, NULL);
}

/*
 *  fwts_args_free()
 *	free option descriptor list
 */
int fwts_args_free(void)
{
	fwts_list_free_items(&options_list, free);

	return FWTS_OK;
}

/*
 *  fwts_args_comma_list
 *	given a comma separated list, return a string of space separated terms.
 *	returns NULL if failed
 */
char *fwts_args_comma_list(const char *arg)
{
        char *tmpstr;
        char *token;
        char *saveptr = NULL;
	char *retstr = NULL;
	char *tmparg;

	if ((tmparg = strdup(arg)) == NULL)
		return NULL;

        for (tmpstr = tmparg; (token = strtok_r(tmpstr, ",", &saveptr)) != NULL; tmpstr = NULL) {
		if (retstr)
			if ((retstr = fwts_realloc_strcat(retstr, " ")) == NULL) {
				free(tmparg);
				return NULL;
			}

		if ((retstr = fwts_realloc_strcat(retstr, token)) == NULL) {
			free(tmparg);
			return NULL;
		}
        }

	free(tmparg);

	/* Any empty list should return an empty string and not NULL */
	if (retstr == NULL)
		retstr = calloc(1, sizeof(char));
		/* Return NULL on calloc failure must be handled by caller */

	return retstr;
}
