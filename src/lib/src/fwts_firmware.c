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
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fwts.h"

static fwts_firmware_type firmware_type;
static bool firmware_type_valid;

/*
 *  fwts_firmware_detect()
 *   check system firmware type
 */
fwts_firmware_type fwts_firmware_detect(void)
{
	struct stat statbuf;

	if (firmware_type_valid)
		return firmware_type;

	if (!stat("/sys/firmware/efi", &statbuf)) {
		firmware_type = FWTS_FIRMWARE_UEFI;

	} else if (!stat("/sys/firmware/opal", &statbuf)) {
		firmware_type = FWTS_FIRMWARE_OPAL;

	} else {
		firmware_type = FWTS_FIRMWARE_BIOS;
	}

	firmware_type_valid = true;
	return firmware_type;
}

int fwts_firmware_features(void)
{
	int features = 0;
	struct stat statbuf;

	if (!stat("/sys/firmware/acpi", &statbuf))
		features |= FWTS_FW_FEATURE_ACPI;
	else if (!stat("/sys/firmware/devicetree/base", &statbuf))
		features |= FWTS_FW_FEATURE_DEVICETREE;

	/* just check for IPMI device presence */
	if (!stat("/dev/ipmi0", &statbuf))
		features |= FWTS_FW_FEATURE_IPMI;

	return features;
}

const char *fwts_firmware_feature_string(const fwts_firmware_feature features)
{
	static const struct {
		const fwts_firmware_feature feature;
		const char name[16];
	} feature_names[] = {
		{ FWTS_FW_FEATURE_ACPI,		"ACPI" },
		{ FWTS_FW_FEATURE_DEVICETREE,	"devicetree" },
		{ FWTS_FW_FEATURE_IPMI,		"IPMI" },
	};

	const int n = FWTS_ARRAY_SIZE(feature_names);
	static const char sep[] = ", ";
	static char str[60];
	size_t len;
	char *p;
	int i;

	/*
	 * ensure we have enough space in str to store n names, plus n-1
	 * separators, plus a trailing nul
	 */
	FWTS_ASSERT((n * (sizeof(feature_names[0].name) - 1)) +
				((n-1) * (sizeof(sep) - 1)) + 1 <
			sizeof(str), str_too_small);

	/* ensure we have a name defined for all features */
	FWTS_ASSERT(((1UL << n) - 1) == FWTS_FW_FEATURE_ALL,
			invalid_feature_names);

	for (p = str, i = 0; i < n; i++) {
		if (!(features & feature_names[i].feature))
			continue;

		/* if this isn't the first feature, add a separator */
		if (p != str) {
			len = sizeof(sep) - 1;
			memcpy(p, sep, len);
			p += len;
		}

		len = strlen(feature_names[i].name);
		memcpy(p, feature_names[i].name, len);
		p += len;
	}

	*p = '\0';

	return str;
}
