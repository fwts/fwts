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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "fwts_pipeio.h"

static char *efi_dev_name = NULL;
static char *module_name = NULL;

/*
 *  check_module_loaded_no_dev()
 *	sanity check - we don't have a device so we definitely should
 *	not have the module loaded either
 */
static int check_module_loaded_no_dev(
	fwts_framework *fw,
	const char *module)
{
	bool loaded;

	if (fwts_module_loaded(fw, module, &loaded) != FWTS_OK)
		return FWTS_ERROR;
	if (loaded) {
		fwts_log_error(fw, "Module '%s' is already loaded, but device not available.", module);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  check_device()
 *	check if the device exists and is a char dev
 */
static int check_device(char *devname)
{
	struct stat statbuf;

	if (stat(devname, &statbuf))
		return FWTS_ERROR;

	if (S_ISCHR(statbuf.st_mode)) {
		efi_dev_name = devname;
		return FWTS_OK;
	}
	return FWTS_ERROR;
}

/*
 *  load_module()
 *	load the module and check if the device appears
 */
static int load_module(
	fwts_framework *fw,
	char *module,
	char *devname)
{
	bool loaded;

	if (fwts_module_load(fw, module) != FWTS_OK)
		return FWTS_ERROR;

	if (fwts_module_loaded(fw, module, &loaded) != FWTS_OK)
		return FWTS_ERROR;

	if (!loaded)
		return FWTS_ERROR;

	if (check_device(devname) != FWTS_OK)
		return FWTS_ERROR;

	module_name = module;

	return FWTS_OK;
}

/*
 *  fwts_lib_efi_runtime_load_module()
 *	load the runtime module, for historical reasons
 *	we have two names for the module and the device
 *	it creates
 */
int fwts_lib_efi_runtime_load_module(fwts_framework *fw)
{
	efi_dev_name = NULL;
	module_name = NULL;

	/* Check if dev is already available */
	if (check_device("/dev/efi_test") == FWTS_OK)
		return FWTS_OK;
	if (check_device("/dev/efi_runtime") == FWTS_OK)
		return FWTS_OK;

	/* Since the devices can't be found, the module should be not loaded */
	if (check_module_loaded_no_dev(fw, "efi_test") != FWTS_OK)
		return FWTS_ERROR;
	if (check_module_loaded_no_dev(fw, "efi_runtime") != FWTS_OK)
		return FWTS_ERROR;

	/* Now try to load the module */

	if (load_module(fw, "efi_test", "/dev/efi_test") == FWTS_OK)
		return FWTS_OK;
	if (load_module(fw, "efi_runtime", "/dev/efi_runtime") == FWTS_OK)
		return FWTS_OK;

	fwts_log_error(fw, "Failed to load efi test module.");
	return FWTS_ERROR;
}

/*
 *  fwts_lib_efi_runtime_unload_module()
 *	unload the runtile module
 */
int fwts_lib_efi_runtime_unload_module(fwts_framework *fw)
{
	bool loaded;
	char *tmp_name = module_name;

	efi_dev_name = NULL;

	/* No module, not much to do */
	if (!module_name)
		return FWTS_OK;

	module_name = NULL;

	/* Unload module */
	if (fwts_module_unload(fw, tmp_name) != FWTS_OK) {
		fwts_log_error(fw, "Failed to unload module '%s'.", tmp_name);
		return FWTS_ERROR;
	}

	/* Module should not be loaded at this point */
	if (fwts_module_loaded(fw, tmp_name, &loaded) != FWTS_OK)
		return FWTS_ERROR;
	if (loaded) {
		fwts_log_error(fw, "Failed to unload module '%s'.", tmp_name);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  fwts_lib_efi_runtime_open()
 *	open the device
 */
int fwts_lib_efi_runtime_open(void)
{
	if (!efi_dev_name)
		return -1;

	return open(efi_dev_name, O_RDWR);
}

/*
 *  fwts_lib_efi_runtime_close()
 *	close the device
 */
int fwts_lib_efi_runtime_close(const int fd)
{
	return close(fd);
}

/*
 *  fwts_lib_efi_runtime_kernel_lockdown()
 *  check if the kernel has been lockdown
 */
int fwts_lib_efi_runtime_kernel_lockdown(fwts_framework *fw)
{
	char *data;

	if ((data = fwts_get("/sys/kernel/security/lockdown")) != NULL) {
		if (strstr(data, "[none]") == NULL) {
			free(data);
			fwts_log_info(fw, "Kernel is in lockdown mode. Aborted.");
			fwts_log_info(fw, "Please unlock the kernel before you test the UEFI tests.");
			fwts_log_info(fw, "Make sure you disable secureboot and disable "
					"the kernel lockdown, (by kernel parameter lockdown=None).");
			return FWTS_ABORTED;
		}
	}
	free(data);
	return FWTS_OK;
}

/*
 *  fwts_lib_efi_runtime_module_init()
 *  check the EFI firmware support and load efi_test driver
 *  also check if the kernel is in lockdown mode.
 */
int fwts_lib_efi_runtime_module_init(fwts_framework *fw, int *fd)
{

	if (fw->firmware_type != FWTS_FIRMWARE_UEFI) {
		fwts_log_info(fw, "Cannot detect any UEFI firmware. Aborted.");
		return FWTS_ABORTED;
	}

	if (fwts_lib_efi_runtime_kernel_lockdown(fw) == FWTS_ABORTED) {
		return FWTS_ABORTED;
	}

	if (fwts_lib_efi_runtime_load_module(fw) != FWTS_OK) {
		fwts_log_info(fw, "Cannot load efi_runtime module. Aborted.");
		return FWTS_ABORTED;
	}

	*fd = fwts_lib_efi_runtime_open();
	if (*fd == -1) {
		fwts_log_info(fw, "Cannot open EFI test driver. Aborted.");
		return FWTS_ABORTED;
	}

	return FWTS_OK;
}
