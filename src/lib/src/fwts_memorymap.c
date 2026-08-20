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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#include "fwts.h"

/*
 *  fwts_fwts_memory_map_entry_compare()
 *	callback used to sort memory_map entries on start address
 */
static int fwts_fwts_memory_map_entry_compare(void *data1, void *data2)
{
        fwts_memory_map_entry *entry1 = (fwts_memory_map_entry *)data1;
        fwts_memory_map_entry *entry2 = (fwts_memory_map_entry *)data2;

	if (entry1->start_address < entry2->start_address)
		return -1;
	else if (entry1->start_address > entry2->start_address)
		return 1;
	else
		return 0;
}

/*
 *  fwts_memory_map_str_to_type()
 *	convert memory strings into type values
 */
static int fwts_memory_map_str_to_type(const char *str)
{
	/* Strings from /sys/firmware/memmap/x/type */

	if (strstr(str, "System RAM"))
		return FWTS_MEMORY_MAP_USABLE;
	if (strstr(str, "reserved") || strstr(str, "Reserved"))
		return FWTS_MEMORY_MAP_RESERVED;
	if (strstr(str, "ACPI Non-volatile Storage"))
		return FWTS_MEMORY_MAP_ACPI;

	/* Strings from kernel log */

	if (strstr(str, "usable"))
		return FWTS_MEMORY_MAP_USABLE;
	if (strstr(str, "reserved"))
		return FWTS_MEMORY_MAP_RESERVED;
	if (strstr(str, "ACPI"))
		return FWTS_MEMORY_MAP_ACPI;

	return FWTS_MEMORY_MAP_UNKNOWN;
}

/*
 *  fwts_memory_map_type_to_str()
 *	convert E280 type values to strings
 */
static char *fwts_memory_map_type_to_str(const int type)
{
	switch (type) {
	case FWTS_MEMORY_MAP_RESERVED:
		return "(Reserved)";
	case FWTS_MEMORY_MAP_ACPI:
		return "(ACPI Non-volatile Storage)";
	case FWTS_MEMORY_MAP_USABLE:
		return "(System RAM)";
	default:
		return "(UNKNOWN)";
	}
}

/*
 *  fwts_register_memory_map_line()
 *	add memory_map line entry into a list ordered on start address
 */
static int fwts_register_memory_map_line(fwts_list *memory_map_list, const uint64_t start, const uint64_t end, const int type)
{
	fwts_memory_map_entry *entry;

	if ((entry = calloc(1, sizeof(fwts_memory_map_entry))) == NULL)
		return FWTS_ERROR;

	entry->start_address = start;
	entry->end_address   = end;
	entry->type          = type;

	if (fwts_list_add_ordered(memory_map_list, entry, fwts_fwts_memory_map_entry_compare) == NULL)
		return FWTS_ERROR;

	return FWTS_OK;
}

/*
 *  fwts_memory_map_type()
 *	figure out memory region type on a given memory address
 */
int fwts_memory_map_type(fwts_list *memory_map_list, const uint64_t memory)
{
	fwts_list_link *item;

	fwts_list_foreach(item, memory_map_list) {
		fwts_memory_map_entry *entry = fwts_list_data(fwts_memory_map_entry*, item);
		if (entry->start_address <= memory && entry->end_address > memory)
			return entry->type;
	}

	return FWTS_MEMORY_MAP_UNKNOWN;
}

fwts_memory_map_entry *fwts_memory_map_info(fwts_list *memory_map_list, const uint64_t memory)
{
	fwts_list_link *item;

	fwts_list_foreach(item, memory_map_list) {
		fwts_memory_map_entry *entry = fwts_list_data(fwts_memory_map_entry*, item);
		if (entry->start_address <= memory && entry->end_address > memory)
			return entry;
	}
	return NULL;
}

/*
 *  fwts_memory_map_is_reserved()
 *	determine if a memory region is marked as reserved or not.
 */
fwts_bool fwts_memory_map_is_reserved(fwts_list *memory_map_list, const uint64_t memory)
{
	int result = FWTS_MEMORY_MAP_UNKNOWN;

	/* when we don't have FWTS_MEMORY_MAP info, assume all is fair */
	if (memory_map_list == NULL)
		return FWTS_TRUE;

	/* when we have FWTS_MEMORY_MAP info list empty, then assume all is fair */
	if (memory_map_list->len == 0)
		return FWTS_TRUE;

	/* bios data area is always reserved */
	if ((memory >= 640 * 1024) && (memory <= 1024*1024))
		return FWTS_TRUE;

	result = fwts_memory_map_type(memory_map_list, memory);

	if (result == FWTS_MEMORY_MAP_RESERVED)
		return FWTS_TRUE;
	if (result == FWTS_MEMORY_MAP_ACPI)
		return FWTS_TRUE;

	return FWTS_FALSE;
}

/*
 *  fwts_memory_map_dmesg_info()
 *	callback to check dmesg for memory_map info
 */
