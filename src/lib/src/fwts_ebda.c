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

#include "fwts.h"

#include <sys/types.h>

#ifdef FWTS_ARCH_INTEL

#define EBDA_OFFSET		0x40e

/*
 *  ftws_ebda_get()
 *	get EBDA offset so EBDA can be mmap'd
 */
off_t fwts_ebda_get(void)
{
	uint16_t *ebda;
	off_t    ebda_addr;

	if ((ebda = fwts_mmap((off_t)EBDA_OFFSET, sizeof(uint16_t))) == FWTS_MAP_FAILED) {
		return FWTS_NO_EBDA;
	}
	ebda_addr = ((off_t)*ebda) << 4;
	(void)fwts_munmap(ebda, sizeof(uint16_t));

	return ebda_addr;
}

#else
off_t fwts_ebda_get(void)
{
	return FWTS_NO_EBDA;
}
#endif
