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
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>

static fwts_acpi_table_info *table;

static int pptt_init(fwts_framework *fw)
{
	int rc;
	rc = acpi_table_generic_init(fw, "PPTT", &table);
	if (fw->flags & FWTS_FLAG_BRSI)
		if (table == NULL || table->length == 0)
			return FWTS_OK;
	return rc;
}

static void pptt_processor_test(
	fwts_framework *fw,
	const fwts_acpi_table_pptt_processor *entry,
	const uint8_t rev,
	bool *passed)
{
	fwts_log_info_verbatim(fw, "  Processor hierarchy node structure (Type 0):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Length:                         ", entry->header.length);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved);
	fwts_log_info_simp_int(fw, "    Flags:                          ", entry->flags);
	fwts_log_info_simp_int(fw, "    Parent:                         ", entry->parent);
	fwts_log_info_simp_int(fw, "    ACPI Processor ID:              ", entry->acpi_processor_id);
	fwts_log_info_simp_int(fw, "    Number of Private Resources:    ", entry->number_priv_resources);

	if ((entry->header.length - sizeof(fwts_acpi_table_pptt_processor)) / 4 == entry->number_priv_resources) {
		uint32_t i;

		for (i = 0; i < entry->number_priv_resources; i++)
			fwts_log_info_verbatim(fw, "    Private Resources[%d]:           0x%8.8" PRIx32, i, entry->private_resource[i]);
	} else {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"PPTTBadProcessorLength",
			"PPTT Processor length does not match "
			"0x%8.8" PRIx32 " private resources",
			entry->number_priv_resources);
	}

	fwts_acpi_reserved_zero("PPTT", "Reserved", entry->reserved, passed);

	if (rev == 1)
		fwts_acpi_reserved_bits("PPTT", "Flags", entry->flags, 2, 31, passed);
	else
		fwts_acpi_reserved_bits("PPTT", "Flags", entry->flags, 5, 31, passed);

}

static void pptt_cache_test(
	fwts_framework *fw,
	const fwts_acpi_table_pptt_cache *entry,
	const uint8_t rev,
	bool *passed)
{

	fwts_log_info_verbatim(fw, "  Cache Type Structure (Type 1):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Length:                         ", entry->header.length);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved);
	fwts_log_info_simp_int(fw, "    Flags:                          ", entry->flags);
	fwts_log_info_simp_int(fw, "    Next Level of Cache:            ", entry->next_level_cache);
	fwts_log_info_simp_int(fw, "    Size:                           ", entry->size);
	fwts_log_info_simp_int(fw, "    Number of sets:                 ", entry->number_sets);
	fwts_log_info_simp_int(fw, "    Associativity:                  ", entry->associativity);
	fwts_log_info_simp_int(fw, "    Attributes:                     ", entry->attributes);
	fwts_log_info_simp_int(fw, "    Line size:                      ", entry->line_size);
	if (rev >= 3)
		fwts_log_info_simp_int(fw, "    Cache ID:                       ", entry->cache_id);

	fwts_acpi_reserved_zero("PPTT", "Reserved", entry->reserved, passed);

	if (rev == 1 || rev == 2)
		fwts_acpi_reserved_bits("PPTT", "Flags", entry->flags, 7, 31, passed);
	else
		fwts_acpi_reserved_bits("PPTT", "Flags", entry->flags, 8, 31, passed);

	fwts_acpi_reserved_bits("PPTT", "Attributes", entry->attributes, 5, 7, passed);
}

