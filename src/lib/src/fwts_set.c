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

#include "fwts.h"

/*
 *  fwts_set()
 *	write text to a given file, used to set
 *	values in /sys or /proc
 */
int fwts_set(const char *file, const char *text)
{
	FILE *fp;
	int ret;

	if ((fp = fopen(file, "w")) == NULL)
		return FWTS_ERROR;

	ret = fprintf(fp, "%s\n", text);
	(void)fclose(fp);

	return (ret < 0) ? FWTS_ERROR : FWTS_OK;
}

/*
 *  fwts_set_int()
 *	write an int to a given file, used to set
 *	values in /sys or /proc
 */
int fwts_set_int(const char *file, const int value)
{
	FILE *fp;
	int ret;

	if ((fp = fopen(file, "w")) == NULL)
		return FWTS_ERROR;

	ret = fprintf(fp, "%d\n", value);
	(void)fclose(fp);

	return (ret < 0) ? FWTS_ERROR : FWTS_OK;
}
