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

#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#include "fwts.h"

#define SYSTEM_BASE_END	(0x000a0000)		/* Assume > 512K, end is at 640K */

#define BIOS_START      (0x000e0000)            /* Start of BIOS memory */
#define BIOS_END        (0x000fffff)            /* End of BIOS memory */

#define MAX_SIZE(x)	(1UL << (sizeof(x) * 8))

/*
 *  fwts_mp_get_address()
 *	scan for _MP_ floating pointer, set phys_addr if found.
 */
static int fwts_mp_get_address(uint32_t *phys_addr)
{
	off_t	ebda;
	int	i;

	typedef struct {
		off_t	start;
		off_t	end;
	} fwts_scan_region;

	fwts_scan_region regions[] = {
		{ SYSTEM_BASE_END - 1024, 	SYSTEM_BASE_END },	/* Assume 640K or more */
		{ BIOS_START,			BIOS_END },
		{ 0,				0x400 },
		{ 0,				0 },
	};

	/* If we have an EBDA region defined, scan this rather than default
	   end of 640K region */
	if ((ebda = fwts_ebda_get()) != FWTS_NO_EBDA) {
		if (ebda != 0) {
			regions[0].start = ebda;
			regions[0].end   = ebda + 1024;
		}
	}

	for (i = 0; regions[i].end; i++) {
		void *mem;
		uint8_t *ptr;
		off_t  start = regions[i].start;
		size_t size = regions[i].end - start;

		if ((mem = fwts_mmap(start, size)) == FWTS_MAP_FAILED)
			continue;

		for (ptr = mem; ptr < (uint8_t*)mem + size; ptr += 16) {
			if ((*ptr == '_') &&
			    (*(ptr + 1) == 'M') &&
			    (*(ptr + 2) == 'P') &&
			    (*(ptr + 3) == '_')) {
				fwts_mp_floating_header *hdr = (fwts_mp_floating_header*)ptr;
				if (fwts_checksum((uint8_t *)hdr, sizeof(fwts_mp_floating_header)) == 0) {
					/* Looks valid, so return addr */
					*phys_addr = hdr->phys_address;
					(void)fwts_munmap(mem, size);
					return FWTS_OK;
				}
			}
		}

		(void)fwts_munmap(mem, size);
	}
	*phys_addr = 0;

	return FWTS_ERROR;
}

/*
 *  fwts_mp_data_get()
 *	find _MP_ data, map in header and table,
 *	collect up entries into a list
 */
int fwts_mp_data_get(fwts_mp_data *data)
{
	uint32_t phys_addr;
	void *mem;
	uint8_t *tmp;
	fwts_mp_config_table_header *header;
	int max_data_size;

	if (data == NULL)
		return FWTS_ERROR;

	if (fwts_mp_get_address(&phys_addr) != FWTS_OK)
		return FWTS_ERROR;

	data->phys_addr = phys_addr;

	/* Get header and find out full size of header and table */
	mem = fwts_mmap((off_t)phys_addr, sizeof(fwts_mp_config_table_header));
	if (mem == FWTS_MAP_FAILED)
		return FWTS_ERROR;

	header = (fwts_mp_config_table_header *)mem;
	fwts_list_init(&data->entries);

	data->size = header->base_table_length +
		((header->spec_rev == 1) ? 0 : header->extended_table_length);
	max_data_size = MAX_SIZE(header->base_table_length) +
			MAX_SIZE(header->extended_table_length);
	if (data->size < 0 || data->size > max_data_size)
		return FWTS_ERROR;

	/* Remap with full header and table now we know how big it is */
	(void)fwts_munmap(mem, sizeof(fwts_mp_config_table_header));
	mem = fwts_mmap((off_t)phys_addr, data->size);
	if (mem == FWTS_MAP_FAILED)
		return FWTS_ERROR;

	data->header = (fwts_mp_config_table_header*)mem;
	tmp = (uint8_t*)mem + sizeof(fwts_mp_config_table_header);

	/* Build a list containing the addresses of where entries start */
	while (tmp < (uint8_t*)mem + data->size) {
		switch (*tmp) {
		case FWTS_MP_CPU_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_processor_entry);
			break;
		case FWTS_MP_BUS_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_bus_entry);
			break;
		case FWTS_MP_IO_APIC_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_io_apic_entry);
			break;
		case FWTS_MP_IO_INTERRUPT_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_io_interrupt_entry);
			break;
		case FWTS_MP_LOCAL_INTERRUPT_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_local_interrupt_entry);
			break;
		case FWTS_MP_SYS_ADDR_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_system_address_space_entry);
			break;
		case FWTS_MP_BUS_HIERARCHY_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_bus_hierarchy_entry);
			break;
		case FWTS_MP_COMPAT_BUS_ADDRESS_SPACE_ENTRY:
			fwts_list_append(&data->entries, tmp);
			tmp += sizeof(fwts_mp_compat_bus_address_space_entry);
			break;
		default:
			tmp = (uint8_t *)mem + data->size; /* Force abort */
			break;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_mp_data_free()
 *	free entries and unmap
 */
int fwts_mp_data_free(fwts_mp_data *data)
{
	if (data == NULL)
		return FWTS_ERROR;

	/* Discard list */
	fwts_list_free_items(&data->entries, NULL);

	/* Free mapped data */
	if (data->header)
		(void)fwts_munmap(data->header, data->size);

	return FWTS_OK;
}
