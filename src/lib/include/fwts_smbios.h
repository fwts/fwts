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

#ifndef __FWTS_SMBIOS_H__
#define __FWTS_SMBIOS_H__

#include <stdint.h>
#include "fwts.h"

#define FWTS_SMBIOS_REGION_START     (0x000e0000)
#define FWTS_SMBIOS_REGION_END       (0x000fffff)
#define FWTS_SMBIOS_REGION_SIZE      (FWTS_SMBIOS_REGION_END - FWTS_SMBIOS_REGION_START)

/* SMBIOS chassis in type 3 (Table 17 - System Enclosure or Chassis Types) */
enum {
	FWTS_SMBIOS_CHASSIS_INVALID,
	FWTS_SMBIOS_CHASSIS_OTHER,
	FWTS_SMBIOS_CHASSIS_UNKNOWN,
	FWTS_SMBIOS_CHASSIS_DESKTOP,
	FWTS_SMBIOS_CHASSIS_LOW_PROFILE_DESKTOP,
	FWTS_SMBIOS_CHASSIS_PIZZA_BOX,
	FWTS_SMBIOS_CHASSIS_MINI_TOWER,
	FWTS_SMBIOS_CHASSIS_TOWER,
	FWTS_SMBIOS_CHASSIS_PORTABLE,
	FWTS_SMBIOS_CHASSIS_LAPTOP,
	FWTS_SMBIOS_CHASSIS_NOTEBOOK,
	FWTS_SMBIOS_CHASSIS_HANDHELD,
	FWTS_SMBIOS_CHASSIS_DOCKING_STATION,
	FWTS_SMBIOS_CHASSIS_ALL_IN_ONE,
	FWTS_SMBIOS_CHASSIS_SUB_NOTEBOOK,
	FWTS_SMBIOS_CHASSIS_SPACE_SAVING,
	FWTS_SMBIOS_CHASSIS_LUNCH_BOX,
	FWTS_SMBIOS_CHASSIS_MAIN_SERVER_CHASSIS,
	FWTS_SMBIOS_CHASSIS_EXPANISON_CHASSIS,
	FWTS_SMBIOS_CHASSIS_SUB_CHASSIS,
	FWTS_SMBIOS_CHASSIS_BUS_EXPANSION_CHASSIS,
	FWTS_SMBIOS_CHASSIS_PERIPHERAL_CHASSIS,
	FWTS_SMBIOS_CHASSIS_RAID_CHASSIS,
	FWTS_SMBIOS_CHASSIS_RACK_MOUNT_CHASSIS,
	FWTS_SMBIOS_CHASSIS_SEALED_CASE_PC,
	FWTS_SMBIOS_CHASSIS_MULTI_SYSTEM_CHASSIS,
	FWTS_SMBIOS_CHASSIS_COMPACT_PCI,
	FWTS_SMBIOS_CHASSIS_ADVANCED_TCA,
	FWTS_SMBIOS_CHASSIS_BLADE,
	FWTS_SMBIOS_CHASSIS_BLADE_ENCLOSURE,
	FWTS_SMBIOS_CHASSIS_TABLET,
	FWTS_SMBIOS_CHASSIS_CONVERTIBLE,
	FWTS_SMBIOS_CHASSIS_DETACHABLE,
	FWTS_SMBIOS_CHASSIS_IOT_GATEWAY,
	FWTS_SMBIOS_CHASSIS_EMBEDDED_PC,
	FWTS_SMBIOS_CHASSIS_MINI_PC,
	FWTS_SMBIOS_CHASSIS_STICK_PC,
	/* end of the chassis types */
	FWTS_SMBIOS_CHASSIS_MAX
};

typedef enum {
	FWTS_SMBIOS_UNKNOWN = -1,
	FWTS_SMBIOS_DMI_LEGACY = 0,
	FWTS_SMBIOS = 1
} fwts_smbios_type;

typedef struct {
	uint8_t  type;
	uint8_t  length;
	uint16_t handle;
	uint8_t  *data;
} fwts_dmi_header;

/*
 * From System Management BIOS (SMBIOS) Reference Specification
 * http://dmtf.org/standards/smbios
 */
typedef struct {
	uint8_t		signature[4];
	uint8_t		checksum;
	uint8_t		length;
	uint8_t		major_version;
	uint8_t		minor_version;
	uint16_t	max_struct_size;
	uint8_t		revision;
	uint8_t		formatted_area[5];
	uint8_t		anchor_string[5];
	uint8_t		intermediate_checksum;
	uint16_t	struct_table_length;
	uint32_t	struct_table_address;
	uint16_t	number_smbios_structures;
	uint8_t		smbios_bcd_revision;
}  __attribute__ ((packed)) fwts_smbios_entry;

/*
 * From System Management BIOS (SMBIOS) Reference Specification 3.0.0
 */
typedef struct {
	uint8_t		signature[5];
	uint8_t		checksum;
	uint8_t		length;
	uint8_t		major_version;
	uint8_t		minor_version;
	uint8_t		docrev;
	uint8_t		revision;
	uint8_t		reserved;
	uint32_t	struct_table_max_size;
	uint64_t	struct_table_address;
}  __attribute__ ((packed)) fwts_smbios30_entry;

void *fwts_smbios_find_entry(fwts_framework *fw, fwts_smbios_entry *entry, fwts_smbios_type *type, uint16_t *version);
void *fwts_smbios30_find_entry(fwts_framework *fw, fwts_smbios30_entry *entry, uint16_t *version);

#endif
