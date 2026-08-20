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

#ifndef __FWTS_KEYMAP_H__
#define __FWTS_KEYMAP_H__

#define FWTS_KEYMAP_PATH	"/lib/udev/keymaps"

typedef struct {
	int	scancode;
	char	*keyname;
	char 	*keytext;
} fwts_keycode;

void fwts_keymap_free(fwts_list *keylist);
fwts_list *fwts_keymap_load(const char *machine);
fwts_keycode *fwts_keymap_find_scancode(fwts_list *keymap, const int scancode);

#endif
