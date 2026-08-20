/*
 * Copyright (C) 2010-2026 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>

typedef struct {
	const char *name;
	const uint8_t revision;
} fwts_acpi_table_rev;

static bool acpi_table_check_field(const char *field, const size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (!isascii(field[i]))
			return false;

	return true;
}

static bool acpi_table_check_field_test(
	fwts_framework *fw,
	const char *table_name,
	const char *field_name,
	const char *field,
	const size_t len)
{
	if (!acpi_table_check_field(field, len)) {
		fwts_failed(fw, LOG_LEVEL_LOW, "ACPITableHdrInfo",
			"ACPI Table %s has non-ASCII characters in "
			"header field %s", table_name, field_name);
		return false;
	}
	return true;
}

static int acpi_table_check_test1(fwts_framework *fw)
{
	int i;
	bool checked = false;

	for (i = 0; ; i++) {
		fwts_acpi_table_info *info;
		fwts_acpi_table_header *hdr;
		char name[50];
		bool passed;

		if (fwts_acpi_get_table(fw, i, &info) != FWTS_OK)
			break;
		if (info == NULL)
			continue;

		checked = true;
		/* RSDP and FACS are special cases, skip these */
		if (!strcmp(info->name, "RSDP") ||
		    !strcmp(info->name, "FACS"))
			continue;

		hdr = (fwts_acpi_table_header *)info->data;
		if (acpi_table_check_field(hdr->signature, 4)) {
			snprintf(name, sizeof(name), "%4.4s", hdr->signature);
		} else {
			/* Table name not printable, so identify it by the address */
			snprintf(name, sizeof(name), "at address 0x%" PRIx64, info->addr);
		}

		/*
		 * Tables shouldn't be short, however, they do have at
		 * least 4 bytes with their signature else they would not
		 * have been loaded by this stage.
		 */
		if (hdr->length < sizeof(fwts_acpi_table_header)) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPITableHdrShort",
				"ACPI Table %s is too short, only %" PRIu32 " bytes long. Further "
				"header checks will be omitted.", name, hdr->length);
			continue;
		}
		/* Warn about empty tables */
		if (hdr->length == sizeof(fwts_acpi_table_header)) {
			fwts_warning(fw,
				"ACPI Table %s is empty and just contains a table header. Further "
				"header checks will be omitted.", name);
			continue;
		}

		passed = acpi_table_check_field_test(fw, name, "Signature", hdr->signature, 4) &
			 acpi_table_check_field_test(fw, name, "OEM ID", hdr->oem_id, 6) &
			 acpi_table_check_field_test(fw, name, "OEM Table ID", hdr->oem_tbl_id, 8) &
			 acpi_table_check_field_test(fw, name, "Creator ID", hdr->creator_id, 4);
		if (passed)
			fwts_passed(fw, "Table %s has valid signature and ID strings.", name);

	}
	if (!checked)
		fwts_aborted(fw, "Cannot find any ACPI tables.");

	return FWTS_OK;
}

/* Supported ACPI tables (see Table 5-30 in ACPI spec)
 * Note: OEMx (N/A) and PSDT (deleted) aren't included.
 */
static const fwts_acpi_table_rev acpi_61_rev[] = {
	{"APIC", 4},
	{"BERT", 1},
	{"BGRT", 1},
	{"CPEP", 1},
	{"DSDT", 2},
	{"ECDT", 1},
	{"EINJ", 1},
	{"ERST", 1},
	{"FACP", 6},
	{"FPDT", 1},
	{"GTDT", 2},
	{"HEST", 1},
	{"MSCT", 1},
	{"MPST", 1},
	{"NFIT", 1},
	{"PCCT", 1},
	{"PMTT", 1},
	{"RASF", 1},
	{"RSDT", 1},
	{"SBST", 1},
	{"SLIT", 1},
	{"SRAT", 3},
	{"SSDT", 2},
	{"XSDT", 1},
	{NULL, 0xff}	// end of table
};

