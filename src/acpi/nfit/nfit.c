/*
 * Copyright (C) 2016-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#define EFI_MEMORY_UC			0x00000001
#define EFI_MEMORY_WC			0x00000002
#define EFI_MEMORY_WT			0x00000004
#define EFI_MEMORY_WB			0x00000008
#define EFI_MEMORY_UCE			0x00000010
#define EFI_MEMORY_WP			0x00001000
#define EFI_MEMORY_RP			0x00002000
#define EFI_MEMORY_XP			0x00004000
#define EFI_MEMORY_NV			0x00008000
#define EFI_MEMORY_MORE_RELIABLE	0x00010000
#define EFI_MEMORY_RO			0x00020000
#define EFI_MEMORY_SP			0x00040000

#define EFI_MEMORY_ALL (EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB | \
			 EFI_MEMORY_UCE | EFI_MEMORY_WP | EFI_MEMORY_RP | EFI_MEMORY_XP | \
			 EFI_MEMORY_NV | EFI_MEMORY_MORE_RELIABLE | EFI_MEMORY_RO | EFI_MEMORY_SP)

static const uint8_t guid_virtual_device[4][16] = {
	// Virtual Disk Region - Volatile
	{ 0x5a, 0x53, 0xab, 0x77, 0xfc, 0x45, 0x4b, 0x62, 0x55, 0x60, 0xf7, 0xb2, 0x81, 0xd1, 0xf9, 0x6e },
	// Virtual CD Region - Volatile
	{ 0x30, 0xbd, 0x5a, 0x3d, 0x75, 0x41, 0xce, 0x87, 0x6d, 0x64, 0xd2, 0xad, 0xe5, 0x23, 0xc4, 0xbb },
	// Virtual Disk Region - Persistent
	{ 0xc9, 0x02, 0xea, 0x5c, 0x07, 0x4d, 0xd3, 0x69, 0x26, 0x9f, 0x44, 0x96, 0xfb, 0xe0, 0x96, 0xf9 },
	// Virtual CD Region - Persistent
	{ 0x88, 0x81, 0x01, 0x08, 0xcd, 0x42, 0x48, 0xbb, 0x10, 0x0f, 0x53, 0x87, 0xd5, 0x3d, 0xed, 0x3d },
};

static fwts_acpi_table_info *nfit_table;
acpi_table_init(NFIT, &nfit_table)

static bool check_length(fwts_framework *fw, const int actual, int min, const char *name)
{
	if (actual < min) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "NFITSubtableLength",
			    "NFIT Subtable %s length %d bytes is too short, expected >= %d bytes",
			    name, actual, min);
		return false;
	}
	return true;
}

static bool scan_nfit_smbios(fwts_framework *fw, int len, uint8_t *table)
{
	int entry = 0;

	while (len > 4) {
		int strbytes = 0;
		fwts_dmi_header *hdr = (fwts_dmi_header *) table;

		fwts_log_info_verbatim(fw, "  NFIT SMBIOS Entry %d:", entry++);
		fwts_log_info_simp_int(fw, "    Type:                                   ", hdr->type);
		fwts_log_info_simp_int(fw, "    Length:                                 ", hdr->length);
		fwts_log_info_simp_int(fw, "    Handle:                                 ", hdr->handle);

		if (hdr->length < 4) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "NFIT_SMBIOS_EntryLength",
				    "NFIT SMBIOS Entry length %d bytes is too short (less than 4 bytes)",
				    hdr->length);
			return false;
		}
		len -= hdr->length;
		table += hdr->length;

		/* Look for structure terminator, ends in two zero bytes */
		while (len > 2 && (table[0] != 0 || table[1] != 0)) {
			strbytes++;
		        table++;
			len--;
		}
		fwts_log_info_verbatim(fw, "    Strings:                                %d bytes", strbytes);

		/* Skip over terminating two zero bytes, see section 6.1 of spec */
		table += 2;
		len -= 2;

	}
	if (len) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "NFIT_SMBIOS_ListLength",
			    "NFIT SMBIOS structure does not end with a complete entry");
		return false;
	}
	return true;
}

