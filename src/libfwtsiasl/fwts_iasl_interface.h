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

#ifndef __FWTS_IASL_INTERFACE__
#define __FWTS_IASL_INTERFACE__

#include <stdint.h>

int fwts_iasl_disassemble_aml(
	char *tables[], char *names[], const int table_entries,
	const int which, const bool use_externals,
	const char *outputfile);
int fwts_iasl_assemble_aml(
	const char *source, char **stdout_output,
	char **stderr_output);
const char *fwts_iasl_exception_level__(uint8_t level);

#endif