static const fwts_acpi_table_rev acpi_62_rev[] = {
	{"APIC", 4},
	{"BERT", 1},
	{"BGRT", 1},
	{"CPEP", 1},
	{"DSDT", 2},
	{"ECDT", 1},
	{"EINJ", 1},
	{"ERST", 1},
	{"FACP", 6},
	{"FPDT", 1},
	{"GTDT", 2},
	{"HEST", 1},
	{"MSCT", 1},
	{"MPST", 1},
	{"NFIT", 1},
	{"PCCT", 2},
	{"PMTT", 1},
	{"PPTT", 1},
	{"RASF", 1},
	{"RSDT", 1},
	{"SBST", 1},
	{"SLIT", 1},
	{"SRAT", 3},
	{"SSDT", 2},
	{"XSDT", 1},
	{NULL, 0xff}	// end of table
};

static const fwts_acpi_table_rev acpi_63_rev[] = {
	{"APIC", 5},
	{"BERT", 1},
	{"BGRT", 1},
	{"CPEP", 1},
	{"DSDT", 2},
	{"ECDT", 1},
	{"EINJ", 1},
	{"ERST", 1},
	{"FACP", 6},
	{"FPDT", 1},
	{"GTDT", 3},
	{"HEST", 1},
	{"MSCT", 1},
	{"MPST", 1},
	{"NFIT", 1},
	{"PCCT", 2},
	{"PMTT", 1},
	{"PPTT", 2},
	{"RASF", 1},
	{"RSDT", 1},
	{"SBST", 1},
	{"SDEV", 1},
	{"SLIT", 1},
	{"SRAT", 3},
	{"SSDT", 2},
	{"XSDT", 1},
	{NULL, 0xff}	// end of table
};

static const fwts_acpi_table_rev acpi_64_rev[] = {
	{"APIC", 5},
	{"BERT", 1},
	{"BGRT", 1},
	{"CPEP", 1},
	{"DSDT", 2},
	{"ECDT", 1},
	{"EINJ", 1},
	{"ERST", 1},
	{"FACP", 6},
	{"FPDT", 1},
	{"GTDT", 3},
	{"HEST", 1},
	{"MSCT", 1},
	{"MPST", 1},
	{"NFIT", 1},
	{"PCCT", 2},
	{"PHAT", 1},
	{"PMTT", 2},
	{"PPTT", 3},
	{"RASF", 1},
	{"RSDT", 1},
	{"SBST", 1},
	{"SDEV", 1},
	{"SLIT", 1},
	{"SRAT", 3},
	{"SSDT", 2},
	{"XSDT", 1},
	{NULL, 0xff}	// end of table
};

static const fwts_acpi_table_rev acpi_65_rev[] = {
	{"APIC", 6},
	{"BERT", 1},
	{"BGRT", 1},
	{"CPEP", 1},
	{"DSDT", 2},
	{"ECDT", 1},
	{"EINJ", 2},
	{"ERST", 1},
	{"FACP", 6},
	{"FPDT", 1},
	{"GTDT", 3},
	{"HEST", 2},
	{"MISC", 1},
	{"MSCT", 1},
	{"MPST", 1},
	{"NFIT", 1},
	{"PCCT", 2},
	{"PHAT", 1},
	{"PMTT", 2},
	{"PPTT", 3},
	{"RASF", 1},
	{"RAS2", 1},
	{"RSDT", 1},
	{"SBST", 1},
	{"SDEV", 1},
	{"SLIT", 1},
	{"SRAT", 3},
	{"SSDT", 2},
	{"XSDT", 1},
	{NULL, 0xff}	// end of table
};

