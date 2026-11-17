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

#include <stdlib.h>
#include <sys/utsname.h>

#include "fwts.h"

struct fwts_arch_info {
	fwts_architecture arch;
	char *name;
};

static const struct fwts_arch_info arch_info[] = {
	{ FWTS_ARCH_X86, "x86" },
	{ FWTS_ARCH_X86, "x86_32" },
	{ FWTS_ARCH_X86, "x86_64" },
	{ FWTS_ARCH_X86, "i686" },
	{ FWTS_ARCH_IA64, "ia64" },
	{ FWTS_ARCH_ARM64, "arm64" },
	{ FWTS_ARCH_ARM64, "aarch64" },
	{ FWTS_ARCH_RISCV64, "riscv64" },
	{ FWTS_ARCH_LOONGARCH64, "loongarch64" },
	{ FWTS_ARCH_OTHER, "other" }
};

static const struct fwts_arch_info arch_name[] = {
	{ FWTS_ARCH_X86, "x86" },
	{ FWTS_ARCH_IA64, "ia64" },
	{ FWTS_ARCH_ARM64, "arm64" },
	{ FWTS_ARCH_RISCV64, "riscv64" },
	{ FWTS_ARCH_LOONGARCH64, "loongarch64" },
	{ FWTS_ARCH_OTHER, "other" },
};

static char *arch_names;

static fwts_architecture fwts_arch_get_arch_by_name(const char *name)
{
	const struct fwts_arch_info *ptr;

	for (ptr = arch_info; ptr->arch != FWTS_ARCH_OTHER; ptr++)
		if (!strcmp(ptr->name, name))
			return ptr->arch;

	return FWTS_ARCH_OTHER;
}

fwts_architecture fwts_arch_get_host(void)
{
	struct utsname buf;

	if (uname(&buf))
		return FWTS_ARCH_OTHER;

	return fwts_arch_get_arch_by_name(buf.machine);
}

fwts_architecture fwts_arch_get_arch(const char *name)
{
	return fwts_arch_get_arch_by_name(name);
}

char *fwts_arch_names(void)
{
	const struct fwts_arch_info *ptr;
	size_t len;

	if (arch_names)
		return arch_names;

	for (ptr = arch_info, len = 1; ptr->arch != FWTS_ARCH_OTHER; ptr++)
		len += strlen(ptr->name) + 1;

	arch_names = calloc(len, sizeof(char));
	if (arch_names) {
		for (ptr = arch_info; ptr->arch != FWTS_ARCH_OTHER; ptr++) {
			strcat(arch_names, ptr->name);
			strcat(arch_names, " ");
		}
	}

	return arch_names;
}

const char *fwts_arch_get_name(const fwts_architecture arch)
{
	const struct fwts_arch_info *ptr;

	for (ptr = arch_name; ptr->arch != FWTS_ARCH_OTHER; ptr++)
		if (ptr->arch == arch)
			break;

	return ptr->name;
}
