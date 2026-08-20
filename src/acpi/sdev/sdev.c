/*
 * Copyright (C) 2017-2026 Canonical
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
acpi_table_init(SDEV, &table)

static void sdev_acpi_namespace_device_test(
	fwts_framework *fw,
	const fwts_acpi_table_sdev_acpi *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  ACPI Integrated Device (Type 0):");
	fwts_log_info_simp_int(fw, "    Type:                             ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Flags:                            ", entry->header.flags);
	if (entry->header.flags & 1)
		fwts_log_info_verbatim(fw, "      Allow handoff to unsecure OS");
	if (entry->header.flags & (1 << 1))
		fwts_log_info_verbatim(fw, "      Secure access components present");
	fwts_log_info_simp_int(fw, "    Length:                           ", entry->header.length);
	fwts_log_info_simp_int(fw, "    Device Id Offset:                 ", entry->device_id_offset);
	fwts_log_info_simp_int(fw, "    Device Id Length:                 ", entry->device_id_length);
	fwts_log_info_simp_int(fw, "    Vendor Specific Data Offset:      ", entry->vendor_offset);
	fwts_log_info_simp_int(fw, "    Vendor Specific Data Length:      ", entry->vendor_length);
	if (entry->header.flags & (1 << 1)) {
		fwts_log_info_simp_int(fw, "    Secure Access Components Offset:  ", entry->secure_access_offset);
		fwts_log_info_simp_int(fw, "    Secure Access Components Length:  ", entry->secure_access_length);
	}

	fwts_acpi_reserved_bits("SDEV", "Flags", entry->header.flags, 2, 15, passed);

}

static void sdev_pcie_endpoint_device_test(
	fwts_framework *fw,
	const fwts_acpi_table_sdev_pcie *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  PCIe Endpoint Device (Type 1):");
	fwts_log_info_simp_int(fw, "    Type:                             ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Flags:                            ", entry->header.flags);
	fwts_log_info_simp_int(fw, "    Length:                           ", entry->header.length);
	fwts_log_info_simp_int(fw, "    PCI Segment Number:               ", entry->segment);
	fwts_log_info_simp_int(fw, "    Start Bus Number:                 ", entry->start_bus);
	fwts_log_info_simp_int(fw, "    PCI Path Offset:                  ", entry->path_offset);
	fwts_log_info_simp_int(fw, "    PCI Path Length:                  ", entry->path_length);
	fwts_log_info_simp_int(fw, "    Vendor Specific Data Offset:      ", entry->vendor_offset);
	fwts_log_info_simp_int(fw, "    Vendor Specific Data Length:      ", entry->vendor_length);

	fwts_acpi_reserved_bits("SDEV", "Flags", entry->header.flags, 1, 15, passed);
}

static int sdev_test1(fwts_framework *fw)
{
	fwts_acpi_table_sdev_header *entry;
	bool passed = true;
	uint32_t offset;

	fwts_log_info_verbatim(fw, "SDEV Secure Devices Table:");

	entry = (fwts_acpi_table_sdev_header *) (table->data + sizeof(fwts_acpi_table_sdev));
	offset = sizeof(fwts_acpi_table_sdev);
	while (offset < table->length) {
		uint32_t type_length;

		if (fwts_acpi_structure_length_zero(fw, "SDEV", entry->length, offset)) {
			passed = false;
			break;
		}

		if (entry->type == FWTS_SDEV_TYPE_ACPI_NAMESPACE) {
			fwts_acpi_table_sdev_acpi *acpi = (fwts_acpi_table_sdev_acpi *) entry;
			sdev_acpi_namespace_device_test(fw, acpi, &passed);
			type_length = sizeof(fwts_acpi_table_sdev_acpi) + acpi->device_id_length +
				      acpi->vendor_length;
			if (acpi->header.flags & (1 << 1))
				type_length += acpi->secure_access_length;
			else {
				type_length -= (sizeof(acpi->secure_access_offset) +
						sizeof(acpi->secure_access_length));
			}

		} else if (entry->type == FWTS_SDEV_TYPE_PCIE_ENDPOINT) {
			fwts_acpi_table_sdev_pcie *pcie = (fwts_acpi_table_sdev_pcie *) entry;
			sdev_pcie_endpoint_device_test(fw, pcie, &passed);
			type_length = sizeof(fwts_acpi_table_sdev_pcie) + pcie->path_length + pcie->vendor_length;
		} else {
			fwts_acpi_reserved_type(fw, "SDEV", entry->type, 0, FWTS_SDEV_TYPE_RESERVED, &passed);
			break;
		}

		if (!fwts_acpi_structure_length(fw, "SDEV", entry->type, entry->length, type_length)) {
			passed = false;
			break;
		}

		offset += entry->length;
		if (fwts_acpi_structure_range(fw, "SDEV", table->length, offset)) {
			passed = false;
			break;
		}

		entry = (fwts_acpi_table_sdev_header *) (table->data + offset);
		fwts_log_nl(fw);
	}

	fwts_log_nl(fw);
	if (passed)
		fwts_passed(fw, "No issues found in SDEV table.");

	return FWTS_OK;
}

static fwts_framework_minor_test sdev_tests[] = {
	{ sdev_test1, "Validate SDEV table." },
	{ NULL, NULL }
};

static fwts_framework_ops sdev_ops = {
	.description = "SDEV Secure Devices Table test",
	.init        = SDEV_init,
	.minor_tests = sdev_tests
};

FWTS_REGISTER("sdev", &sdev_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
