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
acpi_table_init(GTDT, &table)

/*
 *  GTDT  Generic Timer Description Table
 */
static int gtdt_test1(fwts_framework *fw)
{
	bool passed = true;
	uint8_t *ptr, *end_ptr;
	uint32_t i = 0, n;
	const fwts_acpi_table_gtdt *gtdt = (const fwts_acpi_table_gtdt *)table->data;

	if (gtdt->cnt_control_base_phys_addr == 0) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"GTDTInvalidBaseAddr",
			"The 64-bit physical address at which the "
			"Counter Control block should not be Zero. "
			"If not provided, this field must be 0xFFFFFFFFFFFFFFFF");
	}

	if (gtdt->cnt_read_base_phys_addr == 0) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"GTDTInvalidBaseAddr",
			"The 64-bit physical address at which the "
			"Counter Read block Read block should not be Zero. "
			"If not provided, this field must be 0xFFFFFFFFFFFFFFFF");
	}

	fwts_acpi_reserved_bits("GTDT", "Flags", gtdt->virtual_timer_flags, 3, 31, &passed);

	ptr = (uint8_t *)table->data + gtdt->platform_timer_offset;
	n = gtdt->platform_timer_count;
	end_ptr = (uint8_t *)table->data + table->length;

	while ((i < n) && (ptr < end_ptr)) {
		uint32_t len, j;
		fwts_acpi_table_gtdt_block *block;
		fwts_acpi_table_gtdt_block_timer *block_timer;
		fwts_acpi_table_gtdt_watchdog *watchdog;
		char field[80];

		switch (*ptr) {
		case 0x00:
			/* GT Block Structure */
			block = (fwts_acpi_table_gtdt_block *)ptr;
			if (ptr + 20 > end_ptr) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTShortBlock",
					"GTDT block is too short");
				goto done;
			}
			if (block->length < 20) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidBlockLength",
					"GTDT block %" PRIu32 " length has in "
					"invalid length: %" PRIu32 " bytes",
					i, block->length);
				goto done;
			}
			if (block->physical_address == 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTBlockInvalidBaseAddr",
					"The 64-bit physical address at which the "
					"GT CntCTLBase Block shouldn't be zero.");
			}
			if (block->reserved) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidBlockReserved",
					"GTDT block %" PRIu32 " reserved is "
					"non-zero, got 0x%" PRIx8,
					i, block->reserved);
			}
			if (block->block_timer_count > 8) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidBlockTimerCount",
					"GTDT block %" PRIu32 " timer count "
					"is too large, %" PRIu32 ", must be <= 8",
					i, block->block_timer_count);
				break;
			}
			len = (block->block_timer_count * 40) + 20;
			if (len != block->length) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidTimerCountOrLength",
					"GTDT block %" PRIu32 " timer count %"
					PRIu32 " and block length %" PRIu32 ", "
					"expected length of %" PRIu32,
					i, block->block_timer_count,
					block->length, len);
				/* length may be inconsistent, don't trust it so stop here */
				goto done;
			}
			block_timer = &block->block_timers[0];

			for (j = 0; j < block->block_timer_count; j++) {
				if (((uint8_t *)block_timer + sizeof(*block_timer)) > end_ptr)
					break;
				if (block_timer->frame_number > 7) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"GTDTInvalidGTFrameNumber",
						"GTDT block frame number is %" PRIu8
						", expecting 0..7",
						block_timer->frame_number);
				}
				if (block_timer->reserved[0] |
				    block_timer->reserved[1] |
				    block_timer->reserved[2]) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"GTDTBlockTimerReservedNotZero",
						"GTDT block %" PRIu32 " timer reserved "
						"space is not zero, got 0x"
						"%" PRIx8 "%" PRIx8 "%" PRIx8
						" instead", i,
						block_timer->reserved[0],
						block_timer->reserved[1],
						block_timer->reserved[2]);
				}
				if (block_timer->cntbase == 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"GTDTGTxInvalidBaseAddr",
						"Physical Address at which the "
						"CntBase block for GTx shouldn't be zero.");
				}
				if (block_timer->cntel0base == 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						"GTDTGTxInvalidBaseAddr",
						"Physical Address at which the "
						"CntEL0Base block for GTx should not be Zero. "
						"If not provided, this field must be 0xFFFFFFFFFFFFFFFF");
				}

				snprintf(field, sizeof(field), "block %" PRIu32 " physical timer flags", i);
				fwts_acpi_reserved_bits("GTDT", field, block_timer->phys_timer_flags, 2, 31, &passed);

				snprintf(field, sizeof(field), "block %" PRIu32 " virtual timer flags", i);
				fwts_acpi_reserved_bits("GTDT", field, block_timer->virt_timer_flags, 2, 31, &passed);

				snprintf(field, sizeof(field), "block %" PRIu32 " common flags", i);
				fwts_acpi_reserved_bits("GTDT", field, block_timer->common_flags, 2, 31, &passed);
			}
			ptr += block->length;
			break;
		case 0x01:
			/* SBSA Generic Watchdog Timer Structure */
			watchdog = (fwts_acpi_table_gtdt_watchdog *)ptr;
			if (ptr + 28 > end_ptr) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTShortWatchDogTimer",
					"GTDT SBSA generic watchdog timer %"
					PRIu32 " is too short", i);
				goto done;
			}
			if (watchdog->length != 28) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidWatchDogTimeLength",
					"GTDT SBSA generic watchdog timer %" PRIu32
					" length has in invalid length: %"
					PRIu32 " bytes", i, watchdog->length);
				goto done;
			}
			if (watchdog->reserved) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidWatchDogReserved",
					"GTDT SBSA generic watchdog timer %" PRIu32
					" reserved is non-zero, got 0x%" PRIx8,
					i, watchdog->reserved);
			}
			if (watchdog->refresh_frame_addr == 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidWatchDogBaseAddr",
					"Physical Address at which the "
					"RefreshFrame block shouldn't be zero.");
			}
			if (watchdog->watchdog_control_frame_addr == 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"GTDTInvalidWatchDogBaseAddr",
					"Physical Address at which the "
					"Watchdog Control Frame block shouldn't be zero.");
			}

			snprintf(field, sizeof(field), "SBSA generic watchdog timer %" PRIu32 " flags", i);
			fwts_acpi_reserved_bits("GTDT", field, watchdog->watchdog_timer_flags, 3, 31, &passed);

			ptr += watchdog->length;
			break;
		default:
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"GTDTInvalidType",
				"GTDT platform timer structure %" PRIu32
				" has an invalid type: 0x%" PRIx8, i, *ptr);
			/* Can't determine field length, so end of parsing */
			goto done;
		}
		i++;
	}

done:
	if (passed)
		fwts_passed(fw, "No issues found in GTDT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test gtdt_tests[] = {
	{ gtdt_test1, "GTDT Generic Timer Description Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops gtdt_ops = {
	.description = "GTDT Generic Timer Description Table test.",
	.init        = GTDT_init,
	.minor_tests = gtdt_tests
};

FWTS_REGISTER("gtdt", &gtdt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI | FWTS_FLAG_SBBR)

#endif
