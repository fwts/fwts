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
#ifndef __BRIGHTNESS_HELPER__
#define __BRIGHTNESS_HELPER__

#include "fwts.h"
#include <limits.h>
#include <dirent.h>

DIR *brightness_get_dir(void);
const char *brightness_get_path(void);
int brightness_init(fwts_framework *fw);
int brightness_deinit(fwts_framework *fw);
int brightness_get_setting(const char *entry_name, const char *setting, int *value);
int brightness_set_setting(const char *entry_name, const char *setting, const int value);

#endif
