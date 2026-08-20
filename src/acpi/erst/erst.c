/*
 * Copyright (C) 2015-2026 Canonical
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
acpi_table_init(ERST, &table)

/*
 *  ERST Extended System Description Table
 */
static int erst_test1(fwts_framework *fw)
{
	fwts_acpi_table_erst *erst = (fwts_acpi_table_erst*)table->data;
	bool passed = true;
	uint64_t total_length;
	uint32_t i;

	fwts_log_info_verbatim(fw, "ERST Error Record Serialization Table :");
	fwts_log_info_simp_int(fw, "  Serialization Hdr. Size:  ", erst->serialization_header_size);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", erst->reserved);
	fwts_log_info_simp_int(fw, "  Instruction Entry Count:  ", erst->instruction_entry_count);

	fwts_acpi_reserved_zero("ERST", "Reserved", erst->reserved, &passed);

	if (erst->serialization_header_size > table->length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ERSTHeaderSizeTooLong",
			"ERST Serialization Header size %" PRIu32 " is greater "
			"than the entire table length of %zu",
			erst->serialization_header_size,
			table->length);
		goto done;
	}
	total_length = ((uint64_t)sizeof(fwts_acpi_serialization_instruction_entries) *
		erst->instruction_entry_count) + sizeof(fwts_acpi_table_erst);
	if (total_length > table->length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ERSTTooManyEntries",
			"ERST size of %" PRIu64 " based on %" PRIu32
			" Serialization Instruction Entries of %zu "
			"bytes is longer the entire table length of %zu",
			total_length, erst->instruction_entry_count,
			sizeof(fwts_acpi_serialization_instruction_entries),
			table->length);
		goto done;
	}
	if (passed)
		fwts_log_info(fw, "ERST header looks sane.");

	for (i = 0; i < erst->instruction_entry_count; i++) {
		fwts_acpi_serialization_instruction_entries *entry = &erst->entries[i];
		bool entry_passed = true;

#if defined(ERST_DEBUG_ENTRY)
		/*  Disable this for now, it causes the test to be too verbose */
		fwts_log_info_verbatim(fw, "ERST Serialization Instruction Entry %" PRIu32 ":", i);
		fwts_log_info_simp_int(fw, "  Serialization Action:     ", entry->serialization_action);
		fwts_log_info_simp_int(fw, "  Instruction:              ", entry->instruction);
		fwts_log_info_simp_int(fw, "  Flags:                    ", entry->flags);
		fwts_log_info_simp_int(fw, "  Reserved:                 ", entry->reserved);
		fwts_log_info_simp_int(fw, "  Value:                    ", entry->value);
		fwts_log_info_simp_int(fw, "  Mask:                     ", entry->mask);
		fwts_log_info_simp_int(fw, "  Address Space ID:         ", entry->register_region.address_space_id);
		fwts_log_info_simp_int(fw, "  Register Bit Width        ", entry->register_region.register_bit_width);
		fwts_log_info_simp_int(fw, "  Register Bit Offset       ", entry->register_region.register_bit_offset);
		fwts_log_info_simp_int(fw, "  Access Size               ", entry->register_region.access_width);
		fwts_log_info_simp_int(fw, "  Address                   ", entry->register_region.address);

#endif

		switch (entry->serialization_action) {
		case 0x00 ... 0x10:
			/* Allow for reserved too */
			break;
		default:
			entry_passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ERSTIInvalidAction",
				"ERST Serialization Action 0x%" PRIx8
				" is an invalid value, values allowed are "
				"0x00..0x10",
				entry->serialization_action);
			break;
		}
		switch (entry->instruction) {
		case 0x00 ... 0x01:
		case 0x04 ... 0x12:
			break;
		case 0x02 ... 0x03:
			/*
			 *  "For WRITE_REGISTER and WRITE_REGISTER_VALUE instructions,
			 *  this flag indicates that bits within the register that
			 *  are not being written must be preserved rather than destroyed.
			 *  For READ_REGISTER instructions, this flag is ignored."
			 */
			if (entry->flags > 1) {
				entry_passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"ERSTIInvalidFlag",
					"ERST Serialization Flag 0x%" PRIx8
					" is an invalid value, values allowed are "
					"0x00..0x01 for the WRITE_REGISTER and "
					"WRITE_REGISTER_VALUE instructions",
					entry->flags);
			}
			break;
		default:
			entry_passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ERSTIInvalidInstruction",
				"ERST Serialization Instruction 0x%" PRIx8
				" is an invalid value, values allowed are "
				"0x00..0x12",
				entry->instruction);
			break;
		}

		/* 0 = Undefined, 1 = byte ... 4 = QWord */
		if (entry->register_region.access_width > 4) {
			entry_passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ERSTIInvalidGasAccessSize",
				"ERST Serialization Generic Address Access Size 0x%" PRIx8
				" is invalid, should be 0x00 .. 0x04",
				entry->register_region.access_width);
		}
		if (entry_passed)
			fwts_log_info(fw, "ERST Serialization Entry 0x%" PRIx32 " looks sane.", i);

		passed &= entry_passed;
	}

done: 	if (passed)
		fwts_passed(fw, "No issues found in ERST table.");

	return FWTS_OK;
}

static fwts_framework_minor_test erst_tests[] = {
	{ erst_test1, "ERST Error Record Serialization Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops erst_ops = {
	.description = "ERST Error Record Serialization Table test.",
	.init        = ERST_init,
	.minor_tests = erst_tests
};

FWTS_REGISTER("erst", &erst_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