static void pptt_id_test(
	fwts_framework *fw,
	const fwts_acpi_table_pptt_id *entry,
	bool *passed)
{
	char vendor_id[5];

	memcpy(vendor_id, &entry->vendor_id, sizeof(uint32_t));
	vendor_id[4] = 0;

	fwts_log_info_verbatim(fw, "  ID structure (Type 2):");
	fwts_log_info_simp_int(fw, "    Type:                           ", entry->header.type);
	fwts_log_info_simp_int(fw, "    Length:                         ", entry->header.length);
	fwts_log_info_simp_int(fw, "    Reserved:                       ", entry->reserved);
	fwts_log_info_verbatim(fw, "    VENDOR_ID:                      %4.4s", vendor_id);
	fwts_log_info_simp_int(fw, "    LEVEL_1_ID:                     ", entry->level1_id);
	fwts_log_info_simp_int(fw, "    LEVEL_2_ID:                     ", entry->level2_id);
	fwts_log_info_simp_int(fw, "    MAJOR_REV:                      ", entry->major_rev);
	fwts_log_info_simp_int(fw, "    MINOR_REV:                      ", entry->minor_rev);
	fwts_log_info_simp_int(fw, "    SPIN_REV:                       ", entry->spin_rev);

	fwts_acpi_reserved_zero("PPTT", "Reserved", entry->reserved, passed);
}

static int pptt_test1(fwts_framework *fw)
{
	fwts_acpi_table_pptt_header *entry;
	fwts_acpi_table_pptt *pptt;
	bool passed = true;
	uint32_t offset;

	if (table == NULL || table->length == 0) {
		if (fw->flags & FWTS_FLAG_BRSI) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "RISCVNoImpPPTT",
				"The Processor Properties Table (PPTT) MUST be implemented "
				"on RISC-V BRS systems, even on systems with simple hart topology "
				"(per ACPI_030).");
			return FWTS_ERROR;
		}
		fwts_skipped(fw, "ACPI PPTT table does not exist, skipping test");
		return FWTS_SKIP;
	}

	fwts_log_info_verbatim(fw, "PPTT Processor Properties Topology Table:");
	pptt = (fwts_acpi_table_pptt *) table->data;

	entry = (fwts_acpi_table_pptt_header *) (table->data + sizeof(fwts_acpi_table_pptt));
	offset = sizeof(fwts_acpi_table_pptt);
	while (offset < table->length) {
		uint32_t type_length;

		if (fwts_acpi_structure_length_zero(fw, "PPTT", entry->length, offset)) {
			passed = false;
			break;
		}

		if (entry->type == FWTS_PPTT_PROCESSOR) {
			pptt_processor_test(fw, (fwts_acpi_table_pptt_processor *) entry, pptt->header.revision, &passed);
			type_length = sizeof(fwts_acpi_table_pptt_processor) +
				      ((fwts_acpi_table_pptt_processor *) entry)->number_priv_resources * 4;
		} else if (entry->type == FWTS_PPTT_CACHE) {
			pptt_cache_test(fw, (fwts_acpi_table_pptt_cache *) entry, pptt->header.revision, &passed);
			type_length = sizeof(fwts_acpi_table_pptt_cache);
			if (pptt->header.revision < 3)
				type_length -= sizeof(((fwts_acpi_table_pptt_cache *) entry)->cache_id);
		} else if (entry->type == FWTS_PPTT_ID) {
			fwts_log_warning(fw, "PPTT type 2 is deprecated since ACPI 6.3 Errata A.");
			pptt_id_test(fw, (fwts_acpi_table_pptt_id *) entry, &passed);
			type_length = sizeof(fwts_acpi_table_pptt_id);
		} else {
			fwts_acpi_reserved_type(fw, "PPTT", entry->type, 0, FWTS_PPTT_RESERVED, &passed);
			break;
		}

		if (!fwts_acpi_structure_length(fw, "PPTT", entry->type, entry->length, type_length)) {
			passed = false;
			break;
		}

		offset += entry->length;
		if (fwts_acpi_structure_range(fw, "PPTT", table->length, offset)) {
			passed = false;
			break;
		}

		entry = (fwts_acpi_table_pptt_header *) (table->data + offset);
		fwts_log_nl(fw);
	}

	if (passed)
		fwts_passed(fw, "No issues found in PPTT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test pptt_tests[] = {
	{ pptt_test1, "Validate PPTT table." },
	{ NULL, NULL }
};

static fwts_framework_ops pptt_ops = {
	.description = "PPTT Processor Properties Topology Table test.",
	.init        = pptt_init,
	.minor_tests = pptt_tests
};

FWTS_REGISTER("pptt", &pptt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI | FWTS_FLAG_SBBR | FWTS_FLAG_BRSI)

#endif
