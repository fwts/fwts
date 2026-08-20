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

#define	DUMP_SLIC	(0)	/* Disable this, just used it for debugging */

static fwts_acpi_table_info *table;
acpi_table_init(SLIC, &table)

/*
 *  Software Licensing Description Table
 *  OEM Activation 2.0 for Windows Vista Operating Systems
 */
static int slic_test1(fwts_framework *fw)
{
	bool passed = true;
	uint8_t *ptr;
	size_t const slic_min_hdr_size =
		sizeof(fwts_acpi_table_header) +
		sizeof(fwts_acpi_table_slic_header);
	size_t length = slic_min_hdr_size;

	/* Size sanity check #1, got enough table to get initial header */
	if (!fwts_acpi_table_length(fw, "SLIC", table->length, slic_min_hdr_size)) {
		passed = false;
		goto done;
	}
	ptr = (uint8_t *)table->data + sizeof(fwts_acpi_table_header);

	fwts_log_info_verbatim(fw, "Software Licensing Description Table");
	while (length < table->length) {
		fwts_acpi_table_slic_header *hdr =
			(fwts_acpi_table_slic_header *)ptr;
		if (hdr->length < sizeof(fwts_acpi_table_slic_header))
			break;
#if DUMP_SLIC
		fwts_log_info_simp_int(fw, "  Type:                     ", hdr->type);
		fwts_log_info_simp_int(fw, "  Length:                   ", hdr->length);
#endif

		switch (hdr->type) {
		case 0x0000:
			if (hdr->length < sizeof(fwts_acpi_table_slic_key)) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"SLICShortPublicKeyStructure",
					"SLIC Public Key Structure is only %" PRIu32 " bytes and "
					"should be %zu bytes in size",
					hdr->length, sizeof(fwts_acpi_table_slic_key));
			} else {
#if DUMP_SLIC
				fwts_acpi_table_slic_key *key = (fwts_acpi_table_slic_key *)ptr;
#endif
				fwts_log_info(fw, "SLIC Public Key Structure has had minimal check due to proprietary nature of the table");
#if DUMP_SLIC

				fwts_log_info_verbatim(fw, "  SLIC Public Key:\n");
				fwts_log_info_simp_int(fw, "    Key Type:               ", key->key_type);
				fwts_log_info_simp_int(fw, "    Version:                ", key->version);
				fwts_log_info_simp_int(fw, "    Reserved:               ", key->reserved);
				fwts_log_info_simp_int(fw, "    Algorithm:              ", key->algorithm);
				fwts_log_info_verbatim(fw, "    Magic:                  '%c%c%c%c'",
					key->magic[0], key->magic[1], key->magic[2], key->magic[3]);
				fwts_log_info_simp_int(fw, "    Bit Length:             ", key->bit_length);
				fwts_log_info_simp_int(fw, "    Exponent:               ", key->exponent);
				/* For the moment, don't dump the modulus */
#endif
			}
			ptr += hdr->length;
			length += hdr->length;
			break;
		case 0x0001:
			if (hdr->length < sizeof(fwts_acpi_table_slic_marker)) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"SLICShortWindowsMarkerStructure",
					"SLIC Windows Marker Structure is only %" PRIu32 " bytes and "
					"should be %zu bytes in size",
					hdr->length, sizeof(fwts_acpi_table_slic_marker));
			} else {
#if DUMP_SLIC
				fwts_acpi_table_slic_marker *marker = (fwts_acpi_table_slic_marker *)ptr;
#endif
				fwts_log_info(fw, "SLIC Windows Marker Structure has had minimal check due to proprietary nature of the table");
#if DUMP_SLIC
				fwts_log_info_verbatim(fw, "  SLIC Windows Marker:\n");
				fwts_log_info_simp_int(fw, "    Version:                ", marker->version);
				fwts_log_info_verbatim(fw, "    OEM ID:                 '%6.6s'", marker->oem_id);
				fwts_log_info_verbatim(fw, "    OEM Table ID:           '%8.8s'", marker->oem_table_id);
				fwts_log_info_verbatim(fw, "    Windows Flag:           '%8.8s'", marker->windows_flag);
				fwts_log_info_simp_int(fw, "    SLIC Version:           ", marker->slic_version);
				fwts_log_info_verbatim(fw, "    Reserved:               "
					"0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8,
					marker->reserved[0], marker->reserved[1], marker->reserved[2], marker->reserved[3]);
				fwts_log_info_verbatim(fw, "    Reserved:               "
					"0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8,
					marker->reserved[4], marker->reserved[5], marker->reserved[6], marker->reserved[7]);
				fwts_log_info_verbatim(fw, "    Reserved:               "
					"0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8,
					marker->reserved[8], marker->reserved[9], marker->reserved[10], marker->reserved[11]);
				fwts_log_info_verbatim(fw, "    Reserved:               "
					"0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8,
					marker->reserved[12], marker->reserved[13], marker->reserved[14], marker->reserved[15]);
				/* For the moment, don't dump the signature */
#endif
			}
			ptr += hdr->length;
			length += hdr->length;
			break;
		default:
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"SLICInvaldType",
				"SLIC subtable type 0x%8.8" PRIx32 " is invalid, should be "
				"either 0x0000 (Public Key) or 0x0001 (Windows Marker)",
				hdr->type);
			goto done;
		}
	}

done:
	if (passed)
		fwts_passed(fw, "No issues found in SLIC table.");

	return FWTS_OK;
}

static fwts_framework_minor_test slic_tests[] = {
	{ slic_test1, "SLIC Software Licensing Description Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops slic_ops = {
	.description = "SLIC Software Licensing Description Table test.",
	.init        = SLIC_init,
	.minor_tests = slic_tests
};

FWTS_REGISTER("slic", &slic_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
