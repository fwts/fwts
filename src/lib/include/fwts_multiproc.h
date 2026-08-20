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

#ifndef __FWTS_MULTIPROC_H__
#define __FWTS_MULTIPROC_H__

#define FWTS_MP_HEADER_SIGNATURE	"PCMP"

/* 
 * Structures as described in version 1.4 of the
 * Intel MultiProcessor Specification, May 1997
 */

typedef enum {
	FWTS_MP_CPU_ENTRY = 0,
	FWTS_MP_BUS_ENTRY = 1,
	FWTS_MP_IO_APIC_ENTRY = 2,
	FWTS_MP_IO_INTERRUPT_ENTRY = 3,
	FWTS_MP_LOCAL_INTERRUPT_ENTRY = 4,
	FWTS_MP_SYS_ADDR_ENTRY = 128,
	FWTS_MP_BUS_HIERARCHY_ENTRY = 129,
	FWTS_MP_COMPAT_BUS_ADDRESS_SPACE_ENTRY = 130
} fwts_mp_types;

typedef struct {
	uint8_t		signature[4];
	uint32_t	phys_address;
	uint8_t		length;
	uint8_t		spec_rev;
	uint8_t		checksum;
	uint8_t		mp_feature_bytes[5];
} __attribute__ ((packed)) fwts_mp_floating_header;

typedef struct {
	uint8_t		signature[4];
	uint16_t	base_table_length;
	uint8_t		spec_rev;
	uint8_t		checksum;
	uint8_t		oem_id[8];
	uint8_t		product_id[12];
	uint32_t	oem_table_ptr;
	uint16_t	oem_table_size;
	uint16_t	entry_count;
	uint32_t	lapic_address;
	uint16_t	extended_table_length;
	uint8_t		extended_table_checksum;
	uint8_t		reserved;
} __attribute__ ((packed)) fwts_mp_config_table_header;

typedef struct {
	uint8_t		entry_type;
	uint8_t		local_apic_id;
	uint8_t		local_apic_version;
	uint8_t		cpu_flags;
	uint32_t	cpu_signature;
	uint32_t	feature_flags;
	uint32_t	reserved[2];
} __attribute__ ((packed)) fwts_mp_processor_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		bus_id;
	uint8_t		bus_type[6];
} __attribute__ ((packed)) fwts_mp_bus_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		id;
	uint8_t		version;
	uint8_t		flags;
	uint32_t	address;
} __attribute__ ((packed)) fwts_mp_io_apic_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		type;
	uint16_t	flags;
	uint8_t		source_bus_id;
	uint8_t		source_bus_irq;
	uint8_t		destination_io_apic_id;
	uint8_t		destination_io_apic_intin;
} __attribute__ ((packed)) fwts_mp_io_interrupt_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		type;
	uint16_t	flags;
	uint8_t		source_bus_id;
	uint8_t		source_bus_irq;
	uint8_t		destination_local_apic_id;
	uint8_t		destination_local_apic_intin;
} __attribute__ ((packed)) fwts_mp_local_interrupt_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		length;
	uint8_t		bus_id;
	uint8_t		address_type;
	uint64_t	address_base;
	uint64_t	address_length;
} __attribute__ ((packed)) fwts_mp_system_address_space_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		length;
	uint8_t		bus_id;
	uint8_t		bus_info;
	uint8_t		parent_bus;
	uint8_t		reserved[3];
} __attribute__ ((packed)) fwts_mp_bus_hierarchy_entry;

typedef struct {
	uint8_t		entry_type;
	uint8_t		length;
	uint8_t		bus_id;
	uint8_t		address_mod;
	uint32_t	range_list;
} __attribute__ ((packed)) fwts_mp_compat_bus_address_space_entry;

typedef struct {
        fwts_mp_config_table_header     *header;
	int				size;
        fwts_list                       entries;
	uint32_t			phys_addr;
} fwts_mp_data;

int fwts_mp_data_get(fwts_mp_data *data);
int fwts_mp_data_free(fwts_mp_data *data);

#endif
