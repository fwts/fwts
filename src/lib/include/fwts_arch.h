/*
 * Copyright (C) 2016-2021, Al Stone <ahs3@redhat.com>
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
 */

#ifndef __FWTS_ARCH_H__
#define __FWTS_ARCH_H__

/*
 * Possible architectures for either the host (where FWTS is being run)
 * or the target (what is being tested -- e.g., running FWTS on x86 but
 * examining arm64 ACPI tables).
 *
 * NB: kernel conventions are used for the arch names.
 */
typedef enum {
	FWTS_ARCH_X86,
	FWTS_ARCH_IA64,
	FWTS_ARCH_ARM64,
	FWTS_ARCH_RISCV64,
	FWTS_ARCH_LOONGARCH64,
	FWTS_ARCH_OTHER
} fwts_architecture;

extern fwts_architecture fwts_arch_get_host(void);
extern fwts_architecture fwts_arch_get_arch(const char *name);
extern char *fwts_arch_names(void);
extern const char *fwts_arch_get_name(const fwts_architecture arch);

#endif
