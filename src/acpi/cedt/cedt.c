/*
 * Copyright (C) 2022-2026 Canonical
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
acpi_table_init(CEDT, &table)

static void cedt_chbs_test(
	fwts_framework *fw,
	const fwts_acpi_table_cedt_chbs *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  CXL Host Bridge Structure (CHBS):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->header.reserved);
	fwts_log_info_simp_int(fw, "    Record Length:                  ", entry->header.record_length);
	fwts_log_info_simp_int(fw, "    UID:                            ", entry->uid);
	fwts_log_info_simp_int(fw, "    CXL Version:                    ", entry->cxl_version);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved);
	fwts_log_info_simp_int(fw, "    Base:                           ", entry->base);
	fwts_log_info_simp_int(fw, "    Length:                         ", entry->length);

	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->header.reserved, passed);
	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->reserved, passed);

	switch(entry->cxl_version) {
		case 0: /* RCH */
			if (entry->length != 0x2000) {
				*passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"CEDTBadCHBSLength",
					"CEDT CHBS Length must be 0x2000, got "
					"0x%" PRIx64 " instead", entry->length);
			}
			break;
		case 1: /* Host Bridge */
			if (entry->length != 0x10000) {
				*passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"CEDTBadCHBSLength",
					"CEDT CHBS Length must be 0x10000, got "
					"0x%" PRIx64 " instead", entry->length);
			}
			break;
		default:
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"CEDTBadCXLVersion",
				"CXL Version must have the value with 0 or 1, got "
				"0x%" PRIx32 " instead", entry->cxl_version);
			break;
	}

}

static void cedt_cfmws_test(
	fwts_framework *fw,
	fwts_acpi_table_cedt_cfmws *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  CXL Fixed Memory Window Structure (CFMWS):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->header.reserved);
	fwts_log_info_simp_int(fw, "    Record Length:                  ", entry->header.record_length);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved1);
	fwts_log_info_simp_int(fw, "    Base HPA:                       ", entry->base_hpa);
	fwts_log_info_simp_int(fw, "    Window Size:                    ", entry->window_size);
	fwts_log_info_simp_int(fw, "    ENIW:                           ", entry->eniw);
	fwts_log_info_simp_int(fw, "    Interleave Arithmetic:          ", entry->interleave_arithmetic);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved2);
	fwts_log_info_simp_int(fw, "    HBIG:                           ", entry->hbig);
	fwts_log_info_simp_int(fw, "    Window Restrictions:            ", entry->window_restrictions);
	fwts_log_info_simp_int(fw, "    QTG ID:                         ", entry->qtg_id);
	fwts_log_info_verbatim(fw, "    Interleave Target List:");
	fwts_hexdump_data_prefix_all(fw, (uint8_t *)entry->interleave_target_list,
			"      ",sizeof(uint32_t) * entry->eniw );

	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->header.reserved, passed);
	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->reserved1, passed);
	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->reserved2, passed);

	if (entry->interleave_arithmetic > 1) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"CEDTBadInterleaveArithmetic",
			"CEDT CFMWS interleave arithmetic must have the value with 0 or 1, got "
			"0x%2.2" PRIx8 "instead", entry->interleave_arithmetic);
	}

	fwts_acpi_reserved_bits("CEDT", "Window Restrictions", entry->window_restrictions, 6, 15, passed);

}

static void cedt_cxims_test(
	fwts_framework *fw,
	const fwts_acpi_table_cedt_cxims *entry,
	bool *passed)
{
	int i;

	fwts_log_info_verbatim(fw, "  CXL XOR Interleave Math Structure (CXIMS):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->header.reserved);
	fwts_log_info_simp_int(fw, "    Record Length:                  ", entry->header.record_length);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved);
	fwts_log_info_simp_int(fw, "    HBIG:                           ", entry->hbig);
	fwts_log_info_simp_int(fw, "    Number of Bitmap Entries(NIB):  ", entry->nig);
	for (i= 0; i < entry->nig; i++)
		fwts_log_info_simp_int(fw, "    XORMAP List                     ", entry->xormap_list[i]);

	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->header.reserved, passed);
	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->reserved, passed);

}

