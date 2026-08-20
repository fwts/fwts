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

#ifndef __FWTS_FIRMWARE_H__
#define __FWTS_FIRMWARE_H__

#include <stdbool.h>

typedef enum {
	FWTS_FIRMWARE_UNKNOWN = 0,
	FWTS_FIRMWARE_BIOS = 1,
	FWTS_FIRMWARE_UEFI = 2,
	FWTS_FIRMWARE_OPAL = 3,
	FWTS_FIRMWARE_OTHER = 100
} fwts_firmware_type;

typedef enum {
	FWTS_FW_FEATURE_ACPI		= 0x1,
	FWTS_FW_FEATURE_DEVICETREE	= 0x2,
	FWTS_FW_FEATURE_IPMI		= 0x4,
	FWTS_FW_FEATURE_ALL		= FWTS_FW_FEATURE_ACPI |
					  FWTS_FW_FEATURE_DEVICETREE |
					  FWTS_FW_FEATURE_IPMI
} fwts_firmware_feature;

fwts_firmware_type fwts_firmware_detect(void);
int fwts_firmware_features(void);
const char *fwts_firmware_feature_string(const fwts_firmware_feature features);

static inline bool fwts_firmware_has_features(const fwts_firmware_feature features)
{
	return (fwts_firmware_features() & features) == features;
}

#endif
