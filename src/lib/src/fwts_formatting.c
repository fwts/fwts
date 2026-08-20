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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fwts.h"

/*
 *  dup_line()
 *	duplicate a portion of a line of text, from start to end up to
 *	a maximum of width characters
 */
static char *dup_line(const char *start, const char *end, const size_t width)
{
	size_t maxlen;
	char *buffer;
	char *bufptr;

	maxlen = end - start;
	if (maxlen < width)
		maxlen = width;

	buffer = calloc(1, maxlen + 1);
	if (!buffer)
		return NULL;

	bufptr = buffer;
	while (*start && start < end)
		*bufptr++ = *start++;

	*bufptr = '\0';

	return buffer;
}

/*
 *  format_remove_multiple_whitespaces()
 *	Clone text string but remove whitespaces. Returns
 *	NULL if failed to clone.
 */
static char *format_remove_multiple_whitespaces(const char *text)
{
	char *buffer;
	char *bufptr1, *bufptr2;

	if ((bufptr1 = bufptr2 = buffer = strdup(text)) == NULL)
		return NULL;

	while (*bufptr1) {
		if (isspace(*bufptr1)) {
			while (*bufptr1 && isspace(*bufptr1))
				bufptr1++;
			*bufptr2++ = ' ';
		}
		else
			*bufptr2++ = *bufptr1++;
	}
	*bufptr2 = '\0';

	return buffer;
}

/*
 *  fwts_format_text()
 * 	given a text string, format it into a list of lines of
 *	text to a given width.
 */
fwts_list *fwts_format_text(const char *text, const size_t width)
{
	size_t linelen = 0;
	char *lastspace = NULL;
	char *tidied_text;
	char *linestart;
	char *textptr;
	char *tmp;

	fwts_list *list;

	if ((list = fwts_text_list_new()) == NULL)
		return NULL;

	if ((textptr = tidied_text = format_remove_multiple_whitespaces(text)) == NULL) {
		fwts_list_free(list, free);
		return NULL;
	}

	linestart = tidied_text;

	while (*textptr) {
		/* find line break points */
		if (isspace(*textptr) ||
		    ((lastspace != NULL) && (*(textptr-1) != '/') && (*textptr == '/')) ||
		    (*textptr == ':') ||
		    (*textptr == ';') ||
		    (*textptr == ','))
			lastspace = textptr;

		if (linelen >= width) {
			if (lastspace != NULL) {
				if ((tmp = dup_line(linestart, lastspace, width)) == NULL) {
					fwts_text_list_free(list);
					free(tidied_text);
					return NULL;
				}
				fwts_text_list_append(list, tmp);
				free(tmp);

				linestart = lastspace + ((isspace(*lastspace)) ? 1 : 0);
				linelen = textptr - linestart;
				lastspace = NULL;
			}
		}
		textptr++;
		linelen++;
	}
	if ((tmp = dup_line(linestart, textptr, width)) == NULL) {
		fwts_text_list_free(list);
		free(tidied_text);
		return NULL;
	}
	fwts_text_list_append(list, tmp);
	free(tmp);
	free(tidied_text);

	return list;
}