static const fwts_acpi_table_rev acpi_66_rev[] = {
	{"APIC", 7},
	{"BERT", 1},
	{"BGRT", 1},
	{"CPEP", 1},
	{"DSDT", 2},
	{"ECDT", 1},
	{"EINJ", 2},
	{"ERST", 1},
	{"FACP", 6},
	{"FPDT", 1},
	{"GTDT", 3},
	{"HEST", 2},
	{"MISC", 1},
	{"MSCT", 1},
	{"MPST", 1},
	{"NFIT", 1},
	{"PCCT", 2},
	{"PHAT", 1},
	{"PMTT", 2},
	{"PPTT", 3},
	{"RASF", 1},
	{"RAS2", 1},
	{"RHCT", 1},
	{"RSDT", 1},
	{"SBST", 1},
	{"SDEV", 1},
	{"SLIT", 1},
	{"SRAT", 3},
	{"SSDT", 2},
	{"XSDT", 1},
	{NULL, 0xff}	// end of table
};

static int acpi_table_check_test2(fwts_framework *fw)
{
	const fwts_acpi_table_rev *tables_rev;
	bool failed = false;
	uint32_t version;
	int i;

	version = fwts_get_acpi_version(fw);
	fwts_log_info_verbatim(fw, "System supports ACPI %4.4" PRIx32,  version);

	switch (version) {
		case FWTS_ACPI_VERSION_61:
			tables_rev = acpi_61_rev;
			break;
		case FWTS_ACPI_VERSION_62:
			tables_rev = acpi_62_rev;
			break;
		case FWTS_ACPI_VERSION_63:
			tables_rev = acpi_63_rev;
			break;
		case FWTS_ACPI_VERSION_64:
			tables_rev = acpi_64_rev;
			break;
		case FWTS_ACPI_VERSION_65:
			tables_rev = acpi_65_rev;
			break;
		case FWTS_ACPI_VERSION_66:
			tables_rev = acpi_66_rev;
			break;
		default:
			fwts_log_info_verbatim(fw, "This test does not support ACPI %4.4" PRIx32 ".",  version);
			return FWTS_SKIP;
	}

	for (i = 0; ; i++) {
		const fwts_acpi_table_rev *table_rev;
		fwts_acpi_table_header *hdr;
		fwts_acpi_table_info *info;

		if (fwts_acpi_get_table(fw, i, &info) != FWTS_OK)
			break;
		if (info == NULL)
			continue;

		if (!strcmp(info->name, "FACP") ||
		    !strcmp(info->name, "FACS") ||
				!strcmp(info->name, "RSDP"))
			continue;

		hdr = (fwts_acpi_table_header *) info->data;
		for (table_rev = tables_rev; ; table_rev++) {

			if (!table_rev->name)
				break;

			if (!strncmp(info->name, table_rev->name, 4)) {
				if (table_rev->revision != hdr->revision) {
					failed = true;
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPITableBadRevision",
						"ACPI Table %s revision was expected to be %" PRIu8
						", got %" PRIu8 ".", info->name, table_rev->revision,
						hdr->revision);
				} else
					fwts_log_info(fw, "Table %s has a matched revision.", info->name);
			}
		}
	}

	if (!failed)
		fwts_passed(fw, "ACPI spec %4.4" PRIx32 " has matched table revisions.", version);
	else
		fwts_advice(fw,
		"In \"ACPI Table Revision Overview\", ASWG suggests \"Conforming to a given ACPI "
		"specification means that each and every ACPI-related table conforms to the version "
		"number for that table that is listed in that version of the specification.\" "
		"Please refer to https://uefi.org/node/4185 for details.");

	return FWTS_OK;
}

static fwts_framework_minor_test acpi_table_check_tests[] = {
	{ acpi_table_check_test1, "Test ACPI headers." },
	{ acpi_table_check_test2, "Test ACPI spec versus table revisions." },
	{ NULL, NULL }
};

static fwts_framework_ops acpi_table_check_ops = {
	.description = "ACPI table headers sanity tests.",
	.minor_tests = acpi_table_check_tests
};

FWTS_REGISTER("acpitables", &acpi_table_check_ops, FWTS_TEST_ANYTIME,
	      FWTS_FLAG_BATCH | FWTS_FLAG_ACPI | FWTS_FLAG_SBBR)

#endif
