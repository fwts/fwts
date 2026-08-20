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

#if defined(FWTS_HAS_ACPI) && !(FWTS_ARCH_AARCH64)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "fwts_acpi_object_eval.h"

static fwts_acpi_table_info *table;
acpi_table_init(WDAT, &table)

/*
 * ACPI WDAT (Watchdog Action Table)
 *  https://msdn.microsoft.com/en-us/windows/hardware/gg463320.aspx
 */
static int wdat_test1(fwts_framework *fw)
{
	const fwts_acpi_table_wdat *wdat =
		(const fwts_acpi_table_wdat *)table->data;
	uint32_t reserved1, reserved2;
	bool passed = true;
	bool entries_passed = true;
	size_t total_length;
	uint32_t i;

	if (wdat->header.length > (uint32_t)table->length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"WDATBadLength",
			"WDAT header reports that the table is %" PRIu32
			" bytes long, however this is longer than the ACPI "
			"table size of %zu bytes.",
			wdat->header.length,
			table->length);
		goto done;
	}

	reserved1 = (uint32_t) wdat->reserved1[0] + ((uint32_t) wdat->reserved1[1] << 8) +
		   ((uint32_t) wdat->reserved1[2] << 16);

	reserved2 = (uint32_t) wdat->reserved2[0] + ((uint32_t) wdat->reserved2[1] << 8) +
		   ((uint32_t) wdat->reserved2[2] << 16);

	/* Now we have got some sane data, dump the WDAT */
	fwts_log_info_verbatim(fw, "WDAT Microsoft Watchdog Action Table:");
	fwts_log_info_simp_int(fw, "  Watchdog Header Length:   ", wdat->watchdog_header_length);
	fwts_log_info_simp_int(fw, "  PCI Segment:              ", wdat->pci_segment);
	fwts_log_info_simp_int(fw, "  PCI Bus Number:           ", wdat->pci_bus_number);
	fwts_log_info_simp_int(fw, "  PCI Device Number:        ", wdat->pci_device_number);
	fwts_log_info_simp_int(fw, "  PCI Function Number:      ", wdat->pci_function_number);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", reserved1);
	fwts_acpi_reserved_zero("WDAT", "Reserved1", reserved1, &passed);
	fwts_log_info_simp_int(fw, "  Timer Period:             ", wdat->timer_period);
	fwts_log_info_simp_int(fw, "  Maximum Count:            ", wdat->maximum_count);
	fwts_log_info_simp_int(fw, "  Minimum Count:            ", wdat->minimum_count);
	fwts_log_info_simp_int(fw, "  Watchdog Flags:           ", wdat->watchdog_flags);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", reserved2);
	fwts_acpi_reserved_zero("WDAT", "Reserved2", reserved2, &passed);
	fwts_log_info_simp_int(fw, "  Watchdog Entries          ", wdat->number_of_entries);

	if (wdat->minimum_count > wdat->maximum_count) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"WDATMinGreaterThanMax",
			"WDAT Minimum Count is 0x%" PRIx32 " and is greater "
			"than the Maximum Count of 0x%" PRIx32,
			wdat->minimum_count, wdat->maximum_count);
	}

	/*
	 * Check if bits 6..1 are set, just bits 0 and 7 are used
	 * so check if the undefined bits are set. The specification
	 * does not state what these bits should be set as, but
	 * this does sanity check that somebody has not set these
	 * bits accidentally.  This is a LOW issue.
	 */
	fwts_acpi_reserved_bits("WDAT", "Watchdog Flags", wdat->watchdog_flags, 1, 6, &passed);

	total_length = sizeof(fwts_acpi_table_wdat) +
		(wdat->number_of_entries * sizeof(fwts_acpi_table_wdat_instr_entries));
	if (total_length > wdat->header.length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"WDATBadLength",
			"WDAT header reports that the table has %" PRIu32
			" watchdog instruction entries making the table "
			"%zu bytes long, however, the WDAT table is only %" PRIu32
			" bytes in size.",
			wdat->number_of_entries, total_length,
			wdat->header.length);
		goto done;
	}
	for (i = 0; i < wdat->number_of_entries; i++) {
		const fwts_acpi_table_wdat_instr_entries *entry = &wdat->entries[i];

		fwts_log_info_verbatim(fw, "Watchdog Instruction Entry %" PRIu32, i + 1);
		fwts_log_info_simp_int(fw, "  Watchdog Action:          ", entry->watchdog_action);
		fwts_log_info_simp_int(fw, "  Instruction Flags:        ", entry->instruction_flags);
		fwts_log_info_simp_int(fw, "  Reserved:                 ", entry->reserved);
		fwts_acpi_reserved_zero("WDAT", "Watchdog Entry Reserved", entry->reserved, &passed);
		fwts_log_info_simp_int(fw, "    Address Space ID:       ", entry->register_region.address_space_id);
		fwts_log_info_simp_int(fw, "    Register Bit Width      ", entry->register_region.register_bit_width);
		fwts_log_info_simp_int(fw, "    Register Bit Offset     ", entry->register_region.register_bit_offset);
		fwts_log_info_simp_int(fw, "    Access Size             ", entry->register_region.access_width);
		fwts_log_info_simp_int(fw, "    Address                 ", entry->register_region.address);
		fwts_log_info_simp_int(fw, "  Value:                    ", entry->value);
		fwts_log_info_simp_int(fw, "  Mask:                     ", entry->mask);

		switch (entry->watchdog_action) {
		case 0x01:	/* RESET */
		case 0x04:	/* QUERY_CURRENT_COUNTDOWN_PERIOD */
		case 0x05:	/* QUERY_COUNTDOWN_PERIOD */
		case 0x06:	/* SET_COUNTDOWN_PERIOD */
		case 0x08:	/* QUERY_RUNNING_STATE */
		case 0x09:	/* SET_RUNNING_STATE */
		case 0x0a:	/* QUERY_STOPPED_STATE */
		case 0x0b:	/* SET_STOPPED_STATE */
		case 0x10:	/* QUERY_REBOOT */
		case 0x11:	/* SET_REBOOT */
		case 0x12:	/* QUERY_SHUTDOWN */
		case 0x13:	/* SET_SHUTDOWN */
		case 0x20:	/* QUERY_WATCHDOG_STATUS */
		case 0x21:	/* SET_WATCHDOG_STATUS */
			break;
		default:
			entries_passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"WDATWatchdogActionInvalid",
				"WDAT Watchdog Instruction Entry %" PRIu32
				" Watchdog Action field is 0x%" PRIx8
				" and should be one of 0x00, 0x04, 0x05, 0x06, "
				"0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13, "
				"0x20 or 0x21",
				i + 1, entry->watchdog_action);
			break;
		}
		/*
		 * Instruction flags can be 0x00, 0x01, 0x02, 0x03 with
		 * bit 7 clear or set, so bits 7, 1, 0 are valid, (which
		 * is 0x80 | 0x02 | 0x01)
		 */
		if (entry->instruction_flags & ~0x83) {
			entries_passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"WDATInstructionFlagsInvalid",
				"WDAT Watchdog Instruction Entry %" PRIu32
				" Instruction Flags field is 0x%" PRIx8
				" and should be one of 0x00, 0x01, 0x02, 0x03 or "
				" 0x80, 0x81, 0x82, 0x83",
				i + 1, entry->instruction_flags);
		}
	}
	if (entries_passed)
		fwts_passed(fw, "All %" PRIu32 " WDAT Watchdog Instruction Entries look sane.",
			wdat->number_of_entries);

done:
	passed &= entries_passed;
	if (passed)
		fwts_passed(fw, "No issues found in WDAT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test wdat_tests[] = {
	{ wdat_test1, "WDAT Microsoft Hardware Watchdog Action Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops wdat_ops = {
	.description = "WDAT Microsoft Hardware Watchdog Action Table test.",
	.init        = WDAT_init,
	.minor_tests = wdat_tests
};

FWTS_REGISTER("wdat", &wdat_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
