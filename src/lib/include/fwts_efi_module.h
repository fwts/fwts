/*
 * Copyright (C) 2012-2026 Canonical
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

#ifndef __FWTS_EFI_MODULE_H__
#define __FWTS_EFI_MODULE_H__

int fwts_lib_efi_runtime_load_module(fwts_framework *fw);
int fwts_lib_efi_runtime_unload_module(fwts_framework *fw);
int fwts_lib_efi_runtime_open(void);
int fwts_lib_efi_runtime_close(const int fd);
int fwts_lib_efi_runtime_kernel_lockdown(fwts_framework *fw);
int fwts_lib_efi_runtime_module_init(fwts_framework *fw, int *fd);

#endif
