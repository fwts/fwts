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

#include <unistd.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL
#include <sys/io.h>

/*
 *  fwts_cmos_read()
 *	read a byte from cmos memory at a given offset
 */
int fwts_cmos_read(const uint8_t offset, uint8_t *value)
{
	int ret = FWTS_OK;

	*value = ~0;	/* Default in case of error */

	if (ioperm(0x70, 2, 1) < 0)
		return FWTS_ERROR;

	if (ioperm(0x80, 1, 1) < 0) {
		ret = FWTS_ERROR;
		goto tidy0x70;
	}
	/* Want to disable interrupts */
	if (iopl(3) < 0) {
		ret = FWTS_ERROR;
		goto tidy0x80;
	}

	/* specify offset to read */
	if (fwts_outb(offset, 0x70) != FWTS_OK) {
		ret = FWTS_ERROR;
		goto tidy;
	}

	/* Small Delay */
	if (fwts_outb(0, 0x80) != FWTS_OK) {
		ret = FWTS_ERROR;
		goto tidy;
	}

	/* get the CMOS value */
	if (fwts_inb(0x71, value) != FWTS_OK)
		ret = FWTS_ERROR;
tidy:
	(void)iopl(0);
tidy0x80:
	(void)ioperm(0x80, 1, 0);
tidy0x70:
	(void)ioperm(0x70, 2, 0);

	return ret;
}
#else
int fwts_cmos_read(const uint8_t offset, uint8_t *value)
{
	FWTS_UNUSED(offset);

	*value = ~0;	/* Fake a failed read */

	return FWTS_ERROR;
}
#endif
