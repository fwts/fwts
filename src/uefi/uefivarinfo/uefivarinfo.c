/*
 * Copyright (C) 2013-2024 Canonical
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_UEFI)

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "fwts_uefi.h"
#include "fwts_efi_runtime.h"
#include "fwts_efi_module.h"

#define MAX_VARNAME_LENGTH	1024

static int fd;

static int uefivarinfo_init(fwts_framework *fw)
{
	if (fwts_lib_efi_runtime_module_init(fw, &fd) == FWTS_ABORTED)
		return FWTS_ABORTED;
	return FWTS_OK;
}

static int uefivarinfo_deinit(fwts_framework *fw)
{
	fwts_lib_efi_runtime_close(fd);
	fwts_lib_efi_runtime_unload_module(fw);

	return FWTS_OK;
}

static int do_checkvariables(
	fwts_framework *fw,
	uint64_t *usedvars,
	uint64_t *usedvarssize,
	const uint64_t maxvarsize)
{
	uint64_t status;

	struct efi_getnextvariablename getnextvariablename;
	uint64_t variablenamesize = MAX_VARNAME_LENGTH;
	uint16_t variablename[MAX_VARNAME_LENGTH];
	EFI_GUID vendorguid;

	uint8_t *data;
	uint64_t getdatasize;

	uint32_t attributestest;
	struct efi_getvariable getvariable;
	getvariable.Attributes = &attributestest;
	getvariable.status = &status;

	getnextvariablename.VariableNameSize = &variablenamesize;
	getnextvariablename.VariableName = variablename;
	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.status = &status;

	*usedvars = 0;
	*usedvarssize = 0;

	/*
	 * To start the search, need to pass a Null-terminated string
	 * in VariableName
	 */
	variablename[0] = '\0';
	while (true) {
		long ioret;
		status = ~0ULL;

		variablenamesize = MAX_VARNAME_LENGTH;
		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

		if (ioret == -1) {

			/* no next variable was found*/
			if (*getnextvariablename.status == EFI_NOT_FOUND)
				break;

			fwts_log_info(fw, "Failed to get next variable name with UEFI runtime service.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}

		(*usedvars)++;

		data = malloc(maxvarsize);
		if (!data) {
			fwts_log_info(fw, "Failed to allocate memory for test.");
			return FWTS_ERROR;
		}

		getdatasize = maxvarsize;
		getvariable.VariableName = variablename;
		getvariable.VendorGuid = &vendorguid;
		getvariable.DataSize = &getdatasize;
		getvariable.Data = data;
		status = ~0ULL;

		ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);
		if (ioret == -1) {
			if (status != EFI_BUFFER_TOO_SMALL) {
				free(data);
				fwts_log_info(fw, "Failed to get variable with UEFI runtime service.");
				fwts_uefi_print_status_info(fw, status);
				return FWTS_ERROR;
			} else if (getdatasize > maxvarsize) {
				free(data);
				fwts_log_info(fw, "Variable is larger than maximum variable length.");
				fwts_uefi_print_status_info(fw, status);

				/*
				 * Although the variable is larger than maximum variable length,
				 * still try to calculate the total sizes of the used variables.
				 */
				data = malloc(getdatasize);
				if (!data) {
					fwts_log_info(fw, "Failed to allocate memory for test.");
					return FWTS_ERROR;
				}

				getvariable.Data = data;
				status = ~0ULL;

				ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);
				if (ioret == -1) {
					fwts_log_info(fw, "Failed to get variable with variable larger than maximum variable length.");
					fwts_uefi_print_status_info(fw, status);
					free(data);
					return FWTS_ERROR;
				}
			}
		}
		free(data);

		(*usedvarssize) += getdatasize;

	};

	return FWTS_OK;

}

static int do_queryvariableinfo(
	uint64_t *status,
	uint64_t *maxvarstoragesize,
	uint64_t *remvarstoragesize,
	uint64_t *maxvariablesize)
{
	long ioret;
	struct efi_queryvariableinfo queryvariableinfo;

	queryvariableinfo.Attributes = FWTS_UEFI_VAR_NON_VOLATILE |
					FWTS_UEFI_VAR_BOOTSERVICE_ACCESS |
					FWTS_UEFI_VAR_RUNTIME_ACCESS;
	queryvariableinfo.MaximumVariableStorageSize = maxvarstoragesize;
	queryvariableinfo.RemainingVariableStorageSize = remvarstoragesize;
	queryvariableinfo.MaximumVariableSize = maxvariablesize;
	*status = ~0ULL;
	queryvariableinfo.status = status;

	ioret = ioctl(fd, EFI_RUNTIME_QUERY_VARIABLEINFO, &queryvariableinfo);

	if (ioret == -1)
		return FWTS_ERROR;

	return FWTS_OK;
}

static int uefivarinfo_test1(fwts_framework *fw)
{
	uint64_t status;
	uint64_t remvarstoragesize;
	uint64_t maxvariablesize;
	uint64_t maxvarstoragesize;

	uint64_t usedvars;
	uint64_t usedvarssize;

	if (do_queryvariableinfo(&status, &maxvarstoragesize, &remvarstoragesize, &maxvariablesize) == FWTS_ERROR) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw,
				"QueryVariableInfo UEFI runtime interface not supported: cannot test.");
			fwts_advice(fw,
				"Firmware also needs to check if the revision "
				"of system table is correct or not. Linux "
				"kernel returns EFI_UNSUPPORTED as well, if "
				"the FirmwareRevision of system table is less "
				"than EFI_2_00_SYSTEM_TABLE_REVISION.");
			return FWTS_SKIP;
		} else {
			fwts_log_info(fw, "Failed to query variable info with UEFI runtime service.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}

	fwts_log_info_verbatim(fw, "UEFI NVRAM storage:");
	fwts_log_info_verbatim(fw, "  Maximum storage:       %8" PRIu64 " bytes", maxvarstoragesize);
	fwts_log_info_verbatim(fw, "  Remaining storage:     %8" PRIu64 " bytes", remvarstoragesize);
	fwts_log_info_verbatim(fw, "  Maximum variable size: %8" PRIu64 " bytes", maxvariablesize);

	if (do_checkvariables(fw, &usedvars, &usedvarssize, maxvariablesize) == FWTS_OK) {
		fwts_log_info_verbatim(fw, "Currently used:");
		fwts_log_info_verbatim(fw, "  %" PRIu64 " variables, storage used: %" PRIu64 " bytes", usedvars, usedvarssize);
	}

	fwts_passed(fw, "UEFI variable info query test passed.");

	return FWTS_OK;
}

static fwts_framework_minor_test uefivarinfo_tests[] = {
	{ uefivarinfo_test1, "UEFI variable info query." },
	{ NULL, NULL }
};

static fwts_framework_ops uefivarinfo_ops = {
	.description = "UEFI variable info query.",
	.init        = uefivarinfo_init,
	.deinit      = uefivarinfo_deinit,
	.minor_tests = uefivarinfo_tests
};

FWTS_REGISTER("uefivarinfo", &uefivarinfo_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS | FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_EBBR)

#endif
