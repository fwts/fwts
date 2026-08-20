/*
 * Copyright (C) 2010-2026 Canonical
 * Some of this work -  Copyright (C) 2016-2021 IBM
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

#ifndef __FWTS_IPMI_H__
#define __FWTS_IPMI_H__

#include "fwts.h"

#define IPMI_DEV_IPMI_VERSION_MAJOR(x) \
        (x & IPMI_DEV_IPMI_VER_MAJOR_MASK)
#define IPMI_DEV_IPMI_VERSION_MINOR(x) \
        ((x & IPMI_DEV_IPMI_VER_MINOR_MASK) >> IPMI_DEV_IPMI_VER_MINOR_SHIFT)

#define IPMI_DEV_IPMI_VER_MAJOR_MASK	(0x0F)
#define IPMI_DEV_IPMI_VER_MINOR_MASK	(0xF0)

#define IPMI_DEV_IPMI_VER_MINOR_SHIFT	(4)

typedef struct fwts_ipmi_rsp {
	uint8_t completion_code;
	uint8_t device_id;
	uint8_t device_revision;
	uint8_t fw_rev1;
	uint8_t fw_rev2;
	uint8_t ipmi_version;
	uint8_t adtl_device_support;
	uint8_t manufacturer_id[3];
	uint8_t product_id[2];
	uint8_t aux_fw_rev[4];
} __attribute__((packed)) fwts_ipmi_rsp;

int fwts_ipmi_base_query(fwts_ipmi_rsp *fwts_bmc_rsp);
bool fwts_ipmi_present(int fwts_ipmi_flags);

#endif