static void cedt_rdpas_test(
	fwts_framework *fw,
	const fwts_acpi_table_cedt_rdpas *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  RCEC Downstream Port Association Structure (RDPAS):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->header.reserved);
	fwts_log_info_simp_int(fw, "    Record Length:                  ", entry->header.record_length);
	fwts_log_info_simp_int(fw, "    RCEC Segment Number             ", entry->rece_seg_num);
	fwts_log_info_simp_int(fw, "    RCEC BDF:                       ", entry->rece_bdf);
	fwts_log_info_simp_int(fw, "    Protocol Type:                  ", entry->protocol_type);
	fwts_log_info_simp_int(fw, "    Base Address                    ", entry->base_addr);

	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->header.reserved, passed);

	if (entry->protocol_type > 1) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"CEDTBadProtocolType",
			"CEDT CFMWS protocol type must have the value with 0 or 1, got "
			"0x%2.2" PRIx8 " instead", entry->protocol_type);
	}

}

static void cedt_csds_test(
	fwts_framework *fw,
	const fwts_acpi_table_cedt_csds *entry,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  CXL System Description Structure (CSDS):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->header.reserved);
	fwts_log_info_simp_int(fw, "    Record Length:                  ", entry->header.record_length);
	fwts_log_info_simp_int(fw, "    System Capabilities:            ", entry->system_cap);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved);

	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->header.reserved, passed);
	fwts_acpi_reserved_bits("CEDT", "System Capabilities", entry->system_cap, 6, 15, passed);
	fwts_acpi_reserved_zero("CEDT", "Reserved", entry->reserved, passed);

}

static void cedt_structure(
	fwts_framework *fw,
	fwts_acpi_table_cedt_header *entry,
	bool *passed)
{
	switch(entry->type) {
		case FWTS_CEDT_TYPE_CHBS:
			cedt_chbs_test(fw, (fwts_acpi_table_cedt_chbs *)entry, passed);
			break;
		case FWTS_CEDT_TYPE_CFMWS:
			cedt_cfmws_test(fw, (fwts_acpi_table_cedt_cfmws *)entry, passed);
			break;
		case FWTS_CEDT_TYPE_CXIMS:
			cedt_cxims_test(fw, (fwts_acpi_table_cedt_cxims *)entry, passed);
			break;
		case FWTS_CEDT_TYPE_RDPAS:
			cedt_rdpas_test(fw, (fwts_acpi_table_cedt_rdpas *)entry, passed);
			break;
		case FWTS_CEDT_TYPE_CSDS:
			cedt_csds_test(fw, (fwts_acpi_table_cedt_csds *)entry, passed);
			break;
		default:
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"CEDTBadSubtableType",
				"CEDT must have subtable with Type 0 to 4, got "
				"0x%4.4" PRIx16 " instead", entry->type);
			break;
	}
}

static int cedt_test1(fwts_framework *fw)
{
	fwts_acpi_table_cedt_header *entry;
	bool passed = true;
	uint32_t offset;

	if (!fwts_acpi_table_length(fw, "CEDT", table->length, sizeof(fwts_acpi_table_cedt)))
		return FWTS_OK;

	fwts_log_info_verbatim(fw, "CEDT CXL Early Discovery Table:");

	entry = (fwts_acpi_table_cedt_header *) (table->data + sizeof(fwts_acpi_table_cedt));
	offset = sizeof(fwts_acpi_table_cedt);
	while (offset < table->length) {

		if (fwts_acpi_structure_length_zero(fw, "CEDT", entry->record_length, offset)) {
			passed = false;
			break;
		}

		cedt_structure(fw, entry, &passed);

		offset += entry->record_length;
		if (fwts_acpi_structure_range(fw, "CEDT", table->length, offset)) {
			passed = false;
			break;
		}

		entry = (fwts_acpi_table_cedt_header *) (table->data + offset);
		fwts_log_nl(fw);
	}

	if (passed)
		fwts_passed(fw, "No issues found in CEDT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test cedt_tests[] = {
	{ cedt_test1, "Validate CEDT table." },
	{ NULL, NULL }
};

static fwts_framework_ops cedt_ops = {
	.description = "CEDT CXL Early Discovery Table test",
	.init        = CEDT_init,
	.minor_tests = cedt_tests
};

FWTS_REGISTER("cedt", &cedt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
