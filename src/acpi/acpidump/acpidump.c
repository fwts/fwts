/*
 * Copyright (C) 2010-2025 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <inttypes.h>
#include <unistd.h>

static void acpi_dump_raw_table(
	fwts_framework *fw,
	const fwts_acpi_table_info *table)
{
	const uint8_t *data = (uint8_t *)table->data;
	const size_t length = table->length;
        size_t n;

	fwts_log_nl(fw);

        for (n = 0; n < length; n += 16) {
                int left = length - n;
		char buffer[128];
		fwts_dump_raw_data(buffer, sizeof(buffer), data + n, n, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw, "%s", buffer);
        }
}

/*
 *  Dump ACPI header in a form that matches IASL's header dump format
 */
static void acpidump_hdr(
	fwts_framework *fw,
	const fwts_acpi_table_header *hdr,
	const size_t length)
{
	if (length < sizeof(fwts_acpi_table_header))
		return;
	fwts_log_info_verbatim(fw, "[000h 0000   4]                    Signature : \"%4.4s\"",
		hdr->signature);
	fwts_log_info_verbatim(fw, "[004h 0004   4]                 Table Length : %8.8" PRIx32,
		hdr->length);
	fwts_log_info_verbatim(fw, "[008h 0008   1]                     Revision : %2.2" PRIx8,
		hdr->revision);
	fwts_log_info_verbatim(fw, "[009h 0009   1]                     Checksum : %2.2" PRIx8,
		hdr->checksum);
	fwts_log_info_verbatim(fw, "[00Ah 0010   6]                       Oem ID : \"%6.6s\"",
		hdr->oem_id);
	fwts_log_info_verbatim(fw, "[010h 0016   8]                 Oem Table ID : \"%8.8s\"",
		hdr->oem_tbl_id);
	fwts_log_info_verbatim(fw, "[018h 0024   4]                 Oem Revision : %8.8" PRIx32,
		hdr->oem_revision);
	fwts_log_info_verbatim(fw, "[01Ch 0028   4]              Asl Compiler ID : \"%4.4s\"",
		hdr->creator_id);
	fwts_log_info_verbatim(fw, "[020h 0032   4]        Asl Compiler Revision : %8.8" PRIx32,
		hdr->creator_revision);
}

/*
 *  Alas, RSDP is a special case
 */
static void acpidump_rsdp(
	fwts_framework *fw,
	const fwts_acpi_table_info *table)
{
	fwts_acpi_table_rsdp *rsdp = (fwts_acpi_table_rsdp *)table->data;

	if (table->length < sizeof(fwts_acpi_table_rsdp))
		return;

	fwts_log_info_verbatim(fw, "[000h 0000   8]                    Signature : \"%8.8s\"",
		rsdp->signature);
	fwts_log_info_verbatim(fw, "[008h 0008   1]                     Checksum : %1.1" PRIx8,
		rsdp->checksum);
	fwts_log_info_verbatim(fw, "[009h 0009   6]                       Oem ID : \"%6.6s\"",
		rsdp->oem_id);
	fwts_log_info_verbatim(fw, "[00fh 0015   1]                     Revision : %2.2" PRIx8,
		rsdp->revision);
	fwts_log_info_verbatim(fw, "[010h 0016   4]                 RSDT Address : %8.8" PRIx32,
		rsdp->rsdt_address);
	fwts_log_info_verbatim(fw, "[014h 0020   4]                 Table Length : %8.8" PRIx32,
		rsdp->length);
	fwts_log_info_verbatim(fw, "[018h 0024   8]                 XSDT Address : %16.16" PRIx64,
		rsdp->xsdt_address);
	fwts_log_info_verbatim(fw, "[020h 0032   1]            Extended Checksum : %2.2" PRIx8,
		rsdp->extended_checksum);
	fwts_log_info_verbatim(fw, "[021h 0033   3]                     Reserved : %2.2" PRIx8 " %2.2" PRIx8 " %2.2" PRIx8,
		rsdp->reserved[0], rsdp->reserved[1], rsdp->reserved[2]);
}

static int acpidump_test1(fwts_framework *fw)
{
	int i;

	fwts_infoonly(fw);
	if (fwts_iasl_init(fw) != FWTS_OK) {
		fwts_aborted(fw, "Failure to initialise iasl, aborting.");
		return FWTS_ERROR;
	}

	for (i = 0; i < ACPI_MAX_TABLES; i++) {
		fwts_acpi_table_info *table;
		fwts_list *output;
		char *provenance;

		if (fwts_acpi_get_table(fw, i, &table) != FWTS_OK)
			break;
		if (table == NULL)
			break;

		switch (table->provenance) {
		case FWTS_ACPI_TABLE_FROM_FILE:
			provenance = " (loaded from file)";
			break;
		case FWTS_ACPI_TABLE_FROM_FIXUP:
			provenance = " (generated by fwts)";
			break;
		default:
			provenance = "";
			break;
		}

		fwts_log_info_verbatim(fw, "%s @ %lx (%zd bytes)%s",
			table->name, (unsigned long)table->addr, table->length, provenance);
		fwts_log_info_verbatim(fw, "----");

		if (!strcmp(table->name, "RSDP")) {
			/* RSDP is a special case */

			acpidump_rsdp(fw, table);
		} else if (table->has_aml) {
			/* Disassembling the AML bloats the output, so ignore */

			uint8_t *data = (uint8_t *)table->data;
			fwts_acpi_table_header hdr;

			fwts_acpi_table_get_header(&hdr, data);
			acpidump_hdr(fw, &hdr, table->length);
			fwts_log_info_verbatim(fw, "Contains AML Object Code.");
		} else if (fwts_iasl_disassemble(fw, table, true, &output) != FWTS_OK) {
			/* Cannot find, assume standard table header */

			uint8_t *data = (uint8_t *)table->data;
			fwts_acpi_table_header hdr;

			fwts_acpi_table_get_header(&hdr, data);
			acpidump_hdr(fw, &hdr, table->length);
			acpi_dump_raw_table(fw, table);
		} else {
			/* Successfully disassembled, so parse */
			fwts_list_link *line;

			bool skip = false;
			bool unknown = false;

			fwts_list_foreach(line, output) {
				char *text = fwts_text_list_text(line);
				bool ignore = (strstr(text, "Raw Table Data:") != NULL);

				/* We don't want to emit this line */
				if (strstr(text, "Unknown ACPI table signature") != NULL) {
					unknown = true;
					ignore = true;
				}
				/* and we want to ignore text in comments */
				if (!strncmp(text, "/*", 2))
					skip = true;
				if (!(ignore | skip | unknown))
					fwts_log_info_verbatim(fw, "%s", fwts_text_list_text(line));
				if (!strncmp(text, " */", 3))
					skip = false;
			}
			fwts_text_list_free(output);
			if (unknown)
				acpi_dump_raw_table(fw, table);
		}
		fwts_log_nl(fw);
	}
	fwts_iasl_deinit();

	/* Some systems don't have any ACPI tables */
	if (!i)
		fwts_log_info(fw, "Cannot find any ACPI tables.");

	return FWTS_OK;
}

static fwts_framework_minor_test acpidump_tests[] = {
	{ acpidump_test1, "Dump ACPI tables." },
	{ NULL, NULL }
};

static fwts_framework_ops acpidump_ops = {
	.description = "Dump ACPI tables.",
	.minor_tests = acpidump_tests
};

FWTS_REGISTER("acpidump", &acpidump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)

#endif
