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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
acpi_table_init(ASPT, &table)

static int aspt_revision1_test(fwts_framework *fw, fwts_acpi_table_aspt *aspt)
{
	bool passed = true;
	uint32_t offset;
	fwts_acpi_table_asp_global *entry_global;
	fwts_acpi_table_sev_mailbox *entry_sev;
	fwts_acpi_table_acpi_mailbox *entry_acpi;

	fwts_log_info_verbatim(fw, "AMD Secure Processor Table:");
	fwts_log_info_simp_int(fw, "  ASP Register Structure Count:    ", aspt->v1.asp_reg_count);
	offset = sizeof(aspt->header) + sizeof(aspt->v1.asp_reg_count);

	for (uint32_t i = 0; i <  aspt->v1.asp_reg_count; i++) {
		if ((offset + sizeof(fwts_aspt_sub_header)) > table->length) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASPTOutOfRangeOffset",
			"ASPT offset is out of range.");
			return FWTS_OK;
		}
		uint16_t type = ((fwts_aspt_sub_header *)((uint8_t *)table->data + offset))->type;
		switch (type) {
			case 0:
				entry_global = (fwts_acpi_table_asp_global *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  ASP Global Registers:");
				fwts_log_info_simp_int(fw, "    Type:                              ", entry_global->header.type);
				fwts_log_info_simp_int(fw, "    Length:                            ", entry_global->header.length);
				fwts_log_info_simp_int(fw, "    Reserved:                          ", entry_global->reserved);
				fwts_acpi_reserved_zero("ASPT", "Reserved", entry_global->reserved, &passed);
				fwts_log_info_simp_int(fw, "    Feature Register Address:          ", entry_global->v1.feature_reg_addr);
				fwts_log_info_simp_int(fw, "    Interrupt Enable Register Address: ", entry_global->v1.interrupt_enable_reg_addr);
				fwts_log_info_simp_int(fw, "    Interrupt Status Register Address: ", entry_global->v1.interrupt_stable_reg_addr);
				offset += entry_global->header.length;
				break;
			case 1:
				entry_sev = (fwts_acpi_table_sev_mailbox *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  SEV Mailbox Registers:");
				fwts_log_info_simp_int(fw, "    Type:                              ", entry_sev->header.type);
				fwts_log_info_simp_int(fw, "    Length:                            ", entry_sev->header.length);
				fwts_log_info_simp_int(fw, "    Mailbox Interrupt ID:              ", entry_sev->mailbox_interrupt_id);
				fwts_acpi_reserved_bits("ASPT", "Mailbox Interrupt ID",  entry_sev->mailbox_interrupt_id, 6, 7, &passed);
				fwts_log_info_verbatim(fw, "    Reserved:");
				fwts_hexdump_data_prefix_all(fw, entry_sev->reserved, "      ", sizeof(entry_sev->reserved));
				fwts_acpi_reserved_zero_array(fw, "ASPT", "Reserved", entry_sev->reserved, sizeof(entry_sev->reserved), &passed);
				fwts_log_info_simp_int(fw, "    CmdResp Register Address:          ", entry_sev->v1.cmdresp_reg_addr);
				fwts_log_info_simp_int(fw, "    CmdBufAddr_Lo Register Address:    ", entry_sev->v1.cmdbufaddr_lo_reg_addr);
				fwts_log_info_simp_int(fw, "    CmdBufAddr_Hi Register Address:    ", entry_sev->v1.cmdbufaddr_hi_reg_addr);
				offset += entry_sev->header.length;
				break;
			case 2:
				entry_acpi = (fwts_acpi_table_acpi_mailbox *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  ACPI Mailbox Registers:");
				fwts_log_info_simp_int(fw, "    Type:                              ", entry_acpi->header.type);
				fwts_log_info_simp_int(fw, "    Length:                            ", entry_acpi->header.length);
				fwts_log_info_simp_int(fw, "    Reserved:                          ", entry_acpi->reserved);
				fwts_acpi_reserved_zero("ASPT", "Reserved", entry_acpi->reserved, &passed);
				fwts_log_info_simp_int(fw, "    CmdResp Register Address:          ", entry_acpi->v1.cmdresp_reg_addr);
				fwts_log_info_verbatim(fw, "    Reserved:");
				fwts_hexdump_data_prefix_all(fw, entry_acpi->v1.reserved1, "      ", sizeof(entry_acpi->v1.reserved1));
				fwts_acpi_reserved_zero_array(fw, "ASPT", "Reserved", entry_acpi->v1.reserved1, sizeof(entry_acpi->v1.reserved1), &passed);
				offset += entry_acpi->header.length;
				break;
			default:
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"ASPTBadType",
					"ASPT register structures must have type with 0, 1 "
					"and 2, got %" PRIu16 " instead",
					((fwts_aspt_sub_header *)((uint8_t *)table->data + offset))->type);
				return passed;
			fwts_log_nl(fw);
		}
	}

	return passed;
}