/*
 *  NFIT NVDIMM Firmware Interface Table
 */
static int nfit_test1(fwts_framework *fw)
{
	fwts_acpi_table_nfit *nfit = (fwts_acpi_table_nfit*)nfit_table->data;
	fwts_acpi_table_nfit_struct_header *entry;
	uint32_t offset;
	bool passed = true;

	fwts_log_info_verbatim(fw, "NFIT NVDIMM Firmware Interface Table:");
	fwts_log_info_simp_int(fw, "  Reserved:                 ", nfit->reserved);
	fwts_log_nl(fw);

	fwts_acpi_reserved_zero("NFIT", "Reserved", nfit->reserved, &passed);

	offset = sizeof(fwts_acpi_table_nfit);
	entry = (fwts_acpi_table_nfit_struct_header *)(nfit_table->data + offset);

	while (offset < nfit_table->length) {
		uint64_t reserved_passed = 0;

		fwts_log_info_verbatim(fw, "  NFIT Subtable:");
		fwts_log_info_simp_int(fw, "    Type:                                   ", entry->type);
		fwts_log_info_simp_int(fw, "    Length:                                 ", entry->length);

		if (fwts_acpi_structure_length_zero(fw, "NFIT", entry->length, offset)) {
			passed = false;
			break;
		}

		if (entry->type == FWTS_NFIT_TYPE_SYSTEM_ADDRESS) {
			fwts_acpi_table_nfit_system_memory *nfit_struct = (fwts_acpi_table_nfit_system_memory *) entry;
			char guid_str[37];
			bool guid_skip = false;
			size_t i;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_SYSTEM_ADDRESS,
					FWTS_NFIT_NAME_SYSTEM_ADDRESS);
			if (!ret) {
				passed = false;
				break;
			}

			fwts_guid_buf_to_str(nfit_struct->range_guid, guid_str, sizeof(guid_str));

			fwts_log_info_simp_int(fw, "    SPA Range Structure Index:              ", nfit_struct->range_index);
			fwts_log_info_simp_int(fw, "    Flags:                                  ", nfit_struct->flags);
			fwts_log_info_simp_int(fw, "    Reserved:                               ", nfit_struct->reserved);
			fwts_log_info_simp_int(fw, "    Proximity Domain:                       ", nfit_struct->proximity_domain);
			fwts_log_info_verbatim(fw, "    Address Range Type GUID:                %s", guid_str);
			fwts_log_info_simp_int(fw, "    System Physical Address Range Base:     ", nfit_struct->address);
			fwts_log_info_simp_int(fw, "    System Physical Address Range Length:   ", nfit_struct->length);
			fwts_log_info_simp_int(fw, "    Address Range Memory Mapping Attribute: ", nfit_struct->memory_mapping);

			if (fwts_get_acpi_version(fw) >= FWTS_ACPI_VERSION_64)
				fwts_log_info_simp_int(fw, "    SpaLocationCookie:                      ", nfit_struct->spa_location_cookie);

			/* SPA Range Structure Index can be 0 for Virtual CD Region and
			   Virtual Disk Region (both volatile and persistent) */
			for (i = 0; i < 4; i++) {
				if (fwts_guid_match(nfit_struct->range_guid, guid_virtual_device[i], 16)) {
					guid_skip = true;
					break;
				}
			}

			if (guid_skip == false && nfit_struct->range_index == 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NFITBadRangeIndexZero",
					"NFIT SPA Range Structure Index must not be zero");
			}

			if (guid_skip) {
				if (nfit_struct->range_index != 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"NFITBadRangeIndexNonZero",
						"NFIT SPA Range Structure Index must "
						"be zero when region is Virtual CD or "
						"Virtual Disk, got 0x%4.4" PRIx32 " instead",
						nfit_struct->range_index);
				}

				if (nfit_struct->flags != 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"NFITBadFlagsNonZero",
						"NFIT Flags must be zero when "
						"region is Virtual CD or Virtual Disk, got "
						"0x%4.4" PRIx32 " instead",
						nfit_struct->flags);
				}

				if (nfit_struct->proximity_domain != 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"NFITBadProximityDomainNonZero",
						"NFIT Proximity Domain must be zero when "
						"region is Virtual CD or Virtual Disk, got "
						"0x%8.8" PRIx32 " instead",
						nfit_struct->proximity_domain);
				}

				if (nfit_struct->memory_mapping != 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"NFITBadMemoryAttributeNonZero",
						"NFIT Address Range Memory Mapping Attribute "
						"must be zero when region is Virtual CD or "
						"Virtual Disk, got 0x%16.16" PRIx64 " instead",
						nfit_struct->memory_mapping);
				}
			}

			fwts_acpi_reserved_bits("NFIT", "Flags", nfit_struct->flags, 3, 15, &passed);

			if (nfit_struct->reserved != 0)
				reserved_passed = nfit_struct->reserved;

			/* TODO check Proximity Domain with SRAT table */

			if (nfit_struct->memory_mapping & ~EFI_MEMORY_ALL) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NFITBadMemoryMappingAttribute",
					"NFIT Memory Mapping Attribute must meet UEFI Spec, got "
					"0x%16.16" PRIx64 " instead", nfit_struct->memory_mapping);
			}

		} else if (entry->type == FWTS_NFIT_TYPE_MEMORY_MAP) {
			fwts_acpi_table_nfit_memory_map *nfit_struct = (fwts_acpi_table_nfit_memory_map *) entry;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_MEMORY_MAP,
					FWTS_NFIT_NAME_MEMORY_MAP);
			if (!ret) {
				passed = false;
				break;
			}

			fwts_log_info_simp_int(fw, "    NFIT Device Handle:                     ", nfit_struct->device_handle);
			fwts_log_info_simp_int(fw, "    NVDIMM Physical ID:                     ", nfit_struct->physical_id);
			fwts_log_info_simp_int(fw, "    NVDIMM Region ID:                       ", nfit_struct->region_id);
			fwts_log_info_simp_int(fw, "    SPA Range Structure Index:              ", nfit_struct->range_index);
			fwts_log_info_simp_int(fw, "    NVDIMM Control Region Structure Index:  ", nfit_struct->region_index);
			fwts_log_info_simp_int(fw, "    NVDIMM Region Size:                     ", nfit_struct->region_size);
			fwts_log_info_simp_int(fw, "    Region Offset:                          ", nfit_struct->region_offset);
			fwts_log_info_simp_int(fw, "    NVDIMM Physical Address Region Base:    ", nfit_struct->address);
			fwts_log_info_simp_int(fw, "    Interleave Structure Index:             ", nfit_struct->interleave_index);
			fwts_log_info_simp_int(fw, "    Interleave Ways:                        ", nfit_struct->interleave_ways);
			fwts_log_info_simp_int(fw, "    NVDIMM State Flags:                     ", nfit_struct->flags);
			fwts_log_info_simp_int(fw, "    Reserved:                               ", nfit_struct->reserved);

			fwts_acpi_reserved_bits("NFIT", "NVDIMM State Flags", nfit_struct->flags, 7, 15, &passed);

			if (nfit_struct->reserved != 0)
				reserved_passed = nfit_struct->reserved;

		} else if (entry->type == FWTS_NFIT_TYPE_INTERLEAVE) {
			fwts_acpi_table_nfit_interleave *nfit_struct = (fwts_acpi_table_nfit_interleave *) entry;
			uint32_t i;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_INTERLEAVE,
					FWTS_NFIT_NAME_INTERLEAVE);
			if (!ret) {
				passed = false;
				break;
			}

			fwts_log_info_simp_int(fw, "    Interleave Structure Index:             ", nfit_struct->interleave_index);
			fwts_log_info_simp_int(fw, "    Reserved:                               ", nfit_struct->reserved);
			fwts_log_info_simp_int(fw, "    Number of Lines Described:              ", nfit_struct->line_count);
			fwts_log_info_simp_int(fw, "    Line Size:                              ", nfit_struct->line_size);

			ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_INTERLEAVE +
						nfit_struct->line_count * sizeof nfit_struct->line_offset[0],
					FWTS_NFIT_NAME_INTERLEAVE);
			if (!ret) {
				passed = false;
				break;
			}

			for (i = 0; i < nfit_struct->line_count; i++)
				fwts_log_info_verbatim(fw,
					"    Line Offset:                            0x%8.8" PRIx32 ", SPA 0x%16.16" PRIx64,
					nfit_struct->line_offset[i],
					(uint64_t) nfit_struct->line_offset[i] * nfit_struct->line_size);

			if (nfit_struct->reserved != 0)
				reserved_passed = nfit_struct->reserved;

			if (nfit_struct->line_count == 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NFITBadLineCount",
					"NFIT Number of Lines must not be zero");
			}

		} else if (entry->type == FWTS_NFIT_TYPE_SMBIOS) {
			fwts_acpi_table_nfit_smbios *nfit_struct = (fwts_acpi_table_nfit_smbios *) entry;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_SMBIOS,
					FWTS_NFIT_NAME_SMBIOS);
			if (!ret) {
				passed = false;
				break;
			}

			fwts_log_info_simp_int(fw, "    Reserved:                               ", nfit_struct->reserved);
			if (nfit_struct->reserved != 0)
				reserved_passed = nfit_struct->reserved;

			if (entry->length < 8)
				break;

			ret = scan_nfit_smbios(fw, entry->length - 8, nfit_struct->smbios);
			if (!ret)
				passed = false;

		} else if (entry->type == FWTS_NFIT_TYPE_CONTROL_REGION) {
			fwts_acpi_table_nfit_control_range *nfit_struct = (fwts_acpi_table_nfit_control_range *) entry;
			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_CONTROL_REGION,
					FWTS_NFIT_NAME_CONTROL_REGION);
			if (!ret) {
				passed = false;
				break;
			}

			fwts_log_info_simp_int(fw, "    Vendor ID:                              ", nfit_struct->vendor_id);
			fwts_log_info_simp_int(fw, "    Device ID:                              ", nfit_struct->device_id);
			fwts_log_info_simp_int(fw, "    Revision ID:                            ", nfit_struct->revision_id);
			fwts_log_info_simp_int(fw, "    Subsystem Vendor ID:                    ", nfit_struct->subsystem_vendor_id);
			fwts_log_info_simp_int(fw, "    Subsystem Device ID:                    ", nfit_struct->subsystem_device_id);
			fwts_log_info_simp_int(fw, "    Subsystem Revision ID:                  ", nfit_struct->subsystem_revision_id);
			fwts_log_info_simp_int(fw, "    Valid Fields:                           ", nfit_struct->valid_fields);
			fwts_log_info_simp_int(fw, "    Manufacturing Location:                 ", nfit_struct->manufacturing_location);
			fwts_log_info_simp_int(fw, "    Manufacturing Date:                     ", nfit_struct->manufacturing_date);
			fwts_log_info_simp_int(fw, "    Reserved:                               ", nfit_struct->reserved);
			fwts_log_info_simp_int(fw, "    Serial Number:                          ", nfit_struct->serial_number);
			fwts_log_info_simp_int(fw, "    Region Format Interface Code:           ", nfit_struct->interface_code);
			fwts_log_info_simp_int(fw, "    Number of Block Control Windows:        ", nfit_struct->windows_num);

			if (nfit_struct->revision_id & 0xFF00) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NFITBadRevisionId",
					"NFIT Revision ID's BYTE 1 must be zero, got "
					"0x%4.4" PRIx16 " instead", nfit_struct->revision_id);
			}

			if (nfit_struct->subsystem_revision_id & 0xFF00) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NFITBadRevisionId",
					"NFIT Subsystem Revision ID's BYTE 1 must be zero, got "
					"0x%4.4" PRIx16 " instead", nfit_struct->subsystem_revision_id);
			}

			if (nfit_struct->reserved != 0)
				reserved_passed = nfit_struct->reserved;

			fwts_acpi_reserved_bits("NFIT", "Valid", nfit_struct->valid_fields, 1, 7, &passed);

			if (entry->length >= sizeof(*nfit_struct)) {
				uint64_t reserved1;

				reserved1 = (uint64_t) nfit_struct->reserved1[0] +
					    ((uint64_t) nfit_struct->reserved1[1] << 8) +
					    ((uint64_t) nfit_struct->reserved1[2] << 16) +
					    ((uint64_t) nfit_struct->reserved1[3] << 24) +
					    ((uint64_t) nfit_struct->reserved1[4] << 32) +
					    ((uint64_t) nfit_struct->reserved1[5] << 40);

				if (reserved1 != 0)
					reserved_passed = reserved1;

				fwts_log_info_simp_int(fw, "    Size of Block Control Window:           ", nfit_struct->window_size);
				fwts_log_info_simp_int(fw, "    Command Register Offset:                ", nfit_struct->command_offset);
				fwts_log_info_simp_int(fw, "    Size of Command Register:               ", nfit_struct->command_size);
				fwts_log_info_simp_int(fw, "    Status RegisterOffset:                  ", nfit_struct->status_offset);
				fwts_log_info_simp_int(fw, "    Size of Status Register:                ", nfit_struct->status_size);
				fwts_log_info_simp_int(fw, "    NVDIMM Control Region Flag:             ", nfit_struct->flags);
				fwts_log_info_simp_int(fw, "    Reserved:                               ", reserved1);

				fwts_acpi_reserved_bits("NFIT", "NVDIMM Control Region Flags", nfit_struct->flags, 1, 15, &passed);
				fwts_log_info_simp_int(fw, "    NVDIMM Control Region Structure Index:  ", nfit_struct->region_index);
			}

		} else if (entry->type == FWTS_NFIT_TYPE_DATA_REGION) {
			fwts_acpi_table_nfit_data_range *nfit_struct = (fwts_acpi_table_nfit_data_range *) entry;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_DATA_REGION,
					FWTS_NFIT_NAME_DATA_REGION);
			if (!ret) {
				passed = false;
				break;
			}

			fwts_log_info_simp_int(fw, "    NVDIMM Control Region Structure Index:  ", nfit_struct->region_index);
			fwts_log_info_simp_int(fw, "    Number of Block Data Windows:           ", nfit_struct->window_num);
			fwts_log_info_simp_int(fw, "    Block Data Window Start Offset:         ", nfit_struct->window_offset);
			fwts_log_info_simp_int(fw, "    Size of Block Data Window:              ", nfit_struct->window_size);
			fwts_log_info_simp_int(fw, "    NBlock Accessible Memory Capacity:      ", nfit_struct->capacity);
			fwts_log_info_simp_int(fw, "    Beginning address of First Block:       ", nfit_struct->start_address);

			if (nfit_struct->region_index == 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NFITBadRegionIndex",
					"NFIT NVDIMM Control Region Structure Index must not be zero");
			}

		} else if (entry->type == FWTS_NFIT_TYPE_FLUSH_ADDRESS) {
			fwts_acpi_table_nfit_flush_addr *nfit_struct = (fwts_acpi_table_nfit_flush_addr *) entry;
			uint64_t reserved;
			uint16_t i;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_FLUSH_ADDRESS,
					FWTS_NFIT_NAME_FLUSH_ADDRESS);
			if (!ret) {
				passed = false;
				break;
			}

			reserved = (uint64_t) nfit_struct->reserved[0] + ((uint64_t) nfit_struct->reserved[1] << 8) +
				   ((uint64_t) nfit_struct->reserved[2] << 16) + ((uint64_t) nfit_struct->reserved[3] << 24) +
				   ((uint64_t) nfit_struct->reserved[4] << 32) + ((uint64_t) nfit_struct->reserved[5] << 40);

			fwts_log_info_simp_int(fw, "    NFIT Device Handle:                     ", nfit_struct->device_handle);
			fwts_log_info_simp_int(fw, "    Number of Flush Hint Addresses:         ", nfit_struct->hint_count);
			fwts_log_info_simp_int(fw, "    Reserved:                               ", reserved);

			ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_FLUSH_ADDRESS +
						nfit_struct->hint_count * sizeof nfit_struct->hint_address[0],
					FWTS_NFIT_NAME_FLUSH_ADDRESS);
			if (!ret) {
				passed = false;
				break;
			}

			for (i = 0; i < nfit_struct->hint_count; i++)
				fwts_log_info_simp_int(fw, "    Flush Hint Address:                     ", nfit_struct->hint_address[i]);

			if (reserved != 0)
				reserved_passed = reserved;

		} else if (entry->type == FWTS_NFIT_TYPE_PLATFORM_CAPABILITY) {
			fwts_acpi_table_nfit_platform_cap *nfit_struct = (fwts_acpi_table_nfit_platform_cap *) entry;
			uint32_t reserved1;

			bool ret = check_length(fw, entry->length,
					FWTS_NFIT_MINLEN_PLATFORM_CAPABILITY,
					FWTS_NFIT_NAME_PLATFORM_CAPABILITY);
			if (!ret) {
				passed = false;
				break;
			}

			reserved1 = (uint32_t) nfit_struct->reserved1[0] + ((uint32_t) nfit_struct->reserved1[1] << 8) +
				   ((uint32_t) nfit_struct->reserved1[2] << 16);

			fwts_log_info_simp_int(fw, "    Highest Valid Capability:               ", nfit_struct->highest_valid_cap);
			fwts_log_info_simp_int(fw, "    Reserved1:                              ", reserved1);
			fwts_log_info_simp_int(fw, "    Capabilities:                           ", nfit_struct->cap);
			fwts_log_info_simp_int(fw, "    Reserved2:                              ", nfit_struct->reserved2);

			fwts_acpi_reserved_zero("NFIT", "Reserved1", reserved1, &passed);
			fwts_acpi_reserved_bits("NFIT", "Capabilities", nfit_struct->cap, 3, 31, &passed);

			if ((nfit_struct->cap & 0x1) && !(nfit_struct->cap & 0x2)) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"NFITBadCapabilities",
					"NFIT Capabilities[1] must be set and if Capabilities[0] is set, got "
					"0x%8.8" PRIx32 " instead", nfit_struct->cap);
			}

			if (nfit_struct->reserved2 != 0)
				reserved_passed = nfit_struct->reserved2;

		} else {
			fwts_acpi_reserved_type(fw, "NFIT", entry->type, 0, FWTS_NFIT_TYPE_RESERVED, &passed);
			break;
		}

		fwts_acpi_reserved_zero("NFIT", "Reserved", reserved_passed, &passed);
		fwts_log_nl(fw);

		offset += entry->length;
		if (fwts_acpi_structure_range(fw, "NFIT", nfit_table->length, offset)) {
			passed = false;
			break;
		}

		entry = (fwts_acpi_table_nfit_struct_header *)(nfit_table->data + offset);
	}


	if (passed)
		fwts_passed(fw, "No issues found in NFIT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test nfit_tests[] = {
	{ nfit_test1, "NFIT NVDIMM Firmware Interface Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops nfit_ops = {
	.description = "NFIT NVDIMM Firmware Interface Table test.",
	.init        = NFIT_init,
	.minor_tests = nfit_tests
};

FWTS_REGISTER("nfit", &nfit_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
