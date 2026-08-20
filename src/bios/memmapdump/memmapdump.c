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

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

static int memory_mapdump_util(fwts_framework *fw)
{
	fwts_list *memory_mapdump_memory_map_info;

	if ((memory_mapdump_memory_map_info =
		fwts_memory_map_table_load(fw)) == NULL) {
		fwts_log_warning(fw,
			"Cannot memory map table from /sys/firmware/memmap "
			"or kernel log.");
		return FWTS_ERROR;
	}

	fwts_memory_map_table_dump(fw, memory_mapdump_memory_map_info);
	fwts_memory_map_table_free(memory_mapdump_memory_map_info);

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test memory_mapdump_utils[] = {
	{ memory_mapdump_util, "Dump system memory map." },
	{ NULL, NULL }
};

static fwts_framework_ops memory_mapdump_ops = {
	.description = "Dump system memory map.",
	.minor_tests = memory_mapdump_utils
};

FWTS_REGISTER("memmapdump", &memory_mapdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)

#endif
