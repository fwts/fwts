/*
 * Copyright (C) 2023 Intel Corporation
 * Copyright (C) 2025 Ventana Micro Systems Inc
 * Copyright (C) 2026 Canonical
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
acpi_table_init(RHCT, &table)

static void rhct_check_node_isa_string(fwts_framework *fw,
	fwts_acpi_rhct_node_header *hdr,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_rhct_node_isa_string *node =
		(fwts_acpi_rhct_node_isa_string *)*data;

	if (sizeof(fwts_acpi_rhct_node_isa_string) + node->isa_length > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeISAStringShort",
			"RHCT isa string node structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_rhct_node_isa_string) + node->isa_length);
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "RHCT Node ISA String Structure:");
	fwts_log_info_simp_int(fw, "  Type:               ", hdr->type);
	fwts_log_info_simp_int(fw, "  Length:             ", hdr->length);
	fwts_log_info_simp_int(fw, "  Revision:           ", hdr->revision);

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "RHCT", "revision", hdr->revision, 1, passed);

	fwts_log_info_simp_int(fw, "  ISA Length:         ", node->isa_length);

	if (node->isa_length != strlen(node->isa) + 1) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"RHCTBadISAStringLength",
			"RHCT isa string should have length %" PRIu16
			", got %zu",
			node->isa_length, strlen(node->isa) + 1);
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "  ISA String: %s", node->isa);
	fwts_log_nl(fw);

done:
	*length -= (hdr->length - sizeof(fwts_acpi_rhct_node_header));
	*data += (hdr->length - sizeof(fwts_acpi_rhct_node_header));
}

static void rhct_check_node_cmo(fwts_framework *fw,
	fwts_acpi_rhct_node_header *hdr,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_rhct_node_cmo *node =
		(fwts_acpi_rhct_node_cmo *)*data;

	if (sizeof(fwts_acpi_rhct_node_cmo) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeCMOShort",
			"RHCT cmo node structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_rhct_node_cmo));
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "RHCT Node CMO:");
	fwts_log_info_simp_int(fw, "  Type:               ", hdr->type);
	fwts_log_info_simp_int(fw, "  Length:             ", hdr->length);
	fwts_log_info_simp_int(fw, "  Revision:           ", hdr->revision);

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "RHCT", "revision", hdr->revision, 1, passed);

	fwts_log_info_simp_int(fw, "  Reserved:           ", node->reserved);

	if (node->reserved) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeCMOReserved",
			"RHCT cmo node structure reserved field should be zero, got "
			"0x%" PRIx8, node->reserved);
		*passed = false;
	}

	fwts_log_info_simp_int(fw, "  CBOM block size:    ", node->cbom_size);
	fwts_log_info_simp_int(fw, "  CBOP block size:    ", node->cbop_size);
	fwts_log_info_simp_int(fw, "  CBOZ block size:    ", node->cboz_size);
	fwts_log_nl(fw);

done:
	*length -= (hdr->length - sizeof(fwts_acpi_rhct_node_header));
	*data += (hdr->length - sizeof(fwts_acpi_rhct_node_header));
}

static void rhct_check_node_mmu(fwts_framework *fw,
	fwts_acpi_rhct_node_header *hdr,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_rhct_node_mmu *node =
		(fwts_acpi_rhct_node_mmu *)*data;

	if (sizeof(fwts_acpi_rhct_node_mmu) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeMMUShort",
			"RHCT mmu node structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_rhct_node_mmu));
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "RHCT Node MMU:");
	fwts_log_info_simp_int(fw, "  Type:               ", hdr->type);
	fwts_log_info_simp_int(fw, "  Length:             ", hdr->length);
	fwts_log_info_simp_int(fw, "  Revision:           ", hdr->revision);

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "RHCT", "revision", hdr->revision, 1, passed);

	fwts_log_info_simp_int(fw, "  Reserved:           ", node->reserved);

	if (node->reserved) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeMMUReserved",
			"RHCT mmu node structure reserved field should be zero, got "
			"0x%" PRIx8, node->reserved);
		*passed = false;
	}

	fwts_log_info_simp_int(fw, "  MMU Type:           ", node->mmu_type);

	if (node->mmu_type > 2) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeMMUType",
			"RHCT mmu node structure type field should be 0..2, got "
			"0x%" PRIx8, node->mmu_type);
		*passed = false;
	}

	fwts_log_nl(fw);

done:
	*length -= (hdr->length - sizeof(fwts_acpi_rhct_node_header));
	*data += (hdr->length - sizeof(fwts_acpi_rhct_node_header));
}

static void rhct_check_node_hart_info(fwts_framework *fw,
	fwts_acpi_rhct_node_header *hdr,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_rhct_node_hart_info *node =
		(fwts_acpi_rhct_node_hart_info *)*data;

	if (sizeof(fwts_acpi_rhct_node_hart_info) + 4 * node->num_offsets > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTNodeHartInfoShort",
			"RHCT hart info node structure too short, got "
			"%zu bytes, expecting %zu bytes", *length,
			sizeof(fwts_acpi_rhct_node_hart_info) + 4 * node->num_offsets);
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "RHCT Node Hart Info Structure:");
	fwts_log_info_simp_int(fw, "  Type:               ", hdr->type);
	fwts_log_info_simp_int(fw, "  Length:             ", hdr->length);
	fwts_log_info_simp_int(fw, "  Revision:           ", hdr->revision);

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "RHCT", "revision", hdr->revision, 1, passed);

	fwts_log_info_simp_int(fw, "  Number of offsets:  ", node->num_offsets);
	fwts_log_info_simp_int(fw, "  ACPI Processor UID: ", node->uid);
	fwts_log_nl(fw);

done:
	*length -= (hdr->length - sizeof(fwts_acpi_rhct_node_header));
	*data += (hdr->length - sizeof(fwts_acpi_rhct_node_header));
}

/*
 *  See ACPI 6.5+, Section 5.2.36
 */
