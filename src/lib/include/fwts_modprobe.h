/*
 * Copyright (C) 2020-2026 Canonical
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

#ifndef __FWTS_MODPROBE_H__
#define __FWTS_MODPROBE_H__

#include "fwts_framework.h"

int fwts_module_loaded(fwts_framework *fw, const char *module, bool *loaded);
int fwts_module_load(fwts_framework *fw, const char *module);
int fwts_module_unload(fwts_framework *fw, const char *module);

#endif
