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

static fwts_acpi_table_info *table;
acpi_table_init(LPIT, &table)

static void lpit_check_type_0(
	fwts_framework *fw,
	uint32_t *length,
	uint8_t **data,
	bool *passed)
{
	fwts_acpi_table_lpit_c_state *lpi = (fwts_acpi_table_lpit_c_state *)*data;

	/* We know table is at least long enough to access type and length.. */
	fwts_log_info_verbatim(fw, "Native C-state based LPI structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", lpi->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", lpi->length);

	if (lpi->length < sizeof(fwts_acpi_table_lpit_c_state)) {
		*passed = false;
		*length = 0;	/* Force loop exit */
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"LPITNativeCStateLpitShort",
			"Native C-state based LPI structure is too short, "
			"got %" PRIu32 " bytes, expected %zu bytes",
			lpi->length,
			sizeof(fwts_acpi_table_lpit_c_state));
		return;
	}

	fwts_log_info_simp_int(fw, "  ID:                       ", lpi->id);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", lpi->reserved);
	fwts_log_info_simp_int(fw, "  Flags:                    ", lpi->flags);
	fwts_log_info_verbatim(fw, "  Entry Trigger:");
	fwts_log_info_simp_int(fw, "    Address Space ID:       ", lpi->entry_trigger.address_space_id);
	fwts_log_info_simp_int(fw, "    Register Bit Width      ", lpi->entry_trigger.register_bit_width);
	fwts_log_info_simp_int(fw, "    Register Bit Offset     ", lpi->entry_trigger.register_bit_offset);
	fwts_log_info_simp_int(fw, "    Access Size             ", lpi->entry_trigger.access_width);
	fwts_log_info_simp_int(fw, "    Address                 ", lpi->entry_trigger.address);
	fwts_log_info_simp_int(fw, "  Residency:                ", lpi->residency);
	fwts_log_info_simp_int(fw, "  Latency:                  ", lpi->latency);

	/* If flags [1] set, then counter is not available */
	if (lpi->flags & 0x2) {
		fwts_log_info_verbatim(fw, "  Residency Counter not available");
	} else {
		fwts_log_info_verbatim(fw, "  Residency Counter:");
		fwts_log_info_simp_int(fw, "    Address Space ID:       ", lpi->residency_counter.address_space_id);
		fwts_log_info_simp_int(fw, "    Register Bit Width      ", lpi->residency_counter.register_bit_width);
		fwts_log_info_simp_int(fw, "    Register Bit Offset     ", lpi->residency_counter.register_bit_offset);
		fwts_log_info_simp_int(fw, "    Access Size             ", lpi->residency_counter.access_width);
		fwts_log_info_simp_int(fw, "    Address                 ", lpi->residency_counter.address);
		fwts_log_info_simp_int(fw, "  Residency Counter Freq:   ", lpi->residency_counter_freq);
	}
	fwts_log_nl(fw);

	fwts_acpi_reserved_zero("LPIT", "Native C-state based LPI structure reserved", lpi->reserved, passed);
	fwts_acpi_reserved_bits("LPIT", "LPI structure flags", lpi->flags, 2, 31, passed);

	/* 2.2.1.2, if FFH, then it is a MSR, check GAS fields */
	if (((lpi->flags & 2) == 0) &&
	    (lpi->residency_counter.address_space_id == FWTS_GAS_ADDR_SPACE_ID_FFH)) {
		if (lpi->residency_counter.register_bit_width != 64) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_LOW,
				"LPITNativeCStateLpitResidencyCounterWidth",
				"Native C-state based LPI structure Residency Structure "
				"Register Bit Width was %" PRIu8 " for a FFH Address "
				"Space (e.g. a MSR), and was expecting 64.",
				lpi->residency_counter.register_bit_width);
		}
		if (lpi->residency_counter.register_bit_offset != 0) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_LOW,
				"LPITNativeCStateLpitResidencyCounterOffset",
				"Native C-state based LPI structure Residency Structure "
				"Register Bit Offset %" PRIu8 " for a FFH Address "
				"Space (e.g. a MSR), and was expecting 0.",
				lpi->residency_counter.register_bit_offset);
		}
		if (lpi->residency_counter.access_width != 0) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_LOW,
				"LPITNativeCStateLpitResidencyCounterAccessSize",
				"Native C-state based LPI structure Residency Structure "
				"Register Access Size %" PRIu8 " for a FFH Address "
				"Space (e.g. a MSR), and was expecting 0.",
				lpi->residency_counter.access_width);
		}
	}
	if (! *passed)
		fwts_log_nl(fw);

	*length -= lpi->length;
	*data += lpi->length;
}

/*
 *  http://www.uefi.org/sites/default/files/resources/ACPI_Low_Power_Idle_Table.pdf
 */
static int lpit_test1(fwts_framework *fw)
{
	uint8_t *data;
	bool passed = true;
	uint32_t length;
	uint16_t uid = 0, init_id = 0;
	fwts_acpi_table_lpit *lpit = (fwts_acpi_table_lpit *)table->data;

	if (table->length < sizeof(fwts_acpi_table_lpit)) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"LPITTooShort",
			"LPIT is only %zu bytes long, "
			"expected at least %zu bytes for the "
			"header structure",
			table->length,
			sizeof(fwts_acpi_table_lpit_c_state));
		goto done;
	}

	length = lpit->header.length - sizeof(fwts_acpi_table_lpit);
	data = (uint8_t *)table->data + sizeof(fwts_acpi_table_lpit);

	/* Got enough data to be able to inspect the initial 2 x 32 bit words.. */
	while (length > 8) {
		fwts_acpi_table_lpit_c_state *lpi = (fwts_acpi_table_lpit_c_state *)data;
		uint32_t *ptr = (uint32_t *)data;
		uint32_t lpi_length = *(ptr + 1);

		/* Stated LPI length must not be longer than what's left in the table */
		if (length < lpi_length) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"LPITTooShort",
				"LPIT LPI structure type is only %" PRIu32
				" bytes long, expected at least %zu bytes",
				length,
				table->length);
			break;
		}

		/* For now, just one type is described in the specification */
		switch (*ptr) {
		case 0x0:
			lpit_check_type_0(fw, &length, &data, &passed);

			/* check lpi->id starts from zero */
			if (init_id == 0 && lpi->id != 0) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"LPITNativeCStateBadUID",
					"Unique ID of Native C-state based LPI  "
					"structure must start from zero");
			} else
				init_id = 1;

			/* check lpi->id increases monotonically */
			if (uid > lpi->id || (lpi->id - uid) > 1) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"LPITNativeCStateBadUID",
					"Unique ID of Native C-state based LPI  "
					"structure must be a zero-based monotonically "
					"increasing value");
			}
			uid = lpi->id;
			break;
		default:
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"LPITInvalidType",
				"LPIT Type 0x8.8%" PRIx32
				" is an invalid type, expecting 0",
				*ptr);
			length = 0;
			break;
		}
	}

done:
	if (passed)
		fwts_passed(fw, "No issues found in LPIT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test lpit_tests[] = {
	{ lpit_test1, "LPIT Low Power Idle Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops lpit_ops = {
	.description = "LPIT Low Power Idle Table test.",
	.init        = LPIT_init,
	.minor_tests = lpit_tests
};

FWTS_REGISTER("lpit", &lpit_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
