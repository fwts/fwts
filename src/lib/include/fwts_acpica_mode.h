/*
 * Copyright (C) 2013-2026 Canonical
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

#ifndef __FWTS_ACPICA_MODE_H__
#define __FWTS_ACPICA_MODE_H__

typedef enum {
	FWTS_ACPICA_MODE_SERIALIZED		= 0x00000001,
	FWTS_ACPICA_MODE_SLACK			= 0x00000002,
	FWTS_ACPICA_MODE_IGNORE_ERRORS		= 0x00000004,
	FWTS_ACPICA_MODE_DISABLE_AUTO_REPAIR	= 0x00000008
} fwts_acpica_mode;

#endif
