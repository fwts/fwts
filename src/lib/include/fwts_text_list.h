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

#ifndef __FWTS_TEXT_LIST_H__
#define __FWTS_TEXT_LIST_H__

#include "fwts_list.h"

fwts_list         *fwts_text_list_new(void);
void 	           fwts_text_list_free(fwts_list *list);
char              *fwts_text_list_strstr(fwts_list *list, const char *needle);
fwts_list         *fwts_list_from_text(const char *list);
fwts_list_link    *fwts_text_list_append(fwts_list *list, const char *text);

static inline char *fwts_text_list_text(fwts_list_link *item)
{
	return fwts_list_data(char *, item);
}

#endif
