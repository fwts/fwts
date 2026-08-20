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
 *  fwts_scan_efi_systab()
 *	scan EFI systab for a value
 */
void *fwts_scan_efi_systab(const char *name)
{
	fwts_list *systab;
	fwts_list_link *item;
	void *addr = 0;

	if (((systab = fwts_file_open_and_read("/sys/firmware/efi/systab")) == NULL) &&
	    ((systab = fwts_file_open_and_read("/proc/efi/systab")) == NULL)) {
		return NULL;
	}

	fwts_list_foreach(item, systab) {
		char *str = fwts_list_data(char *, item);
		char *s_ptr = strstr(str, name);

		if (s_ptr) {
			char *ptr = strstr(str, "=");
			if (ptr) {
				if ((size_t)(ptr - s_ptr) != strlen(name))
					continue;
				addr = (void*)strtoul(ptr+1, NULL, 0);
				break;
			}
		}
	}
	fwts_list_free(systab, free);

	return addr;
}
