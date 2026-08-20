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
#ifndef __FWTS_IO_PORT_H__
#define __FWTS_IO_PORT_H__

#include <stdint.h>

int fwts_inb(uint32_t port, uint8_t *value);
int fwts_inw(uint32_t port, uint16_t *value);
int fwts_inl(uint32_t port, uint32_t *value);

int fwts_outb(uint32_t port, uint8_t value);
int fwts_outw(uint32_t port, uint16_t value);
int fwts_outl(uint32_t port, uint32_t value);

#endif
