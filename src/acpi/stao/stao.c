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

#include "fwts_acpi_object_eval.h"

static fwts_acpi_table_info *table;
acpi_table_init(STAO, &table)

/*
 *  Extract a STAO ACPI String from the raw buffer
 */
static bool stao_acpi_string(
	fwts_framework *fw,
	const char *str,
	const char *end,
	bool *passed,
	size_t *len)
{
	const char *ptr = str;

	while (*ptr) {
		if (ptr > end) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"STAOAcpiStringNotNullTerminated",
				"STAO ACPI String has reached the end of the "
				"ACPI table and did not have a 0x00 string "
				"terminator");
			*len = 0;
			return false;
		}
		ptr++;
	}
	*len = ptr - str;
	return true;
}

/*
 *  STAO Status Override Table
 *    http://wiki.xenproject.org/mediawiki/images/0/02/Status-override-table.pdf
 */
static int stao_test1(fwts_framework *fw)
{
	const fwts_acpi_table_stao *stao = (const fwts_acpi_table_stao *)table->data;
	bool passed = true;
	const char *ptr, *end;
	int strings = 0;

	if (stao->header.length > (uint32_t)table->length) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"STAOBadLength",
			"STAO header reports that the table is %" PRIu32
			" bytes long, however this is longer than the ACPI "
			"table size of %zu bytes.",
			stao->header.length,
			table->length);
		goto done;
	}

	/* Now we have got some sane data, dump the STAO */
        fwts_log_info_verbatim(fw, "STAO Status Override Table:");
        fwts_log_info_simp_int(fw, "  UART:                     ", stao->uart);

	ptr = (const char *)stao->namelist;
	end = (const char *)table->data + stao->header.length;

	while (ptr < end) {
		size_t len;

		if (!stao_acpi_string(fw, ptr, end, &passed, &len))
			break;

		fwts_log_info_verbatim(fw, "  ACPI String:              '%s'", ptr);
		strings++;
		/*  String length + null byte */
		ptr += len + 1;
	}
	if (!strings)
		goto done;

	ptr = (const char *)stao->namelist;
	end = (const char *)table->data + table->length;

	while (ptr < end) {
		bool found;
		size_t len;

		if (!stao_acpi_string(fw, ptr, end, &passed, &len))
			break;

		found = fwts_acpi_obj_find(fw, ptr);

		if (!found) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"STAOAcpiStringNotFound",
				"STAO ACPI String '%s' not found in ACPI object name space.",
				ptr);
		}
		ptr += len + 1;
	}

done:
	if (passed)
		fwts_passed(fw, "No issues found in STAO table.");

	return FWTS_OK;
}

static fwts_framework_minor_test stao_tests[] = {
	{ stao_test1, "STAO Status Override Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops stao_ops = {
	.description = "STAO Status Override Table test.",
	.init        = STAO_init,
	.minor_tests = stao_tests
};

FWTS_REGISTER("stao", &stao_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
