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
#include <stdint.h>

#include "fwts.h"

/*
 *  fwts_guid_buf_to_str()
 *	format given GUID 'guid' into a string guid_str.
 *	guid_str needs to be at least 37 chars long
 */
void fwts_guid_buf_to_str(const uint8_t *guid, char *guid_str, const size_t guid_str_len)
{
	if (guid_str && guid_str_len > 36)
		snprintf(guid_str, guid_str_len,
			"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			guid[3], guid[2], guid[1], guid[0], guid[5], guid[4], guid[7], guid[6],
			guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
}

/*
 *  fwts_guid_str_to_buf()
 *	convert a GUID string back to a 16 byte GUID
 *	guid needs to be at least 16 chars long
 */
void fwts_guid_str_to_buf(const char *guid_str, uint8_t *guid, const size_t guid_len)
{
	if (guid && guid_len >= 16) {
		sscanf(guid_str,
			"%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
			&guid[3], &guid[2], &guid[1], &guid[0], &guid[5], &guid[4], &guid[7], &guid[6],
			&guid[8], &guid[9], &guid[10], &guid[11], &guid[12], &guid[13], &guid[14], &guid[15]);
	}
}
/*
 *  fwts_guid_match()
 *	compare whether two GUIDs are the same
 */
bool fwts_guid_match(const uint8_t *guid, const uint8_t *guid2, const size_t guid_size)
{
	size_t i;

	for (i = 0; i < guid_size; i++) {
		if (guid[i] != guid2[i])
			return false;
	}

	return true;
}
