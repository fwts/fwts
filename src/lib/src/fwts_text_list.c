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

#include "fwts.h"

/*
 *  fwts_text_list_new()
 *	initialise a text list
 */
fwts_list *fwts_text_list_new(void)
{
	return fwts_list_new();
}

/*
 *  fwts_text_list_free()
 *	free a text list
 */
void fwts_text_list_free(fwts_list *list)
{
	fwts_list_free(list, free);
}

/*
 *  fwts_text_list_append()
 *	append a string to a text list
 */
fwts_list_link *fwts_text_list_append(fwts_list *list, const char *text)
{
	char *str;

	if ((str = strdup(text)) == NULL)
		return NULL;
	else
		return fwts_list_append(list, str);
}

/*
 *  fwts_text_list_strstr()
 *	check if string needle exists in the text list, returns
 *	the position in the string if found, NULL if not found
 */
char *fwts_text_list_strstr(fwts_list *list, const char *needle)
{
	fwts_list_link *item;
	char *match;

	if (list == NULL)
		return NULL;

	fwts_list_foreach(item, list)
		if ((match = strstr(fwts_list_data(char *, item), needle)) != NULL)
			return match;

	return NULL;
}

/*
 *  fwts_list_from_text()
 *	turn a string containing lines of text into
 *	a text list
 */
fwts_list *fwts_list_from_text(const char *text)
{
	const char *ptr;
	fwts_list *list;

	if (text == NULL)
		return NULL;

	if ((list = fwts_list_new()) == NULL)
		return NULL;

	ptr = text;

	while (*ptr) {
		const char *start = ptr;
		char *str;
		int len;

		while (*ptr && *ptr != '\n')
			ptr++;

		len = ptr - start;

		if (*ptr == '\n')
			ptr++;

		if ((str = calloc(1, len + 1)) == NULL) {
			fwts_text_list_free(list);
			return NULL;
		}
		strncpy(str, start, len);
		str[len] = '\0';

		fwts_list_append(list, str);
	}

	return list;
}
