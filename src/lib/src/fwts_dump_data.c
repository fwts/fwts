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
#include <stdlib.h>

#include "fwts.h"

/*
 *  fwts_dump_raw_data()
 *	print raw uint8 data of length `nbytes` into a buffer (length len)
 *      as a hex dump. nbytes must be no more than 16. The address/offset
 *      of the buffer in memory is annotated by addr.
 */
void fwts_dump_raw_data(
	char *buffer,		/* buffer to contained formatted dump */
	const size_t len,	/* Length of buffer */
	const uint8_t *data,	/* Octets to dump */
	const int addr,		/* Original buffer data address */
	const size_t nbytes)	/* Number of bytes to dump, max 16 */
{
        int i;
	int n = 0;
	int nbytes_max = nbytes > 16 ? 16 : nbytes;

	if (addr >= 0x100000)
		n = snprintf(buffer, len, "%6.6X: ", addr);
	else if (addr >= 0x10000)
		n = snprintf(buffer, len, " %5.5X: ", addr);
	else
		n = snprintf(buffer, len, "  %4.4X: ", addr);

	/* Hex dump */
        for (i = 0; i < nbytes_max; i++)
                n += snprintf(buffer + n, len - n, "%2.2X ", data[i]);

	/* Padding */
        for (; i < 16; i++)
                n += snprintf(buffer + n, len - n, "   ");

        n += snprintf(buffer + n, len - n, " ");

	/* printable ASCII dump */
        for (i = 0; i < nbytes_max; i++)
		buffer[n++] = (data[i] < 32 || data[i] > 126) ? '.' : data[i];
	buffer[n] = '\0';
}

/*
 *  fwts_dump_raw_data_prefix()
 *	simply print raw uint8 data of length `nbytes` into a buffer (length len)
 *      as a hex dump. nbytes must be no more than 16 with prefix. The prefix could
 *	be used as alighment.
 */
void fwts_dump_raw_data_prefix(
	char *buffer,		/* buffer to contained formatted dump */
	const size_t len,	/* Length of buffer */
	const uint8_t *data,	/* Octets to dump */
	const char *prefix,	/* Prefix string or for alignment */
	const size_t nbytes)	/* Number of bytes to dump, max 16 */
{
        int i;
	int n = 0;
	int nbytes_max = nbytes > 16 ? 16 : nbytes;

	n = snprintf(buffer, len, "%s", prefix);

	/* Hex dump */
        for (i = 0; i < nbytes_max; i++)
                n += snprintf(buffer + n, len - n, "%2.2X ", data[i]);

	buffer[n] = '\0';
}

/*
 *  fwts_hexdump_data_prefix_all()
 *	print all raw uint8 data of length `nbytes` as a hex dump with prefix.
 *	The prefix could be used as alighment.
 */
void fwts_hexdump_data_prefix_all(
	fwts_framework *fw,
	const uint8_t *data,	/* Octets to dump */
	const char *prefix,	/* Prefix string or for alignment for each line */
	const size_t nbytes)	/* Number of bytes to dump for all data */
{

	size_t i;

	for (i = 0; i < nbytes; i += 16) {
		char buffer[128];
		const size_t left = nbytes - i;

		fwts_dump_raw_data_prefix(buffer, sizeof(buffer), data + i, prefix, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw, "%s", buffer);
	}
}
