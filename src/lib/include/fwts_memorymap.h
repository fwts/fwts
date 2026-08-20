/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This file was originally part of the Linux-ready Firmware Developer Kit
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
 */

#ifndef __FWTS_FWTS_MEMORY_MAP_H__
#define __FWTS_FWTS_MEMORY_MAP_H__

#include "fwts_types.h"
#include "fwts_list.h"
#include "fwts_framework.h"

#define FWTS_MEMORY_MAP_UNKNOWN    0
#define FWTS_MEMORY_MAP_RESERVED   1
#define FWTS_MEMORY_MAP_ACPI       2
#define FWTS_MEMORY_MAP_USABLE     16

typedef struct {
	uint64_t        start_address;
	uint64_t        end_address;
	int             type;
} fwts_memory_map_entry;

int        fwts_memory_map_type(fwts_list *memory_map_list, const uint64_t memory);
fwts_bool  fwts_memory_map_is_reserved(fwts_list *memory_map_list, const uint64_t memory);
fwts_list *fwts_memory_map_table_load(fwts_framework *fw);
void       fwts_memory_map_table_free(fwts_list *memory_map_list);
void       fwts_memory_map_table_dump(fwts_framework *fw, fwts_list *memory_map_list);
const char *fwts_memory_map_name(const int type);
fwts_memory_map_entry *fwts_memory_map_info(fwts_list *memory_map_list, const uint64_t memory);

#endif
