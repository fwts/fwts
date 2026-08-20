/*
 * Copyright (C) 2023-2026 Canonical
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

static fwts_acpi_table_info *table;
acpi_table_init(IVRS, &table)

static void ivrs_ivhd_device_entries_check(
	fwts_framework *fw,
	const uint8_t *entries,
	const uint16_t entries_len,
	bool *passed)
{
	uint8_t type_range = entries[0] >> 6;
	uint16_t offset = 0;

	while (offset < entries_len) {
		type_range = entries[offset] >> 6;
		if (type_range == 0) {
			fwts_acpi_reserved_type(fw, "IVRS", entries[offset], 1, 5, passed);
			offset += 4;

		} else if (type_range == 1) {
			if (entries[offset] < 66 || entries[offset] == 68 || entries[offset] == 68 || entries[offset] > 72) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"IVRSBadType",
					"IVRS must have device entry types 66..67 or 70..72"
					", got %" PRId8 " instead", entries[offset]);
				*passed = false;
			}
			offset += 8;
		} else
			break;
	}
}

static void ivrs_ivhd_10_test(
	fwts_framework *fw,
	const fwts_acpi_table_ivhd_10 *entry,
	bool *passed)
{

	uint16_t entries_len = entry->ivhd.length - sizeof(fwts_acpi_table_ivhd_10);

	fwts_log_info_verbatim(fw, "  I/O Virtualization Hardware Definition Type 10h:");
	fwts_log_info_simp_int(fw, "    Type:			", entry->ivhd.type);
	fwts_log_info_simp_int(fw, "    Flags:			", entry->ivhd.flags);
	fwts_log_info_simp_int(fw, "    Length:			", entry->ivhd.length);
	fwts_log_info_simp_int(fw, "    DeviceID:			", entry->ivhd.device_id);
	fwts_log_info_simp_int(fw, "    Capability Offset:		", entry->ivhd.capability_offset);
	fwts_log_info_simp_int(fw, "    IOMMU Base Address:		", entry->ivhd.iommu_base_address);
	fwts_log_info_simp_int(fw, "    PCI Segment Group:		", entry->ivhd.pci_seg_group);
	fwts_log_info_simp_int(fw, "    IOMMU Info:			", entry->ivhd.iommu_info);
	fwts_log_info_simp_int(fw, "    IOMMU Feature Reporting:	", entry->iommu_feature_reoprt);

	fwts_acpi_reserved_bits("IVRS", "IOMMU Info", entry->ivhd.iommu_info, 5, 7, passed);
	fwts_acpi_reserved_bits("IVRS", "IOMMU Info", entry->ivhd.iommu_info, 13, 15, passed);

	ivrs_ivhd_device_entries_check(fw, entry->ivhd_device_entries,entries_len , passed);
}

static void ivrs_ivhd_11_40_test(
	fwts_framework *fw,
	const fwts_acpi_table_ivhd_11_40 *entry,
	bool *passed)
{

	uint16_t entries_len = entry->ivhd.length - sizeof(fwts_acpi_table_ivhd_11_40);

	fwts_log_info_simp_int(fw, "    Type:			", entry->ivhd.type);
	fwts_log_info_simp_int(fw, "    Flags:			", entry->ivhd.flags);
	fwts_log_info_simp_int(fw, "    Length:			", entry->ivhd.length);
	fwts_log_info_simp_int(fw, "    DeviceID:			", entry->ivhd.device_id);
	fwts_log_info_simp_int(fw, "    Capability Offset:		", entry->ivhd.capability_offset);
	fwts_log_info_simp_int(fw, "    IOMMU Base Address:		", entry->ivhd.iommu_base_address);
	fwts_log_info_simp_int(fw, "    PCI Segment Group:		", entry->ivhd.pci_seg_group);
	fwts_log_info_simp_int(fw, "    IOMMU Info:			", entry->ivhd.iommu_info);
	fwts_log_info_simp_int(fw, "    IOMMU Attribute:		", entry->iommu_attr);
	fwts_log_info_simp_int(fw, "    EFR Register Image:		", entry->efr_reg_image);
	fwts_log_info_simp_int(fw, "    WFR Register Image 2:	", entry->efr_reg_image_2);
	fwts_acpi_reserved_bits("IVRS", "Flags", entry->ivhd.flags, 6, 7, passed);
	fwts_acpi_reserved_bits("IVRS", "IOMMU Info", entry->ivhd.iommu_info, 5, 7, passed);
	fwts_acpi_reserved_bits("IVRS", "IOMMU Info", entry->ivhd.iommu_info, 13, 15, passed);
	fwts_acpi_reserved_bits("IVRS", "IOMMU Attribute", entry->iommu_attr, 0, 12, passed);
	fwts_acpi_reserved_bits("IVRS", "IOMMU Attribute", entry->iommu_attr, 28, 31, passed);

	ivrs_ivhd_device_entries_check(fw, entry->ivhd_device_entries, entries_len, passed);
}

static void ivrs_ivmd_test(
	fwts_framework *fw,
	const fwts_acpi_table_ivmd *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  I/O Virtualization Memory Definition:");
	fwts_log_info_simp_int(fw, "    Type:			", entry->type);
	fwts_log_info_simp_int(fw, "    Flags:			", entry->flags);
	fwts_log_info_simp_int(fw, "    Length:			", entry->length);
	fwts_log_info_simp_int(fw, "    DeviceID:			", entry->device_id);
	fwts_log_info_simp_int(fw, "    Auxiliary data:		", entry->auxiliary_data);
	fwts_log_info_simp_int(fw, "    Reserved:			", entry->reserved);
	fwts_log_info_simp_int(fw, "    PCI Segment Group:		", entry->pci_seg_group);
	fwts_log_info_simp_int(fw, "    Start address:		", entry->start_address);
	fwts_log_info_simp_int(fw, "    Memory block length:	", entry->mem_block_len);

	fwts_acpi_reserved_bits("IVRS", "Flags", entry->flags, 4, 7, passed);
	fwts_acpi_reserved_zero("IVRS", "Reserved", entry->reserved, passed);

}

static int ivrs_test1(fwts_framework *fw)
{
	bool passed = true;
	fwts_acpi_table_ivrs *ivrs = (fwts_acpi_table_ivrs *)table->data;
	fwts_acpi_table_ivhd *entry;

	uint32_t offset;

	if (!fwts_acpi_table_length(fw, "IVRS", table->length, sizeof(fwts_acpi_table_ivrs)))
		return FWTS_OK;

	fwts_log_info_verbatim(fw, "IVRS I/O Virtualization Reporting Structure:");

	fwts_log_info_simp_int(fw, "  IVinfo:     ", ivrs->ivinfo);
	fwts_log_info_simp_int(fw, "  Reserved:   ", ivrs->reserved);
	fwts_log_nl(fw);

	if (ivrs->header.revision == 0 || ivrs->header.revision > 2) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"IVRSBadRevision",
			"IVRS format revision number must have the value with 1 or 2, got "
			"0x%2.2" PRIx8 "instead", ivrs->header.revision);
	}
	fwts_acpi_reserved_bits("IVRS", "IVinfo", ivrs->ivinfo, 2, 4, &passed);
	fwts_acpi_reserved_bits("IVRS", "IVinfo", ivrs->ivinfo, 23, 31, &passed);
	fwts_acpi_reserved_zero("IVRS", "Reserved", ivrs->reserved, &passed);

	entry = (fwts_acpi_table_ivhd *) (table->data + sizeof(fwts_acpi_table_ivrs));
	offset = sizeof(fwts_acpi_table_ivrs);

	while (offset < table->length) {

		switch(entry->type) {
			case 0x10:
				ivrs_ivhd_10_test(fw, (fwts_acpi_table_ivhd_10 *)entry, &passed);
				offset += entry->length;
				break;
			case 0x11:
				fwts_log_info_verbatim(fw, "  I/O Virtualization Hardware Definition Type 11h:");
				ivrs_ivhd_11_40_test(fw, (fwts_acpi_table_ivhd_11_40 *)entry, &passed);
				offset += entry->length;
				break;
			case 0x40:
				fwts_log_info_verbatim(fw, "  I/O Virtualization Hardware Definition Type 40h:");
				if (ivrs->header.revision != 2) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"IVRSBadRevision",
						"IVRS only allows the inclusion of IVHD type 40h on the revision 2, got "
						"0x%2.2" PRIx8 "instead", ivrs->header.revision);
				}
				ivrs_ivhd_11_40_test(fw, (fwts_acpi_table_ivhd_11_40 *)entry, &passed);
				offset += entry->length;
				break;
			case 0x20:
			case 0x21:
			case 0x22:
				ivrs_ivmd_test(fw, (fwts_acpi_table_ivmd *)entry, &passed);
				offset += entry->length;
				break;
			default:
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"IVRSBadIVHDType",
					"IVHD or IVMD must have type with Type 0x10, 0x11, 0x20 to 0x22 "
					"and 0x40 , got 0x%2.2" PRIx8 " instead", entry->type);
				offset += entry->length;
				break;
		}

		if (fwts_acpi_structure_range(fw, "IVRS", table->length, offset)) {
			passed = false;
			break;
		}

		entry = (fwts_acpi_table_ivhd *) (table->data + offset);
		fwts_log_nl(fw);
	}

	if (passed)
		fwts_passed(fw, "No issues found in IVRS table.");

	return FWTS_OK;
}

static fwts_framework_minor_test ivrs_tests[] = {
	{ ivrs_test1, "Validate IVRS table." },
	{ NULL, NULL }
};

static fwts_framework_ops ivrs_ops = {
	.description = "IVRS I/O Virtualization Reporting Structure test.",
	.init        = IVRS_init,
	.minor_tests = ivrs_tests
};

FWTS_REGISTER("ivrs", &ivrs_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
