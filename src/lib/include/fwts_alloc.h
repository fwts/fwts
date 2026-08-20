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
#ifndef __FWTS_ALLOC_H__
#define __FWTS_ALLOC_H__

void *fwts_low_calloc(const size_t nmemb, const size_t size);
void *fwts_low_malloc(const size_t size);
void *fwts_low_realloc(const void *ptr, const size_t size);
void fwts_low_free(const void *ptr);

#endif