static int rhct_test1(fwts_framework *fw)
{
	const fwts_acpi_table_rhct *rhct = (const fwts_acpi_table_rhct *)table->data;
	fwts_acpi_rhct_node_header *hdr;
	uint8_t *data = (uint8_t *)table->data;
	bool passed = true;
	ssize_t length = (ssize_t)rhct->header.length;

	if (fwts_checksum(data, length) != 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SPECRHCTChecksum",
			"RHCT Check Sum is incorrect");

	if (rhct->flags & 0xfffe)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RHCTFlagsNonZero",
			"RHCT flags field, bit 1..31 are reserved and "
			"should be zero, but are set as: 0x%" PRIx32 ".\n",
			rhct->flags);

	data += sizeof(fwts_acpi_table_rhct);
	length -= sizeof(fwts_acpi_table_rhct);

	while (length > 0) {
		hdr = (fwts_acpi_rhct_node_header *)data;

		data += sizeof(fwts_acpi_rhct_node_header);
		length -= sizeof(fwts_acpi_rhct_node_header);

		switch (hdr->type) {
		case FWTS_RHCT_NODE_ISA_STRING:
			rhct_check_node_isa_string(fw, hdr, &length, &data, &passed);
			break;
		case FWTS_RHCT_NODE_CMO:
			rhct_check_node_cmo(fw, hdr, &length, &data, &passed);
			break;
		case FWTS_RHCT_NODE_MMU:
			rhct_check_node_mmu(fw, hdr, &length, &data, &passed);
			break;
		case FWTS_RHCT_NODE_HART_INFO:
			rhct_check_node_hart_info(fw, hdr, &length, &data, &passed);
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"RHCTInvalidType",
				"RHCT Node Structure Type 0x%" PRIx16
				" is an invalid type, expecting 0x00..0x02,0xFFFF",
				hdr->type);
			passed = false;
			length = 0;
			break;
		}
	}

	if (passed)
		fwts_passed(fw, "No issues found in RHCT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test rhct_tests[] = {
	{ rhct_test1, "RHCT RISC-V Hart Capabilities Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops rhct_ops = {
	.description = "RHCT RISC-V Hart Capabilities Table test.",
	.init        = RHCT_init,
	.minor_tests = rhct_tests
};

FWTS_REGISTER("rhct", &rhct_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
