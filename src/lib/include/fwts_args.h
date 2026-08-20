/*
 * Copyright (C) 2011-2026 Canonical
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

#ifndef __FWTS_ARGS_H__
#define __FWTS_ARGS_H__

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

typedef struct {
	const char *long_name;		/* e.g. "quiet", long name */
	const char *short_name;		/* e.g. "q", short help info */
	const int  has_arg;
	const char *explanation;	/* explanation: "Enabled quiet mode..." */
} fwts_option;

#include "fwts.h"

typedef int (*fwts_args_optarg_handler)(fwts_framework *framework,
	const int argc, char * const argv[], const int option_char,
	const int long_index);
typedef int (*fwts_args_optarg_check)(fwts_framework *framework);

int fwts_args_init(void);
int fwts_args_add_options(fwts_option *options,
	const fwts_args_optarg_handler handler,
	const fwts_args_optarg_check check);
int fwts_args_parse(fwts_framework *fw, const int argc, char * const argv[]);
void fwts_args_show_options(void);
int fwts_args_free(void);
char *fwts_args_comma_list(const char *arg);

#endif