static void fwts_memory_map_dmesg_info(void *data, void *private)
{
	char *str;
	char *line = (char *)data;
	fwts_list *memory_map_list = (fwts_list *)private;

	if ((str = strstr(line,"BIOS-memory_map:")) != NULL) {
		uint64_t start;

		errno = 0;
		start = strtoull(str+10, NULL, 16);
		if (errno != 0)
			return;
		str = strstr(line," - ");
		if (str) {
			uint64_t end;
			errno = 0;
			str += 3;
			end = strtoull(str, NULL, 16);
			if (errno != 0 || end == 0)
				return;
			fwts_register_memory_map_line(memory_map_list, start, end - 1, fwts_memory_map_str_to_type(line));
		}
	} else if ((str = strstr(line,"BIOS-e820:")) != NULL) {
		uint64_t start;

		errno = 0;
		start = strtoull(str + sizeof("BIOS-e820: [mem 0x"), NULL, 16);
		if (errno != 0)
			return;
		str = strstr(line,"-0x");
		if (str) {
			uint64_t end;
			errno = 0;
			end = strtoull(str + sizeof("-0x"), NULL, 16);
			if (errno != 0 || end == 0)
				return;
			fwts_register_memory_map_line(memory_map_list, start, end - 1, fwts_memory_map_str_to_type(line));
		}
	}
}

/*
 *  fwts_memory_map_dump_info()
 *	callback to dump FWTS_MEMORY_MAP region
 */
static void fwts_memory_map_dump_info(void *data, void *private)
{
	fwts_memory_map_entry *entry = (fwts_memory_map_entry *)data;
	fwts_framework *fw = (fwts_framework *)private;

	fwts_log_info_verbatim(fw, "0x%16.16" PRIx64 " - 0x%16.16" PRIx64 " %s",
			entry->start_address, entry->end_address,
			fwts_memory_map_type_to_str(entry->type));
}

/*
 *  fwts_memory_map_table_dump()
 *	dump FWTS_MEMORY_MAP region
 */
void fwts_memory_map_table_dump(fwts_framework *fw, fwts_list *memory_map_list)
{
	fwts_log_info_verbatim(fw, "Memory Map Layout");
	fwts_log_info_verbatim(fw, "-----------------");

	fwts_list_iterate(memory_map_list, fwts_memory_map_dump_info, fw);
}

/*
 *  fwts_memory_map_table_load_from_klog()
 *	load memory_map data from the kernel log
 */
fwts_list *fwts_memory_map_table_load_from_klog(fwts_framework *fw)
{
	fwts_list *klog;
	fwts_list *memory_map_list;

	FWTS_UNUSED(fw);

	if ((klog = fwts_klog_read()) == NULL)
		return NULL;

	if ((memory_map_list = fwts_list_new()) == NULL) {
		fwts_klog_free(klog);
		return NULL;
	}

	fwts_list_iterate(klog, fwts_memory_map_dmesg_info, memory_map_list);
	fwts_klog_free(klog);

	return memory_map_list;
}

/*
 *  fwts_memory_map_table_read_entry()
 *	load individual memory_map entry from /sys/firmware/memmap/
 */
static fwts_memory_map_entry *fwts_memory_map_table_read_entry(const char *which)
{
	char path[PATH_MAX];
	char *data;
	fwts_memory_map_entry *entry;

	if ((entry = calloc(1, sizeof(fwts_memory_map_entry))) == NULL)
		return NULL;

	snprintf(path, sizeof(path), "/sys/firmware/memmap/%s/start", which);
	if ((data = fwts_get(path)) == NULL) {
		free(entry);
		return NULL;
	}
	sscanf(data, "0x%" SCNx64, &entry->start_address);
	free(data);

	snprintf(path, sizeof(path), "/sys/firmware/memmap/%s/end", which);
	if ((data = fwts_get(path)) == NULL) {
		free(entry);
		return NULL;
	}
	sscanf(data, "0x%" SCNx64, &entry->end_address);
	free(data);

	snprintf(path, sizeof(path), "/sys/firmware/memmap/%s/type", which);
	if ((data = fwts_get(path)) == NULL) {
		free(entry);
		return NULL;
	}
	entry->type = fwts_memory_map_str_to_type(data);
	free(data);

	return entry;
}

/*
 *  fwts_memory_map_table_load()
 *	load memory_map table from /sys/firmware/memmap/
 */
fwts_list *fwts_memory_map_table_load(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *directory;
	fwts_list *memory_map_list;

	/* Try to load from /sys/firmware/memmap, but if we fail, try
	   scanning the kernel log as a fallback */
	if ((dir = opendir("/sys/firmware/memmap/")) == NULL)
		return fwts_memory_map_table_load_from_klog(fw);

	if ((memory_map_list = fwts_list_new()) == NULL) {
		(void)closedir(dir);
		return NULL;
	}

	while ((directory = readdir(dir)) != NULL) {
		if (strncmp(directory->d_name, ".", 1)) {
			fwts_memory_map_entry *entry;

			entry = fwts_memory_map_table_read_entry(directory->d_name);
			if (!entry) {
				fwts_memory_map_table_free(memory_map_list);
				(void)closedir(dir);
				return NULL;
			}
			fwts_list_add_ordered(memory_map_list, entry, fwts_fwts_memory_map_entry_compare);
		}
	}
	(void)closedir(dir);

	return memory_map_list;
}

/*
 *  fwts_memory_map_table_free()
 *	free memory_map list
 */
void fwts_memory_map_table_free(fwts_list *memory_map_list)
{
	fwts_list_free(memory_map_list, free);
}

const char *fwts_memory_map_name(const int type)
{
	switch (type) {
	case FWTS_FIRMWARE_BIOS:
		return "Int 15 AX=E820 BIOS memory map";
	case FWTS_FIRMWARE_UEFI:
		return "UEFI run-time service memory map";
	default:
		return "Unknown memory map";
	}
}
