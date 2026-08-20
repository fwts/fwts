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
#include <string.h>

#include "fwts.h"

/*
 *  fwts_get()
 *	get a string from a file. used to gather text from /proc or /sys entries
 */
char *fwts_get(const char *file)
{
	FILE *fp;
	char buffer[4096];

	if ((fp = fopen(file, "r")) == NULL)
		return NULL;

	if (fgets(buffer, sizeof(buffer), fp) == NULL) {
		(void)fclose(fp);
		return NULL;
	}

	(void)fclose(fp);

	return strdup(buffer);
}

/*
 *  fwts_get_int()
 *	get an int from a file. used to gather int values from /proc or /sys entries
 */
int fwts_get_int(const char *file, int *value)
{
	char *data;

	*value = 0;

	if ((data = fwts_get(file)) == NULL) {
		*value = 0;
		return FWTS_ERROR;
	}

	*value = atoi(data);
	free(data);

	return FWTS_OK;
}
