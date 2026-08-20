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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "fwts.h"

/*
 *  fwts_file_read()
 *	read given file and return contents as a list of lines
 */
fwts_list *fwts_file_read(FILE *fp)
{
	fwts_list *list;
	char buffer[8192];

	if ((list = fwts_list_new()) == NULL)
		return NULL;

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		buffer[strlen(buffer) - 1] = '\0';	/* Chop off "\n" */
		fwts_text_list_append(list, buffer);
	}

	return list;
}

/*
 *  fwts_file_open_and_read()
 *	open and read file and return contents as a list of lines
 */
fwts_list* fwts_file_open_and_read(const char *file)
{
	FILE *fp;
	fwts_list *list;

	if ((fp = fopen(file, "r")) == NULL)
		return NULL;

	list = fwts_file_read(fp);
	(void)fclose(fp);

	return list;
}

/*
 *  fwts_gzfile_read()
 *	read given gz file and return contents as a list of lines
 */
fwts_list *fwts_gzfile_read(gzFile *fp)
{
	fwts_list *list;
	char buffer[8192];

	if ((list = fwts_list_new()) == NULL)
		return NULL;

	while (gzgets(*fp, buffer, sizeof(buffer)) != NULL) {
		buffer[strlen(buffer) - 1] = '\0';	/* Chop off "\n" */
		fwts_text_list_append(list, buffer);
	}

	return list;
}

/*
 *  fwts_gzfile_open_and_read()
 *	open and read gz file and return contents as a list of lines
 */
fwts_list* fwts_gzfile_open_and_read(const char *file)
{
	gzFile fp;
	fwts_list *list;

	if ((fp = gzopen(file, "r")) == Z_NULL)
		return NULL;

	list = fwts_gzfile_read(&fp);
	(void)gzclose(fp);

	return list;
}
