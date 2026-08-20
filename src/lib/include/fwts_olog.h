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

#ifndef __FWTS_OLOG_H__
#define __FWTS_OLOG_H__

#include <sys/types.h>
#include <regex.h>

#include "fwts_list.h"
#include "fwts_framework.h"
#include "fwts_log.h"

fwts_list *fwts_olog_read(fwts_framework *fw);

typedef void (*fwts_olog_progress_func)(fwts_framework *fw, int percent);
int        fwts_olog_firmware_check(fwts_framework *fw, fwts_olog_progress_func progress, fwts_list *olog, int *errors);

#endif
