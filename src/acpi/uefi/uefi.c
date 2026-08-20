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
#include <ctype.h>

static fwts_acpi_table_info *table;
acpi_table_init(UEFI, &table)

/*
 *  UEFI ACPI DATA Table
 *  See UEFI specification Appendix O.
 */
static int uefi_test1(fwts_framework *fw)
{
	fwts_acpi_table_uefi *uefi = (fwts_acpi_table_uefi *)table->data;
	bool passed = true;
	uint32_t i;
	char guid[37];

	/*
	 * GUID for SMM Communication ACPI Table
	 * {0xc68ed8e2, 0x9dc6, 0x4cbd, 0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32}
	 */
	static const uint8_t guid_smm[16] = { 0xe2, 0xd8, 0x8e, 0xc6, 0xc6, 0x9d, 0xbd, 0x4c,
						0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32 };

	/* Enough length for the uefi table? */
	if (!fwts_acpi_table_length(fw, "UEFI", table->length, sizeof(fwts_acpi_table_uefi))) {
		passed = false;
		goto done;
	}

	fwts_guid_buf_to_str(uefi->uuid, guid, sizeof(guid));

	fwts_log_info_verbatim(fw, "UEFI ACPI Data Table:");
	fwts_log_info_verbatim(fw, "  Identifier: %s", guid);
	fwts_log_info_simp_int(fw, "  DataOffset: ", uefi->dataoffset);

	/* Sanity check the dataoffset */
	if (uefi->dataoffset > table->length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIDataOffset",
			"Invalid UEFI DataOffset, exceed the whole table length "
			"%zu bytes, instead got %" PRIu16 " offset bytes"
			, table->length, uefi->dataoffset);
	}

	/* check the GUID for SMM Communication ACPI table */
	if (memcmp(uefi->uuid, guid_smm, 16) == 0) {
		fwts_acpi_table_uefi_smmcomm *uefi_smmcomm = (fwts_acpi_table_uefi_smmcomm *)table->data;

		/* check the dataoffset for SMM Comm table */
		if (uefi_smmcomm->boot.dataoffset != 54) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIDataOffset",
				"Invalid UEFI DataOffset for SMM Communication table, "
				"DataOffset should be 54, instead got %" PRIu16 " offset bytes"
				, uefi_smmcomm->boot.dataoffset);
		}

		fwts_log_info_simp_int(fw, "  SW SMI Number: ", uefi_smmcomm->sw_smi_number);
		fwts_log_info_simp_int(fw, "  Buffer Ptr Address: ", uefi_smmcomm->buf_ptr_addr);
	} else {
		/* dump the remaining data */
		fwts_log_info_verbatim(fw, "  Data:");
		for (i = 0; i < (table->length - uefi->dataoffset) ; i += 16) {
			int left = table->length - uefi->dataoffset -i;
			char buffer[128];
			fwts_dump_raw_data(buffer,sizeof(buffer), uefi->data + i, i, left > 16 ? 16 : left);
			fwts_log_info_verbatim(fw, "%s", buffer);
		}
	}

done:
	if (passed)
		fwts_passed(fw, "No issues found in UEFI table.");

	return FWTS_OK;
}

static fwts_framework_minor_test uefi_tests[] = {
	{ uefi_test1, "UEFI Data Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops uefi_ops = {
	.description = "UEFI Data Table test.",
	.init        = UEFI_init,
	.minor_tests = uefi_tests
};

FWTS_REGISTER("uefi", &uefi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
