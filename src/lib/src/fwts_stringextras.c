/*
 * Copyright (C) 2010-2026 Canonical
 *
 * The following functions are derivative work from systemd, and
 * are covered by Copyright 2010 Lennart Poettering:
 *     fwts_string_endswith()
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

#include <string.h>

#include "fwts.h"

/*
 *  fwts_chop_newline()
 *	strip off trailing \n and ' ' from end of a string
 */
void fwts_chop_newline(char *str)
{
	size_t len;

	if (!str)
		return;

	len = strlen(str);
	while (len > 0 && str[len-1] == '\n')
		str[--len] = '\0';

	while (len > 0 && str[len-1] == ' ')
		str[--len] = '\0';

	while (len > 0 && str[len-1] == '\r')
		str[--len] = '\0';

}

/*
 *  fwts_realloc_strcat()
 *	append newstr to orig (allocated on the heap)
 *	return NULL if fails, otherwise return expanded string
 */
char *fwts_realloc_strcat(char *orig, const char *newstr)
{
	size_t newlen = strlen(newstr);

	if (orig) {
		char *tmp;

		tmp = realloc(orig, strlen(orig) + newlen + 1);
		if (!tmp) {
			free(orig);
			return NULL;
		}
		orig = tmp;
		strcat(orig, newstr);
	} else {
		if ((orig = malloc(newlen + 1)) == NULL)
			return NULL;
		strcpy(orig, newstr);
	}
	return orig;
}

/*
 * fwts_string_endswith()
 * see if str ends with postfix
 * return NULL if fails, otherwise return the matched substring
 */
const char *fwts_string_endswith(const char *str, const char *postfix)
{
	size_t sl, pl;

	sl = strlen(str);
	pl = strlen(postfix);

	if (pl == 0)
		return str + sl;

	if (sl < pl)
		return NULL;

	if (memcmp(str + sl - pl, postfix, pl) != 0)
		return NULL;

	return str + sl - pl;
}