static int aspt_revision2_test(fwts_framework *fw, fwts_acpi_table_aspt *aspt)
{
	bool passed = true;
	uint32_t offset;
	fwts_acpi_table_asp_global *entry_global;
	fwts_acpi_table_sev_mailbox *entry_sev;
	fwts_acpi_table_acpi_mailbox *entry_acpi;
	fwts_acpi_table_tee_interface *entry_tee;
	fwts_acpi_table_sfs_status *entry_sfs;

	fwts_log_info_verbatim(fw, "AMD Secure Processor Table:");
	fwts_log_info_simp_int(fw, "  ASP Register Base Address:       ", aspt->v2.asp_reg_base_addr);
	fwts_log_info_simp_int(fw, "  ASP Register Space Pages:        ", aspt->v2.asp_reg_apace_pages);
	fwts_log_info_simp_int(fw, "  ASP Register Structure Count:    ", aspt->v2.asp_reg_count);
	offset = sizeof(aspt->header) + sizeof(aspt->v2.asp_reg_base_addr) +
			sizeof(aspt->v2.asp_reg_apace_pages) + sizeof(aspt->v2.asp_reg_count);

	for (uint32_t i = 0; i <  aspt->v2.asp_reg_count; i++) {
		if ((offset + sizeof(fwts_aspt_sub_header)) > table->length) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASPTOutOfRangeOffset",
			"ASPT offset is out of range.");
			return FWTS_OK;
		}
		uint16_t type = ((fwts_aspt_sub_header *)((uint8_t *)table->data + offset))->type;
		switch (type) {
			case 0:
				entry_global = (fwts_acpi_table_asp_global *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  ASP Global Registers:");
				fwts_log_info_simp_int(fw, "    Type:                             ", entry_global->header.type);
				fwts_log_info_simp_int(fw, "    Length:                           ", entry_global->header.length);
				fwts_log_info_simp_int(fw, "    Reserved:                         ", entry_global->reserved);
				fwts_acpi_reserved_zero("ASPT", "Reserved", entry_global->reserved, &passed);
				fwts_log_info_simp_int(fw, "    Feature Register Offset:          ", entry_global->v2.feature_reg_offset);
				fwts_log_info_simp_int(fw, "    Interrupt Enable Register Offset: ", entry_global->v2.interrupt_enable_reg_offset);
				fwts_log_info_simp_int(fw, "    Interrupt Status Register Offset: ", entry_global->v2.interrupt_stable_reg_offset);
				offset += entry_global->header.length;
				break;
			case 1:
				entry_sev = (fwts_acpi_table_sev_mailbox *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  SEV Mailbox Registers:");
				fwts_log_info_simp_int(fw, "    Type:                             ", entry_sev->header.type);
				fwts_log_info_simp_int(fw, "    Length:                           ", entry_sev->header.length);
				fwts_log_info_simp_int(fw, "    Mailbox Interrupt ID:             ", entry_sev->mailbox_interrupt_id);
				fwts_acpi_reserved_bits("ASPT", "Mailbox Interrupt ID",  entry_sev->mailbox_interrupt_id, 6, 7, &passed);
				fwts_log_info_verbatim(fw, "    Reserved:");
				fwts_hexdump_data_prefix_all(fw, entry_sev->reserved, "      ", sizeof(entry_sev->reserved));
				fwts_acpi_reserved_zero_array(fw, "ASPT", "Reserved", entry_sev->reserved, sizeof(entry_sev->reserved), &passed);
				fwts_log_info_simp_int(fw, "    CmdResp Register Offset:          ", entry_sev->v2.cmdresp_reg_offset);
				fwts_log_info_simp_int(fw, "    CmdBufAddr_Lo Register Offset:    ", entry_sev->v2.cmdbufaddr_lo_reg_offset);
				fwts_log_info_simp_int(fw, "    CmdBufAddr_Hi Register Offset:    ", entry_sev->v2.cmdbufaddr_hi_reg_offset);
				offset += entry_sev->header.length;
				break;
			case 2:
				entry_acpi = (fwts_acpi_table_acpi_mailbox *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  ACPI Mailbox Registers:");
				fwts_log_info_simp_int(fw, "    Type:                             ", entry_acpi->header.type);
				fwts_log_info_simp_int(fw, "    Length:                           ", entry_acpi->header.length);
				fwts_log_info_simp_int(fw, "    Reserved:                         ", entry_acpi->reserved);
				fwts_acpi_reserved_zero("ASPT", "Reserved", entry_acpi->reserved, &passed);
				fwts_log_info_simp_int(fw, "    CmdResp Register Offset:          ", entry_acpi->v2.cmdresp_reg_offset);
				fwts_log_info_verbatim(fw, "    Reserved:");
				fwts_hexdump_data_prefix_all(fw, entry_acpi->v2.reserved1, "       ", sizeof(entry_acpi->v2.reserved1));
				fwts_acpi_reserved_zero_array(fw, "ASPT", "Reserved", entry_acpi->v2.reserved1, sizeof(entry_acpi->v2.reserved1), &passed);
				offset += entry_acpi->header.length;
				break;
			case 3:
				entry_tee = (fwts_acpi_table_tee_interface *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  ASP TEE Interface Registers:");
				fwts_log_info_simp_int(fw, "    Type:                             ", entry_tee->header.type);
				fwts_log_info_simp_int(fw, "    Length:                           ", entry_tee->header.length);
				fwts_log_info_simp_int(fw, "    Revision:                         ", entry_tee->revision);
				fwts_log_info_simp_int(fw, "    Reserved:                         ", entry_tee->reserved);
				fwts_acpi_reserved_zero("ASPT", "Reserved", entry_tee->reserved, &passed);
				fwts_log_info_simp_int(fw, "    CmdResp Register Offset:          ", entry_tee->cmdresp_reg_offset);
				fwts_log_info_simp_int(fw, "    CmdBufAddr_Lo Register Offset:    ", entry_tee->cmdbufaddr_lo_reg_offset);
				fwts_log_info_simp_int(fw, "    CmdBufAddr_Hi Register Offset:    ", entry_tee->cmdbufaddr_hi_reg_offset);
				offset += entry_tee->header.length;
				break;
			case 4:
				entry_sfs = (fwts_acpi_table_sfs_status *)((uint8_t *)table->data + offset);
				fwts_log_info_verbatim(fw, "  ASP SFS Status Registers:");
				fwts_log_info_simp_int(fw, "    Type:                             ", entry_sfs->header.type);
				fwts_log_info_simp_int(fw, "    Length:                           ", entry_sfs->header.length);
				fwts_log_info_simp_int(fw, "    Revision:                         ", entry_sfs->revision);
				fwts_log_info_verbatim(fw, "    Reserved:");
				fwts_hexdump_data_prefix_all(fw, entry_sfs->reserved, "      ", sizeof(entry_sfs->reserved));
				fwts_acpi_reserved_zero_array(fw, "ASPT", "Reserved", entry_sfs->reserved, sizeof(entry_sfs->reserved), &passed);
				fwts_log_info_simp_int(fw, "    SFS Status Memory Buffer Address: ", entry_sfs->sfs_status_mem_buf_addr);
				offset += entry_sfs->header.length;
				break;
			default:
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"ASPTBadType",
					"ASPT register structures must have type with 0 to 4, "
					"got %" PRIu16 " instead",
					((fwts_aspt_sub_header *)((uint8_t *)table->data + offset))->type);
				return passed;
			fwts_log_nl(fw);
		}
	}

	return passed;
}

static int aspt_test1(fwts_framework *fw)
{
	bool passed = true;

	fwts_acpi_table_aspt *aspt = (fwts_acpi_table_aspt *)table->data;

	switch (aspt->header.revision) {
		case 1:
			passed = aspt_revision1_test(fw, aspt);
			break;
		case 2:
			passed = aspt_revision2_test(fw, aspt);
			break;
		default:
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASPTUnknowRevision",
				"ASPT revision is unknown, currently must be one or two, "
				" but got %" PRIu8 " instead", aspt->header.revision);
			return FWTS_OK;
	}

	if (passed)
		fwts_passed(fw, "No issues found in ASPT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test aspt_tests[] = {
	{ aspt_test1, "ASPT Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops aspt_ops = {
	.description = "ASPT Table test.",
	.init        = ASPT_init,
	.minor_tests = aspt_tests
};

FWTS_REGISTER("aspt", &aspt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
