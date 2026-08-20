/*
 * Copyright (C) 2011-2026 Canonical
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

#ifndef __FWTS_HWINFO_H__
#define __FWTS_HWINFO_H__

#include "fwts.h"

typedef struct {
	fwts_list network;	/* PCI network config */
	fwts_list videocard;	/* PCI video card config */
	fwts_list netdevs;	/* Network devices */
	fwts_list input;	/* Input device config */
	fwts_list bluetooth;	/* Bluetooth config */
	fwts_list typec;	/* USB type c config */
	fwts_list scsi_disk;	/* SCSI disk config */
	fwts_list drm;		/* DRM config */
} fwts_hwinfo;

int fwts_hwinfo_get(fwts_framework *fw, fwts_hwinfo *hwinfo);
void fwts_hwinfo_compare(fwts_framework *fw, fwts_hwinfo *hwinfo1, fwts_hwinfo *hwinfo2, int *differences);
int fwts_hwinfo_free(fwts_hwinfo *hwinfo);

#endif
