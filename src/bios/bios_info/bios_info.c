/*
 * Copyright (C) 2010-2026 Canonical
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

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
	const char *dmi_field;
	const char *label;
} fwts_bios_info;

static const fwts_bios_info bios_info[] = {
	{ "bios_vendor",	"BIOS Vendor" },
	{ "bios_version", 	"BIOS Version" },
	{ "bios_date",		"BIOS Release Date" },
	{ "board_name",		"Board Name" },
	{ "board_serial",	"Board Serial #" },
	{ "board_version",	"Board Version" },
	{ "board_asset_tag",	"Board Asset Tag" },
	{ "chassis_serial", 	"Chassis Serial #" },
	{ "chassis_type",	"Chassis Type " },
	{ "chassis_vendor",	"Chassis Vendor" },
	{ "chassis_version",	"Chassis Version" },
	{ "chassis_asset_tag",	"Chassic Asset Tag" },
	{ "product_name",	"Product Name" },
	{ "product_serial",	"Product Serial #" },
	{ "product_uuid",	"Product UUID " },
	{ "product_version",	"Product Version" },
	{ "sys_vendor",		"System Vendor" },
	{ NULL, NULL }
};

static int bios_info_test1(fwts_framework *fw)
{
	char path[PATH_MAX];
	int i;

	for (i = 0; bios_info[i].dmi_field != NULL; i++) {
		char *data;
		snprintf(path, sizeof(path), "/sys/class/dmi/id/%s",
			bios_info[i].dmi_field);
		if ((data = fwts_get(path)) != NULL) {
			fwts_chop_newline(data);
			fwts_log_info_verbatim(fw, "%-18.18s: %s",
				bios_info[i].label, data);
			free(data);
		}
	}

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test bios_info_tests[] = {
	{ bios_info_test1, "Gather BIOS DMI information" },
	{ NULL, NULL }
};

static fwts_framework_ops bios_info_ops = {
	.description = "Gather BIOS DMI information.",
	.minor_tests = bios_info_tests
};

FWTS_REGISTER("bios_info", &bios_info_ops, FWTS_TEST_FIRST, FWTS_FLAG_BATCH)

#endif
