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

#ifndef __FWTS_DUMP_DATA_H__
#define __FWTS_DUMP_DATA_H__

#include "fwts.h"

void fwts_dump_raw_data(char *buffer, const size_t len, const uint8_t *data, const int where, const size_t bytes);
void fwts_dump_raw_data_prefix(char *buffer, const size_t len, const uint8_t *data, const char *prefix, const size_t bytes);
void fwts_hexdump_data_prefix_all(fwts_framework *fw, const uint8_t *data, const char *prefix,	const size_t nbytes);
#endif
