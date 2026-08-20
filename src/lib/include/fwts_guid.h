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

#ifndef __FWTS_GUID_H__
#define __FWTS_GUID_H__

#include <stdlib.h>
#include <stdint.h>

void fwts_guid_buf_to_str(const uint8_t *guid, char *guid_str, const size_t guid_str_len);
void fwts_guid_str_to_buf(const char *guid_str, uint8_t *guid, const size_t guid_len);
bool fwts_guid_match(const uint8_t *guid, const uint8_t *guid2, const size_t guid_size);

#endif
