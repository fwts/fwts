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

#ifndef __FWTS_BUTTON_H__
#define __FWTS_BUTTON_H__

#define FWTS_BUTTON_LID_ANY		(0x0000)
#define FWTS_BUTTON_LID_OPENED		(0x0001)
#define FWTS_BUTTON_LID_CLOSED		(0x0002)

#define FWTS_BUTTON_POWER_EXISTS	(0x00010)

int fwts_button_match_state(fwts_framework *fw, const uint32_t button, uint32_t *matched, uint32_t *not_matched);

#endif
