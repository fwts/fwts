/*
 * Copyright (C) 2014-2026 Canonical
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

#ifndef __FWTS_SAFE_MEMCPY_H__
#define __FWTS_SAFE_MEMCPY_H__

int fwts_safe_memcpy(void *dst, const void *src, const size_t n);
int fwts_safe_memread(const void *src, const size_t n);
int fwts_safe_memread32(const void *src, const size_t n);
int fwts_safe_memread64(const void *src, const size_t n);

#endif
