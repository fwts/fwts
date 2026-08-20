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

#define TEST_GUID1 \
{ \
	0xF6FAB04F, 0xACAF, 0x4af3, {0xB9, 0xFA, 0xDC, \
						0xF9, 0x7F, 0xB4, 0x42, 0x6F} \
}

#define TEST_GUID2 \
{ \
	0xBD6D18A3, 0xEE9E, 0x445E, {0xA8, 0x1B, 0xD3, \
						0xDD, 0xB7, 0x11, 0xD0, 0x6E} \
}

#define MAX_DATA_LENGTH		1024

static int fd;
static EFI_GUID gtestguid1 = TEST_GUID1;
static EFI_GUID gtestguid2 = TEST_GUID2;

static uint32_t uefi_get_variable_multiple = 1024;
static uint32_t uefi_set_variable_multiple = 40;
static uint32_t uefi_query_variable_multiple = 1024;

#define UEFI_GET_VARIABLE_MULTIPLE_MAX		(100000)
#define UEFI_SET_VARIABLE_MULTIPLE_MAX		 (10000)
#define UEFI_QUERY_VARIABLE_MULTIPLE_MAX	(100000)

static uint32_t attributes =
	FWTS_UEFI_VAR_NON_VOLATILE |
	FWTS_UEFI_VAR_BOOTSERVICE_ACCESS |
	FWTS_UEFI_VAR_RUNTIME_ACCESS;
static uint16_t variablenametest[] = {'T', 'e', 's', 't', 'v', 'a', 'r', '\0'};
static uint16_t variablenametest2[] = {'T', 'e', 's', 't', 'v', 'a', 'r', ' ', '\0'};
static uint16_t variablenametest3[] = {'T', 'e', 's', 't', 'v', 'a', '\0'};

static uint32_t runtimeservicessupported;

static void uefirtvariable_env_cleanup(void)
{
	struct efi_setvariable setvariable;
	uint64_t status;
	uint8_t data = 0;

	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = &gtestguid1;
	setvariable.Attributes = 0;
	setvariable.DataSize = 0;
	setvariable.Data = &data;
	status = ~0ULL;
	setvariable.status = &status;
	(void)ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	status = ~0ULL;
	setvariable.VariableName = variablenametest2;
	(void)ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	status = ~0ULL;
	setvariable.VariableName = variablenametest3;
	(void)ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	status = ~0ULL;
	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = &gtestguid2;
	(void)ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);
}

static int uefirtvariable_init(fwts_framework *fw)
{
	if (fwts_lib_efi_runtime_module_init(fw, &fd) == FWTS_ABORTED)
		return FWTS_ABORTED;

	uefirtvariable_env_cleanup();

	fwts_uefi_rt_support_status_get(fd, &runtimeservicessupported);

	return FWTS_OK;
}

static int uefirtvariable_deinit(fwts_framework *fw)
{
	fwts_lib_efi_runtime_close(fd);
	fwts_lib_efi_runtime_unload_module(fw);

	return FWTS_OK;
}

static int getvariable_test(
	fwts_framework *fw,
	const uint64_t datasize,
	uint16_t *varname,
	const uint32_t multitesttime)
{
	long ioret;
	struct efi_getvariable getvariable;
	struct efi_setvariable setvariable;

	uint64_t status = ~0ULL;
	uint8_t testdata[MAX_DATA_LENGTH];
	uint64_t dataindex;
	uint64_t getdatasize = sizeof(testdata);
	uint32_t attributestest = 0;

	uint8_t data[datasize];
	uint32_t i;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	for (dataindex = 0; dataindex < datasize; dataindex++)
		data[dataindex] = (uint8_t)dataindex;

	setvariable.VariableName = varname;
	setvariable.VendorGuid = &gtestguid1;
	setvariable.Attributes = attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_OUT_OF_RESOURCES) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"Run out of resources for SetVariable UEFI "
				"runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware may reclaim some resources after "
				"rebooting. Reboot and test again may be "
				"helpful to continue the test.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to set variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, GetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	getvariable.VariableName = varname;
	getvariable.VendorGuid = &gtestguid1;
	getvariable.Attributes = &attributestest;
	getvariable.DataSize = &getdatasize;
	getvariable.Data = testdata;
	getvariable.status = &status;

	for (i = 0; i < multitesttime; i++) {
		status = ~0ULL;
		ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Skipping test, GetVariable runtime "
					"service is not supported on this platform.");
				goto err_restore_env;
			}
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetVariable",
				"Failed to get variable with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			goto err_restore_env;
		}
	}
	if (*getvariable.status != EFI_SUCCESS) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetVariableStatus",
			"Failed to get variable, return status is not "
			"EFI_SUCCESS.");
		fwts_uefi_print_status_info(fw, status);
		goto err_restore_env;
	} else if (*getvariable.Attributes != attributes) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetVariableAttributes",
			"Failed to get variable with right attributes, "
			"attributes we got is %" PRIu32
			", but it should be %" PRIu32 ".",
			*getvariable.Attributes, attributes);
		goto err_restore_env;
	} else if (*getvariable.DataSize != datasize) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetVariableDataSize",
			"Failed to get variable with correct datasize.");
		goto err_restore_env;
	} else {
		for (dataindex = 0; dataindex < datasize; dataindex++) {
			if (data[dataindex] != (uint8_t)dataindex) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"UEFIRuntimeGetVariableData",
					"Failed to get variable with "
					"correct data.");
				goto err_restore_env;
			}
		}
	}

	/* delete the variable */
	setvariable.DataSize = 0;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to delete variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	return FWTS_OK;

err_restore_env:

	setvariable.DataSize = 0;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to delete variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	return FWTS_ERROR;

}

static bool compare_guid(const EFI_GUID *guid1, const EFI_GUID *guid2)
{
	bool ident = true;

	if ((guid1->Data1 != guid2->Data1) ||
	    (guid1->Data2 != guid2->Data2) ||
	    (guid1->Data3 != guid2->Data3))
		ident = false;
	else {
		int i;

		for (i = 0; i < 8; i++) {
			if (guid1->Data4[i] != guid2->Data4[i])
				ident = false;
		}
	}
	return ident;
}

static bool compare_name(const uint16_t *name1, const uint16_t *name2)
{
	bool ident = true;
	int i = 0;

	while (true) {
		if ((name1[i] != name2[i])) {
			ident = false;
			break;
		} else if (name1[i] == '\0')
			break;
		i++;
	}
	return ident;
}

static int getnextvariable_test1(fwts_framework *fw)
{
	long ioret;
	uint64_t status = ~0ULL;

	struct efi_setvariable setvariable;

	uint64_t dataindex, datasize = 10;
	uint8_t data[MAX_DATA_LENGTH];

	struct efi_getnextvariablename getnextvariablename;
	uint64_t variablenamesize = MAX_DATA_LENGTH;
	uint64_t maxvariablenamesize = variablenamesize;
	uint16_t *variablename;
	EFI_GUID vendorguid;
	bool found_name = false, found_guid = false;
	int ret = FWTS_OK;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	for (dataindex = 0; dataindex < datasize; dataindex++)
		data[dataindex] = (uint8_t)dataindex;

	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = &gtestguid1;
	setvariable.Attributes = attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_OUT_OF_RESOURCES) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"Run out of resources for SetVariable UEFI "
				"runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware may reclaim some resources after "
				"rebooting. Reboot and test again may be "
				"helpful to continue the test.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to set variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME)) {
		fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	variablename = malloc(sizeof(uint16_t) * variablenamesize);
	if (!variablename) {
		fwts_skipped(fw, "Unable to alloc memory for variable name");
		ret = FWTS_SKIP;
		goto err_restore_env;
	}
	getnextvariablename.VariableNameSize = &variablenamesize;
	getnextvariablename.VariableName = variablename;
	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.status = &status;

	/*
	 * To start the search, need to pass a Null-terminated string
	 * in VariableName
	 */
	variablename[0] = '\0';
	while (true) {
		variablenamesize = maxvariablenamesize;
		status = ~0ULL;
		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
					"service is not supported on this platform.");
				return FWTS_SKIP;
			}
			/* no next variable was found*/
			if (*getnextvariablename.status == EFI_NOT_FOUND)
				break;

			/*
			 * If the buffer we provided is too small for the name,
			 * allocate a larger buffer and try again
			 */
			if (*getnextvariablename.status == EFI_BUFFER_TOO_SMALL) {
				uint16_t *tmp;

				tmp = realloc(variablename,
					      sizeof(uint16_t) * variablenamesize);
				if (tmp) {
					variablename = tmp;
					getnextvariablename.VariableName = variablename;
					maxvariablenamesize = variablenamesize;
					continue;
				}
				fwts_skipped(fw, "Unable to reallocate memory for variable name");
				ret = FWTS_SKIP;
				goto err_restore_env;
			}

			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to get next variable name with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			ret = FWTS_ERROR;
			goto err_restore_env;
		}
		if (compare_name(getnextvariablename.VariableName, variablenametest))
			found_name = true;
		if (compare_guid(getnextvariablename.VendorGuid, &gtestguid1))
			found_guid = true;
		if (found_name && found_guid)
			break;
	};

	if (variablename) {
		free(variablename);
		getnextvariablename.VariableName = NULL;
		variablename = NULL;
	}

	if (!found_name) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetNextVariableNameName",
			"Failed to get next variable name with right name.");
		goto err_restore_env;
	}
	if (!found_guid) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetNextVariableNameGuid",
			"Failed to get next variable name correct guid.");
		goto err_restore_env;
	}

	/* delete the variable */
	setvariable.DataSize = 0;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to delete variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	return FWTS_OK;

err_restore_env:

	setvariable.DataSize = 0;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);
	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to delete variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		ret = FWTS_ERROR;
	}

	if (variablename)
		free(variablename);

	return ret;
}

/*
 * Return true if variablenamesize is the length of the
 * NULL-terminated unicode string, variablename.
 */
static bool strlen_valid(const uint16_t *variablename, const uint64_t variablenamesize)
{
	uint64_t len;
	uint16_t c;

	for (len = 2; len <= variablenamesize; len += sizeof(c)) {
		uint64_t i = (len / sizeof(c)) - 1;

		c = variablename[i];
		if (!c)
			break;
	}

	return len == variablenamesize;
}

static int getnextvariable_test2(fwts_framework *fw)
{
	uint64_t status;

	struct efi_getnextvariablename getnextvariablename;
	uint64_t variablenamesize = MAX_DATA_LENGTH;
	uint64_t maxvariablenamesize = variablenamesize;
	uint16_t *variablename;
	EFI_GUID vendorguid;
	int ret = FWTS_OK;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME)) {
		fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	variablename = malloc(sizeof(uint16_t) * variablenamesize);
	if (!variablename) {
		fwts_skipped(fw, "Unable to alloc memory for variable name");
		return FWTS_SKIP;
	}

	getnextvariablename.VariableNameSize = &variablenamesize;
	getnextvariablename.VariableName = variablename;
	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.status = &status;

	/*
	 * To start the search, need to pass a Null-terminated string
	 * in VariableName
	 */
	variablename[0] = '\0';
	while (true) {
		long ioret;

		status = ~0ULL;
		variablenamesize = maxvariablenamesize;
		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
					"service is not supported on this platform.");
				return FWTS_SKIP;
			}
			/* no next variable was found*/
			if (*getnextvariablename.status == EFI_NOT_FOUND)
				break;
			/*
			 * If the buffer we provided is too small for the name,
			 * allocate a larger buffer and try again
			 */
			if (*getnextvariablename.status == EFI_BUFFER_TOO_SMALL) {
				uint16_t *tmp;

				tmp = realloc(variablename,
					      sizeof(uint16_t) * variablenamesize);
				if (tmp) {
					variablename = tmp;
					getnextvariablename.VariableName = variablename;
					maxvariablenamesize = variablenamesize;
					continue;
				}
				fwts_skipped(fw, "Unable to reallocate memory for variable name");
				ret = FWTS_SKIP;
				break;
			}

			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to get next variable name with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			ret = FWTS_ERROR;
			break;

		}

		if (!strlen_valid(variablename, variablenamesize)) {
			fwts_warning(fw, "UEFIRuntimeGetNextVariableName "
				"Unexpected variable name size returned.");
			ret = FWTS_ERROR;
			break;
		}
	};

	if (variablename)
		free(variablename);
	return ret;
}

struct efi_var_item {
	struct efi_var_item *next; /* collision chain */
	uint16_t *name;
	EFI_GUID *guid;
	uint64_t hash;
};

/*
 * This is a completely arbitrary value.
 */
#define BUCKET_SIZE 32
static struct efi_var_item *buckets[BUCKET_SIZE];

/*
 * This doesn't have to be a super efficient hashing function with
 * minimal collisions, just more efficient than iterating over a
 * simple linked list.
 */
static inline uint64_t hash_func(uint16_t *variablename, const uint64_t length)
{
	uint64_t i, hash = 0;
	uint16_t *c = variablename;

	for (i = 0; i < length; i++)
		hash = hash * 33 + *c++;

	return hash;
}

/*
 * Return's 0 on success, -1 if there was a collision - meaning we've
 * encountered duplicate variable names with the same GUID.
 */
static int bucket_insert(struct efi_var_item *item)
{
	struct efi_var_item *chain;
	unsigned int index = item->hash % BUCKET_SIZE;

	chain = buckets[index];

	while (chain) {
		/*
		 * OK, we got a collision - no big deal. Walk the
		 * chain and see whether this variable name and
		 * variable guid already appear on it.
		 */
		if (compare_name(item->name, chain->name)) {
			if (compare_guid(item->guid, chain->guid))
				return -1; /* duplicate */
		}

		chain = chain->next;
	}

	item->next = buckets[index];
	buckets[index] = item;
	return 0;
}

static void bucket_destroy(void)
{
	struct efi_var_item *item;
	int i;

	for (i = 0; i < BUCKET_SIZE; i++) {
		item = buckets[i];

		while (item) {
			struct efi_var_item *chain = item->next;

			free(item->name);
			free(item->guid);
			free(item);
			item = chain;
		}
	}
}

static int getnextvariable_test3(fwts_framework *fw)
{
	long ioret;
	uint64_t status;

	struct efi_getnextvariablename getnextvariablename;
	uint64_t variablenamesize = MAX_DATA_LENGTH;
	uint64_t maxvariablenamesize = variablenamesize;
	uint16_t *variablename;
	EFI_GUID vendorguid;
	int ret;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME)) {
		fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	variablename = malloc(sizeof(uint16_t) * variablenamesize);
	if (!variablename) {
		fwts_skipped(fw, "Unable to alloc memory for variable name");
		return FWTS_SKIP;
	}

	getnextvariablename.VariableNameSize = &variablenamesize;
	getnextvariablename.VariableName = variablename;
	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.status = &status;

	/*
	 * To start the search, need to pass a Null-terminated string
	 * in VariableName
	 */
	variablename[0] = '\0';
	while (true) {
		struct efi_var_item *item;

		status = ~0ULL;
		variablenamesize = maxvariablenamesize;
		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
					"service is not supported on this platform.");
				return FWTS_SKIP;
			}
			/* no next variable was found*/
			if (*getnextvariablename.status == EFI_NOT_FOUND)
				break;
			/*
			 * if the buffer we provided is too small for the name,
			 * allocate a larger buffer and try again
			 */
			if (*getnextvariablename.status == EFI_BUFFER_TOO_SMALL) {
				uint16_t *tmp;
				tmp = realloc(variablename,
					      sizeof(uint16_t) * variablenamesize);
				if (tmp) {
					variablename = tmp;
					getnextvariablename.VariableName = variablename;
					maxvariablenamesize = variablenamesize;
					continue;
				}
				fwts_skipped(fw, "Unable to reallocate memory for variable name");
				ret = FWTS_SKIP;
				goto err;
			}

			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to get next variable name with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			ret = FWTS_ERROR;
			goto err;
		}

		item = malloc(sizeof(*item));
		if (!item) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to allocate memory for test.");
			ret = FWTS_ERROR;
			goto err;
		}

		item->guid = malloc(sizeof(EFI_GUID));
		if (!item->guid) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to allocate memory for test.");
			free(item);
			ret = FWTS_ERROR;
			goto err;
		}

		memcpy(item->guid, &vendorguid, sizeof(EFI_GUID));

		item->next = NULL;
		item->name = malloc(variablenamesize);
		if (!item->name) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to allocate memory for test.");
			free(item->guid);
			free(item);
			ret = FWTS_ERROR;
			goto err;
		}

		memcpy(item->name, variablename, variablenamesize);

		/* Ensure there are no duplicate variable names + guid */
		item->hash = hash_func(variablename, variablenamesize);

		if (bucket_insert(item)) {
			char name[variablenamesize];

			fwts_uefi_str16_to_str(name, sizeof(name), variablename);
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Duplicate variable name %s found.", name);

			free(item->name);
			free(item->guid);
			free(item);
			ret = FWTS_ERROR;
			goto err;
		}
	};

	ret = FWTS_OK;
err:
	bucket_destroy();
	if (variablename)
		free(variablename);
	return ret;
}

static int getnextvariable_test4(fwts_framework *fw)
{
	long ioret;
	uint64_t status = ~0ULL;
	uint64_t i;

	struct efi_getnextvariablename getnextvariablename;
	uint64_t variablenamesize = MAX_DATA_LENGTH;
	uint16_t variablename[MAX_DATA_LENGTH];
	EFI_GUID vendorguid;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME)) {
		fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getnextvariablename.VariableNameSize = &variablenamesize;
	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.status = &status;

	/*
	 * Check for expected error values.
	 */
	getnextvariablename.VariableName = NULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetNextVaiableName runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
	}

	if (ioret != -1 || status != EFI_INVALID_PARAMETER) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetNextVariableName",
			"Expected EFI_INVALID_PARAMETER with NULL VariableName.");
		fwts_uefi_print_status_info(fw, status);
		goto err;
	}

	getnextvariablename.VariableName = variablename;
	getnextvariablename.VendorGuid = NULL;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

	if (ioret != -1 || status != EFI_INVALID_PARAMETER) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetNextVariableName",
			"Expected EFI_INVALID_PARAMETER with NULL VendorGuid.");
		fwts_uefi_print_status_info(fw, status);
		goto err;
	}

	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.VariableNameSize = NULL;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

	if (ioret != -1 || status != EFI_INVALID_PARAMETER) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetNextVariableName",
			"Expected EFI_INVALID_PARAMETER with NULL "
			"VariableNameSize.");
		fwts_uefi_print_status_info(fw, status);
		goto err;
	}

	for (i = 0; i < 3; i++) {
		variablenamesize = i;
		getnextvariablename.VariableNameSize = &variablenamesize;

		/*
		 * To start the search, need to pass a Null-terminated
		 * string in VariableName
		 */
		variablename[0] = '\0';
		status = ~0ULL;

		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

		/*
		 * We expect this machine to have at least some UEFI
		 * variables, which is the reason we don't check for
		 * EFI_NOT_FOUND at this point.
		 */
		if (ioret != -1 || status != EFI_BUFFER_TOO_SMALL) {
			if (i != 2 && status == EFI_INVALID_PARAMETER)
				continue;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Expected EFI_BUFFER_TOO_SMALL with small "
				"VariableNameSize.");
			fwts_uefi_print_status_info(fw, status);
			goto err;
		}

		/* Has the firmware failed to update the variable size? */
		if (variablenamesize == i) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"EFI_BUFFER_TOO_SMALL VariableNameSize was "
				"not updated.");
			goto err;
		}
	}

	return FWTS_OK;

err:
	return FWTS_ERROR;
}

static int setvariable_insertvariable(
	fwts_framework *fw,
	const uint32_t var_attributes,
	const uint64_t datasize,
	uint16_t *varname,
	EFI_GUID *gtestguid,
	const uint8_t datadiff)
{
	long ioret;
	struct efi_setvariable setvariable;

	uint64_t status = ~0ULL;
	uint64_t dataindex;

	uint8_t data[datasize + 1];

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	for (dataindex = 0; dataindex < datasize; dataindex++)
		data[dataindex] = (uint8_t)dataindex + datadiff;

	setvariable.VariableName = varname;
	setvariable.VendorGuid = gtestguid;
	setvariable.Attributes = var_attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = &status;
	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if ((status == EFI_INVALID_PARAMETER) &&
			((attributes & FWTS_UEFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS) ||
			(attributes & FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) ||
			(attributes & FWTS_UEFI_VARIABLE_APPEND_WRITE))) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"EFI_INVALID_PARAMETER shall be returned, "
				"when firmware doesn't support these operations "
				"with EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS or "
				"EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS "
				"EFI_VARIABLE_APPEND_WRITE attributes is set.");
			return FWTS_SKIP;
		}
		if (datasize == 0)
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeSetVariable",
				"Failed to delete variable with UEFI "
				"runtime service.");
		else {
			if (status == EFI_OUT_OF_RESOURCES) {
				fwts_uefi_print_status_info(fw, status);
				fwts_skipped(fw,
					"Run out of resources for SetVariable "
					"UEFI runtime interface: cannot test.");
				fwts_advice(fw,
					"Firmware may reclaim some resources "
					"after rebooting. Reboot and test "
					"again may be helpful to continue "
					"the test.");
				return FWTS_SKIP;
			}
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeSetVariable",
				"Failed to set variable with UEFI "
				"runtime service.");
		}
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int setvariable_checkvariable(
	fwts_framework *fw,
	const uint64_t datasize,
	uint16_t *varname,
	EFI_GUID *gtestguid,
	const uint8_t datadiff)
{
	long ioret;
	struct efi_getvariable getvariable;

	uint64_t status = ~0ULL;
	uint8_t testdata[datasize];
	uint64_t dataindex;
	uint64_t getdatasize = sizeof(testdata);
	uint32_t attributestest;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, GetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	getvariable.VariableName = varname;
	getvariable.VendorGuid = gtestguid;
	getvariable.Attributes = &attributestest;
	getvariable.DataSize = &getdatasize;
	getvariable.Data = testdata;
	getvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetVariable",
			"Failed to get variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (*getvariable.Attributes != attributes) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeSetVariableAttributes",
			"Failed to set variable with right attributes, "
			"attributes we got is %" PRIu32
			", but it should both be %" PRIu32 ".",
			attributestest, attributes);
		return FWTS_ERROR;
	} else if (*getvariable.DataSize != datasize) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeSetVariableDataSize",
			"Failed to set variable with correct datasize.");
		return FWTS_ERROR;
	} else {
		for (dataindex = 0; dataindex < datasize; dataindex++) {
			if (testdata[dataindex] != ((uint8_t)dataindex + datadiff)) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"UEFIRuntimeSetVariableData",
					"Failed to set variable with "
					"correct data.");
				return FWTS_ERROR;
			}
		}
	}
	return FWTS_OK;
}

static int setvariable_checkvariable_notfound(
	fwts_framework *fw,
	uint16_t *varname,
	EFI_GUID *gtestguid)
{
	long ioret;
	struct efi_getvariable getvariable;

	uint64_t status = ~0ULL;
	uint8_t testdata[MAX_DATA_LENGTH];
	uint64_t getdatasize = sizeof(testdata);
	uint32_t attributestest;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, GetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	getvariable.VariableName = varname;
	getvariable.VendorGuid = gtestguid;
	getvariable.Attributes = &attributestest;
	getvariable.DataSize = &getdatasize;
	getvariable.Data = testdata;
	getvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);
	/* expect the uefi runtime interface return EFI_NOT_FOUND */
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (*getvariable.status == EFI_NOT_FOUND)
			return FWTS_OK;
	}
	fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
		"Failed to set variable with UEFI runtime service, "
		"expected the status return EFI_NOT_FOUND.");
	fwts_uefi_print_status_info(fw, status);
	return FWTS_ERROR;
}

static int setvariable_invalidattr(
	fwts_framework *fw,
	const uint32_t var_attributes,
	const uint64_t datasize,
	uint16_t *varname,
	EFI_GUID *gtestguid,
	const uint8_t datadiff)
{
	long ioret;
	struct efi_setvariable setvariable;
	uint64_t status = ~0ULL;
	uint64_t dataindex;
	uint8_t data[datasize];

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}


	for (dataindex = 0; dataindex < datasize; dataindex++)
		data[dataindex] = (uint8_t)dataindex + datadiff;

	setvariable.VariableName = varname;
	setvariable.VendorGuid = gtestguid;
	setvariable.Attributes = var_attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
	}
	if ((status == EFI_SUCCESS) && (ioret != -1)) {
		if ((attributes & FWTS_UEFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS) &&
			(attributes & FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) &&
			(status != EFI_INVALID_PARAMETER)) {
			fwts_warning(fw,
				"Both the EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS attribute and the "
				"EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute are set "
				"in a SetVariable call, then the firmware must return EFI_INVALID_PARAMETER.");
		} else {
			fwts_warning(fw,
				"After ExitBootServices() is performed, the "
				"attributes %" PRIu32 ", "
				"for SetVariable shouldn't be set successfully.",
				attributes);
		}
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int setvariable_test1(
	fwts_framework *fw,
	const uint64_t datasize1,
	const uint64_t datasize2,
	uint16_t *varname)
{
	int ret;
	uint8_t datadiff_g2 = 2, datadiff_g1 = 0;

	ret = setvariable_insertvariable(fw, attributes, datasize2,
		varname, &gtestguid2, datadiff_g2);
	if (ret != FWTS_OK)
		return ret;

	ret = setvariable_insertvariable(fw, attributes, datasize1,
		varname, &gtestguid1, datadiff_g1);
	if (ret != FWTS_OK)
		goto err_restore_env1;

	ret = setvariable_checkvariable(fw, datasize2, varname,
		&gtestguid2, datadiff_g2);
	if (ret != FWTS_OK)
		goto err_restore_env;

	ret = setvariable_checkvariable(fw, datasize1, varname,
		&gtestguid1, datadiff_g1);
	if (ret != FWTS_OK)
		goto err_restore_env;

	ret = setvariable_insertvariable(fw, attributes, 0, varname,
		&gtestguid1, datadiff_g1);
	if (ret != FWTS_OK)
		goto err_restore_env1;

	ret = setvariable_insertvariable(fw, attributes, 0, varname,
		&gtestguid2, datadiff_g2);
	if (ret != FWTS_OK)
		return ret;

	return FWTS_OK;

err_restore_env:
	(void)setvariable_insertvariable(fw, attributes, 0, varname,
		&gtestguid1, datadiff_g1);
err_restore_env1:
	(void)setvariable_insertvariable(fw, attributes, 0, varname,
		&gtestguid2, datadiff_g2);

	return ret;

}

static int setvariable_test2(fwts_framework *fw, uint16_t *varname)
{
	int ret;
	uint64_t datasize = 10;
	uint8_t datadiff1 = 0, datadiff2 = 2, datadiff3 = 4;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		varname, &gtestguid1, datadiff1);
	if (ret != FWTS_OK)
		return ret;

	/* insert the same data */
	ret = setvariable_insertvariable(fw, attributes, datasize,
		varname, &gtestguid1, datadiff1);
	if (ret != FWTS_OK)
		return ret;

	ret = setvariable_checkvariable(fw, datasize, varname,
		&gtestguid1, datadiff1);
	if (ret != FWTS_OK)
		goto err_restore_env1;

	/* insert different data */
	datasize = 20;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		varname, &gtestguid1, datadiff2);
	if (ret != FWTS_OK)
		goto err_restore_env1;

	ret = setvariable_checkvariable(fw, datasize, varname,
		&gtestguid1, datadiff2);
	if (ret != FWTS_OK)
		goto err_restore_env2;

	datasize = 5;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		varname, &gtestguid1, datadiff3);
	if (ret != FWTS_OK)
		goto err_restore_env2;

	ret = setvariable_checkvariable(fw, datasize, varname,
		&gtestguid1, datadiff3);
	if (ret != FWTS_OK)
		goto err_restore_env3;

	ret = setvariable_insertvariable(fw, attributes, 0, varname,
		&gtestguid1, datadiff3);
	if (ret != FWTS_OK)
		return ret;

	return FWTS_OK;

err_restore_env1:
	(void)setvariable_insertvariable(fw, attributes, 0,
		varname, &gtestguid1, datadiff1);
	return ret;

err_restore_env2:
	(void)setvariable_insertvariable(fw, attributes, 0,
		varname, &gtestguid1, datadiff2);
	return ret;

err_restore_env3:
	(void)setvariable_insertvariable(fw, attributes, 0,
		varname, &gtestguid1, datadiff3);
	return ret;
}

static int setvariable_test3(fwts_framework *fw)
{
	int ret;
	uint64_t datasize = 10;
	uint8_t datadiff1 = 0, datadiff2 = 1, datadiff3 = 2;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		variablenametest2, &gtestguid1, datadiff2);
	if (ret != FWTS_OK)
		return ret;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		variablenametest3, &gtestguid1, datadiff3);
	if (ret != FWTS_OK)
		goto err_restore_env2;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		variablenametest, &gtestguid1, datadiff1);
	if (ret != FWTS_OK)
		goto err_restore_env1;

	ret = setvariable_checkvariable(fw, datasize,
		variablenametest2, &gtestguid1, datadiff2);
	if (ret != FWTS_OK)
		goto err_restore_env;

	ret = setvariable_checkvariable(fw, datasize,
		variablenametest3, &gtestguid1, datadiff3);
	if (ret != FWTS_OK)
		goto err_restore_env;

	ret = setvariable_checkvariable(fw, datasize,
		variablenametest, &gtestguid1, datadiff1);
	if (ret != FWTS_OK)
		goto err_restore_env;

	ret = setvariable_insertvariable(fw, attributes, 0,
		variablenametest, &gtestguid1, datadiff1);
	if (ret != FWTS_OK)
		goto err_restore_env1;

	ret = setvariable_insertvariable(fw, attributes, 0,
		variablenametest3, &gtestguid1, datadiff3);
	if (ret != FWTS_OK)
		goto err_restore_env2;

	ret = setvariable_insertvariable(fw, attributes, 0,
		variablenametest2, &gtestguid1, datadiff2);
	if (ret != FWTS_OK)
		return ret;

	return FWTS_OK;

err_restore_env:
	(void)setvariable_insertvariable(fw, attributes, 0,
		variablenametest, &gtestguid1, datadiff1);
err_restore_env1:
	(void)setvariable_insertvariable(fw, attributes, 0,
		variablenametest3, &gtestguid1, datadiff3);
err_restore_env2:
	(void)setvariable_insertvariable(fw, attributes, 0,
		variablenametest2, &gtestguid1, datadiff2);

	return ret;

}

static int setvariable_test4(fwts_framework *fw)
{
	int ret;
	uint64_t datasize = 10;
	uint8_t datadiff = 0;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		variablenametest, &gtestguid1, datadiff);
	if (ret != FWTS_OK)
		return ret;

	ret = setvariable_insertvariable(fw, attributes, 0,
		variablenametest, &gtestguid1, datadiff);
	if (ret != FWTS_OK)
		return ret;

	if (setvariable_checkvariable_notfound(fw, variablenametest,
		&gtestguid1) == FWTS_ERROR)
		return FWTS_ERROR;

	return FWTS_OK;
}

static int setvariable_test5(fwts_framework *fw)
{
	int ret;
	uint64_t datasize = 10;
	uint8_t datadiff = 0;

	ret = setvariable_insertvariable(fw, attributes, datasize,
		variablenametest, &gtestguid1, datadiff);
	if (ret != FWTS_OK)
		return ret;

	ret = setvariable_insertvariable(fw, 0, datasize,
		variablenametest, &gtestguid1, datadiff);
	if (ret != FWTS_OK)
		return ret;

	if (setvariable_checkvariable_notfound(fw, variablenametest,
		&gtestguid1) == FWTS_ERROR)
		return FWTS_ERROR;

	return FWTS_OK;
}

static int setvariable_test6(fwts_framework *fw)
{
	uint64_t datasize = 10;
	uint8_t datadiff = 0;
	uint32_t attributesarray[] = {
		FWTS_UEFI_VAR_BOOTSERVICE_ACCESS,
		FWTS_UEFI_VAR_NON_VOLATILE | FWTS_UEFI_VAR_BOOTSERVICE_ACCESS,
		FWTS_UEFI_VAR_BOOTSERVICE_ACCESS | FWTS_UEFI_VAR_RUNTIME_ACCESS
	};
	uint64_t index;

	for (index = 0; index < (sizeof(attributesarray)/(sizeof attributesarray[0])); index++) {
		int ret = setvariable_invalidattr(fw, attributesarray[index], datasize, variablenametest, &gtestguid1, datadiff);

		if (ret == FWTS_ERROR) {
			/* successfully set variable with invalid attributes, test fail */
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIRuntimeSetVariable",
				"Successfully set variable with invalid attribute, expected fail.");
			(void)setvariable_insertvariable(fw, 0, datasize, variablenametest, &gtestguid1, datadiff);
			return FWTS_ERROR;
		}

		if (setvariable_checkvariable_notfound(fw, variablenametest,
			&gtestguid1) == FWTS_ERROR) {
			fwts_log_info(fw,
				"Get the variable which is set by SetVariable "
				"with invalid attribute %"
				PRIu32 " after ExitBootServices() is "
				"performed, test failed.",
				attributesarray[index]);
			(void)setvariable_insertvariable(fw, 0, datasize, variablenametest, &gtestguid1, datadiff);
			return FWTS_ERROR;
		}
	}

	return FWTS_OK;
}

static int setvariable_test7(fwts_framework *fw)
{
	int ret;
	uint64_t datasize = 10;
	uint8_t datadiff = 0;
	uint32_t attr;

	attr = attributes | FWTS_UEFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS | FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
	ret = setvariable_invalidattr(fw, attr, datasize, variablenametest, &gtestguid1, datadiff);
	if (ret == FWTS_ERROR) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIRuntimeSetVariable",
			"Successfully set variable with both authenticated (EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS "
			"EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) attributes are set, expected fail.");
		(void)setvariable_insertvariable(fw, 0, datasize, variablenametest, &gtestguid1, datadiff);
		return FWTS_ERROR;
	}

	if (setvariable_checkvariable_notfound(fw, variablenametest,
		&gtestguid1) == FWTS_ERROR) {
		fwts_log_info(fw,
			"Get the variable which is set by SetVariable with both "
			"authenticated (EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS "
			"EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) "
			"attributes are set %" PRIu32 " , test failed.", attr);
		(void)setvariable_insertvariable(fw, 0, datasize, variablenametest, &gtestguid1, datadiff);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int setvariable_test8(fwts_framework *fw)
{
	long ioret;
	struct efi_setvariable setvariable;
	uint32_t attr = attributes | FWTS_UEFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS;
	uint64_t datasize = 1;
	uint8_t data = 1;
	uint64_t status = ~0ULL;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = &gtestguid1;
	setvariable.Attributes = attr;
	setvariable.DataSize = datasize;
	setvariable.Data = &data;
	setvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (status == EFI_UNSUPPORTED && ioret == -1)
		return FWTS_OK;

	fwts_warning(fw,
		"EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS is deprecated (UEFI 2.7) "
		"and should not be used. Platforms should return EFI_UNSUPPORTED "
		"if a caller to SetVariable() specifies this attribute.");
	fwts_uefi_print_status_info(fw, status);

	return FWTS_ERROR;
}

static int do_queryvariableinfo(
	uint64_t *status,
	uint64_t *remvarstoragesize,
	uint64_t *maxvariablesize)
{
	long ioret;
	struct efi_queryvariableinfo queryvariableinfo;
	uint64_t maxvarstoragesize;

	queryvariableinfo.Attributes = attributes;
	queryvariableinfo.MaximumVariableStorageSize = &maxvarstoragesize;
	queryvariableinfo.RemainingVariableStorageSize = remvarstoragesize;
	queryvariableinfo.MaximumVariableSize = maxvariablesize;
	queryvariableinfo.status = status;
	*status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_QUERY_VARIABLEINFO, &queryvariableinfo);

	if (ioret == -1)
		return FWTS_ERROR;

	return FWTS_OK;
}

static int getnextvariable_multitest(
	fwts_framework *fw,
	const uint32_t multitesttime)
{
	long ioret;
	uint64_t status = ~0ULL;
	uint32_t i;

	struct efi_setvariable setvariable;

	uint64_t dataindex, datasize = 10;
	uint8_t data[MAX_DATA_LENGTH];

	struct efi_getnextvariablename getnextvariablename;
	uint64_t variablenamesize = MAX_DATA_LENGTH;
	uint16_t variablename[MAX_DATA_LENGTH];
	EFI_GUID vendorguid;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	for (dataindex = 0; dataindex < datasize; dataindex++)
		data[dataindex] = (uint8_t)dataindex;

	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = &gtestguid1;
	setvariable.Attributes = attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_OUT_OF_RESOURCES) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"Run out of resources for SetVariable UEFI "
				"runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware may reclaim some resources after "
				"rebooting. Reboot and test again may be "
				"helpful to continue the test.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to set variable with UEFI runtime service.");
		return FWTS_ERROR;
	}

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME)) {
		fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getnextvariablename.VariableNameSize = &variablenamesize;
	getnextvariablename.VariableName = variablename;
	getnextvariablename.VendorGuid = &vendorguid;
	getnextvariablename.status = &status;

	for (i = 0; i < multitesttime; i++) {
		variablename[0] = '\0';
		variablenamesize = MAX_DATA_LENGTH;
		status = ~0ULL;
		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);

		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Skipping test, GetNextVariableName runtime "
					"service is not supported on this platform.");
				return FWTS_SKIP;
			}
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeGetNextVariableName",
				"Failed to get next variable name with "
				"UEFI runtime service.");
			goto err_restore_env;
		}
	};

	setvariable.DataSize = 0;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to set variable with UEFI runtime service.");
		return FWTS_ERROR;
	}

	return FWTS_OK;

err_restore_env:

	setvariable.DataSize = 0;
	status = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to delete variable with UEFI runtime service.");
	}

	return FWTS_ERROR;
}


static int uefirtvariable_test1(fwts_framework *fw)
{
	int ret;
	uint64_t datasize = 10;
	uint32_t multitesttime = 1;

	ret = getvariable_test(fw, datasize, variablenametest, multitesttime);
	if (ret != FWTS_OK)
		return ret;

	fwts_passed(fw, "UEFI runtime service GetVariable interface test passed.");

	return FWTS_OK;
}

static int uefirtvariable_test2(fwts_framework *fw)
{
	int ret;

	fwts_log_info(fw, "The runtime service GetNextVariableName interface function test.");
	ret = getnextvariable_test1(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "The runtime service GetNextVariableName interface function test passed.");

	fwts_log_info(fw, "Check the GetNextVariableName returned value of VariableNameSize is equal to the length of VariableName.");
	ret = getnextvariable_test2(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "Check the GetNextVariableName returned value of VariableNameSize is equal to the length of VariableName passed.");

	fwts_log_info(fw, "Test GetNextVariableName interface returns unique variables.");
	ret = getnextvariable_test3(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "Test GetNextVariableName interface returns unique variables passed.");

	fwts_log_info(fw, "The GetNextVariableName interface conformance tests.");
	ret = getnextvariable_test4(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "The runtime service GetNextVariableName interface conformance tests passed.");


	return FWTS_OK;
}

static int uefirtvariable_test3(fwts_framework *fw)
{
	int ret;
	uint64_t datasize1 = 10, datasize2 = 20;

	fwts_log_info(fw, "Testing SetVariable on two different GUIDs and the same variable name.");
	ret = setvariable_test1(fw, datasize1, datasize2, variablenametest);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on two different GUIDs and the same variable name passed.");

	fwts_log_info(fw, "Testing SetVariable on the same and different variable data.");
	ret = setvariable_test2(fw, variablenametest);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on the same and different variable data passed.");

	fwts_log_info(fw, "Testing SetVariable on similar variable name.");
	ret = setvariable_test3(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on similar variable name passed.");

	fwts_log_info(fw, "Testing SetVariable on DataSize is 0.");
	ret = setvariable_test4(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on DataSize is 0 passed.");

	fwts_log_info(fw, "Testing SetVariable on Attributes is 0.");
	ret = setvariable_test5(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on Attributes is 0 passed.");

	fwts_log_info(fw, "Testing SetVariable on Invalid Attributes.");
	ret = setvariable_test6(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on Invalid Attributes passed.");

	fwts_log_info(fw, "Testing SetVariable with both Authenticated Attributes set.");
	ret = setvariable_test7(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "Testing SetVariable with both Authenticated Attributes set passed.");

	fwts_log_info(fw, "Testing SetVariable with EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS Attributes.");
	ret = setvariable_test8(fw);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "Testing SetVariable with with EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS Attributes passed.");

	return FWTS_OK;
}

static int uefirtvariable_test4(fwts_framework *fw)
{
	uint64_t status;
	uint64_t remvarstoragesize;
	uint64_t maxvariablesize;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_QUERY_VARIABLE_INFO)) {
		fwts_skipped(fw, "Skipping test, QueryVariableInfo runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	if (do_queryvariableinfo(&status, &remvarstoragesize, &maxvariablesize) == FWTS_ERROR) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw,
				"Not support the QueryVariableInfo UEFI "
				"runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware also needs to check if the revision "
				"of system table is correct or not. Linux "
				"kernel returns EFI_UNSUPPORTED as well, if "
				"the FirmwareRevision of system table is less "
				"than EFI_2_00_SYSTEM_TABLE_REVISION.");
			return FWTS_SKIP;
		} else {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeQueryVariableInfo",
				"Failed to query variable info with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	fwts_passed(fw, "UEFI runtime service query variable info interface test passed.");

	return FWTS_OK;
}

static int uefirtvariable_test5(fwts_framework *fw)
{
	int ret;
	uint32_t multitesttime = uefi_get_variable_multiple;
	uint64_t datasize = 10;

	fwts_log_info(fw, "Testing GetVariable on getting the variable %" PRIu32
		" times.", uefi_get_variable_multiple);
	ret = getvariable_test(fw, datasize, variablenametest, multitesttime);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "GetVariable on getting the variable multiple times passed.");

	fwts_log_info(fw, "Testing GetNextVariableName on getting the variable multiple times.");
	ret = getnextvariable_multitest(fw, multitesttime);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "GetNextVariableName on getting the next variable name multiple times passed.");

	return FWTS_OK;

}

static int uefirtvariable_test6(fwts_framework *fw)
{
	int ret;
	uint32_t multitesttime = uefi_set_variable_multiple;
	uint64_t datasize = 10;
	uint8_t datadiff = 0;
	uint32_t i, j;
	uint8_t variablenamelength = 32;
	uint16_t variablenametest4[variablenamelength+1];

	fwts_log_info(fw, "Testing SetVariable on setting the variable with the same data %" PRIu32
		" times.", uefi_set_variable_multiple);
	for (i = 0; i < multitesttime; i++) {
		ret = setvariable_insertvariable(fw, attributes, datasize,
			variablenametest, &gtestguid1, datadiff);
		if (ret != FWTS_OK) {
			if (i > 0)
				(void)setvariable_insertvariable(fw, attributes, 0, variablenametest,
										&gtestguid1, datadiff);
			return ret;
		}
	}
	ret = setvariable_insertvariable(fw, attributes, 0,
		variablenametest, &gtestguid1, datadiff);
	if (ret != FWTS_OK)
		return ret;
	fwts_passed(fw, "SetVariable on setting the variable with the same data multiple times passed.");

	fwts_log_info(fw, "Testing SetVariable on setting the variable with different data %" PRIu32
		" times.", uefi_set_variable_multiple);
	for (i = 0; i < multitesttime; i++) {
		ret = setvariable_insertvariable(fw, attributes, datasize+i,
			variablenametest, &gtestguid1, datadiff);
		if (ret != FWTS_OK)
			return ret;
		ret = setvariable_insertvariable(fw, attributes, 0,
			variablenametest, &gtestguid1, datadiff);
		if (ret != FWTS_OK)
			return ret;
	}
	fwts_passed(fw, "Testing SetVariable on setting the variable with different data multiple times passed.");

	fwts_log_info(fw, "Testing SetVariable on setting the variable with different name %" PRIu32
		" times.", uefi_set_variable_multiple);
	for (i = 0; i < variablenamelength; i++) {
		variablenametest4[i] = 'a';
		variablenametest4[i+1] = '\0';
		ret = setvariable_insertvariable(fw, attributes, datasize,
			variablenametest4, &gtestguid1, datadiff);
		if (ret != FWTS_OK)
			return ret;
		ret = setvariable_insertvariable(fw, attributes, 0,
			variablenametest4, &gtestguid1, datadiff);
		if (ret != FWTS_OK)
			return ret;
	}
	fwts_passed(fw, "Testing SetVariable on setting the variable with different name multiple times passed.");

	fwts_log_info(fw, "Testing SetVariable on setting the variable with different name and data %" PRIu32
		" times.", uefi_set_variable_multiple);

	/*
	 * This combine test do a lot of setvariable, reduce variablenamelength
	 * and multitesttime, for saving the setvariable times to avoid running
	 * out of nvram space and getting the EFI_OUT_OF_RESOURCES
	 */
	variablenamelength /= 4;
	multitesttime /= 4;

	for (i = 0; i < variablenamelength; i++) {
		variablenametest4[i] = 'a';
		variablenametest4[i+1] = '\0';
		for (j = 0; j < multitesttime; j++) {
			ret = setvariable_insertvariable(fw, attributes,
				datasize+j, variablenametest4, &gtestguid1,
				datadiff);
			if (ret != FWTS_OK)
				return ret;
			ret = setvariable_insertvariable(fw, attributes, 0,
				variablenametest4, &gtestguid1, datadiff);
			if (ret != FWTS_OK)
				return ret;
		}
	}
	fwts_passed(fw, "Testing SetVariable on setting the variable with different name and data multiple times passed.");

	return FWTS_OK;
}

static int uefirtvariable_test7(fwts_framework *fw)
{
	uint32_t multitesttime = uefi_query_variable_multiple;
	uint64_t status;
	uint64_t remvarstoragesize;
	uint64_t maxvariablesize;
	uint32_t i;

	fwts_log_info(fw, "Testing QueryVariableInfo on querying the variable %" PRIu32
		" times.", uefi_query_variable_multiple);

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_QUERY_VARIABLE_INFO)) {
		fwts_skipped(fw, "Skipping test, QueryVariableInfo runtime "
			"service is not supported on this platform.");
		return FWTS_SKIP;
	}

	/* first check if the firmware support QueryVariableInfo interface */
	if (do_queryvariableinfo(&status, &remvarstoragesize, &maxvariablesize) == FWTS_ERROR) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw,
				"Not support the QueryVariableInfo UEFI "
				"runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware also needs to check if the revision "
				"of system table is correct or not. Linux "
				"kernel returns EFI_UNSUPPORTED as well, if "
				"the FirmwareRevision of system table is less "
				"than EFI_2_00_SYSTEM_TABLE_REVISION.");
			return FWTS_SKIP;
		} else {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeQueryVariableInfo",
				"Failed to query variable info with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	for (i = 0; i < multitesttime; i++) {
		if (do_queryvariableinfo(&status, &remvarstoragesize, &maxvariablesize) == FWTS_ERROR) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIRuntimeQueryVariableInfo",
				"Failed to query variable info with UEFI "
				"runtime service.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}
	fwts_passed(fw, "UEFI runtime service query variable info interface stress test passed.");

	return FWTS_OK;
}

static void getvariable_test_invalid(
	fwts_framework *fw,
	struct efi_getvariable *getvariable,
	const char *test)
{
	long ioret;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, GetVariable runtime service "
			"is not supported on this platform.");
		return;
	}

	fwts_log_info(fw, "Testing GetVariable with %s.", test);
	*(getvariable->status) = ~0ULL;

	ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, getvariable);

	if (ioret == -1) {
		if (*(getvariable->status) == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetVariable runtime "
				"service is not supported on this platform.");
			return;
		}
		if (*(getvariable->status) == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "GetVariable with %s returned error "
				"EFI_INVALID_PARAMETER as expected.", test);
			return;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIRuntimeGetVariableInvalid",
			"GetVariable with %s failed to get expected error return "
			"status, expected EFI_INVALID_PARAMETER.", test);
		fwts_uefi_print_status_info(fw, *(getvariable->status));
		return;
	}
	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFIRuntimeGetVariableInvalid",
		"GetVariable with %s failed to get an error return status, "
		"expected EFI_INVALID_PARAMETER.", test);

	return;

}

static int uefirtvariable_test8(fwts_framework *fw)
{
	struct efi_getvariable getvariable;
	struct efi_setvariable setvariable;
	uint8_t data[16];
	uint64_t status= ~0ULL;
        uint64_t dataindex;
	uint64_t getdatasize = sizeof(data);
	uint32_t attr;
	int ioret;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		fwts_skipped(fw, "Skipping test, SetVariable runtime service "
			"is not supported on this platform.");
		return FWTS_SKIP;
	}

	for (dataindex = 0; dataindex < sizeof(data); dataindex++)
		data[dataindex] = (uint8_t)dataindex;

	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = &gtestguid1;
	setvariable.Attributes = attributes;
	setvariable.DataSize = sizeof(data);
	setvariable.Data = data;
	setvariable.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, SetVariable runtime "
				"service is not supported on this platform.");
			return FWTS_OK;
		}
		if (status == EFI_OUT_OF_RESOURCES) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"Run out of resources for SetVariable UEFI "
				"runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware may reclaim some resources after "
				"rebooting. Reboot and test again may be "
				"helpful to continue the test.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to set variable with UEFI runtime service.");
		return FWTS_ERROR;
	}

	getvariable.VariableName = NULL;
	getvariable.VendorGuid = &gtestguid1;
	getvariable.Attributes = &attr;
	getvariable.DataSize = &getdatasize;
	getvariable.Data = data;
	getvariable.status = &status;
	getvariable_test_invalid(fw, &getvariable, "NULL variable name");

	getvariable.VariableName = variablenametest;
	getvariable.VendorGuid = NULL;
	getvariable.Attributes = &attr;
	getvariable.DataSize = &getdatasize;
	getvariable.Data = data;
	getvariable.status = &status;
	getvariable_test_invalid(fw, &getvariable, "NULL vendor GUID");

	getvariable.VariableName = variablenametest;
	getvariable.VendorGuid = &gtestguid1;
	getvariable.Attributes = &attr;
	getvariable.DataSize = NULL;
	getvariable.Data = data;
	getvariable.status = &status;
	getvariable_test_invalid(fw, &getvariable, "NULL datasize");

	getvariable.VariableName = variablenametest;
	getvariable.VendorGuid = &gtestguid1;
	getvariable.Attributes = &attr;
	getvariable.DataSize = &getdatasize;
	getvariable.Data = NULL;
	getvariable.status = &status;
	getvariable_test_invalid(fw, &getvariable, "NULL data");

	getvariable.VariableName = NULL;
	getvariable.VendorGuid = NULL;
	getvariable.Attributes = &attr;
	getvariable.DataSize = NULL;
	getvariable.Data = NULL;
	getvariable.status = &status;
	getvariable_test_invalid(fw, &getvariable, "NULL variable name, vendor GUID, datasize and data");

	/* delete the variable */
	setvariable.DataSize = 0;
	status = ~0ULL;
	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);
	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
			"Failed to delete variable with UEFI runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int uefirtvariable_test9(fwts_framework *fw)
{
	long ioret;

	struct efi_getvariable getvariable;
	struct efi_setvariable setvariable;
	struct efi_getnextvariablename getnextvariablename;
	struct efi_queryvariableinfo queryvariableinfo;

	EFI_GUID guid;
	uint64_t status = ~0ULL;
	uint8_t data = 1;
	uint64_t datasize = 1;
	uint64_t variablenamesize = MAX_DATA_LENGTH;
	uint16_t *variablename;
	uint64_t remvarstoragesize;
	uint64_t maxvariablesize;
	uint64_t maxvarstoragesize;
	uint8_t testdata[MAX_DATA_LENGTH];
	uint64_t getdatasize = sizeof(testdata);
	uint32_t attr;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_SET_VARIABLE)) {
		setvariable.VariableName = variablenametest;
		setvariable.VendorGuid = &gtestguid1;
		setvariable.Attributes = attributes;
		setvariable.DataSize = datasize;
		setvariable.Data = &data;
		setvariable.status = &status;

		ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
				fwts_passed(fw, "UEFI SetVariable runtime service "
					"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
						"Get the SetVariable runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS)
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeSetVariable",
					"Get the SetVariable runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
		}
	}
	else
		fwts_skipped(fw, "SetVariable runtime service supported, skip test.");

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_VARIABLE)) {
		getvariable.VariableName = variablenametest;
		getvariable.VendorGuid = &gtestguid1;
		getvariable.Attributes = &attr;
		getvariable.DataSize = &getdatasize;
		getvariable.Data = testdata;
		getvariable.status = &status;

		ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
				fwts_passed(fw, "UEFI GetVariable runtime service "
					"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetVariable",
						"Get the GetVariable runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS)
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetVariable",
					"Get the GetVariable runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
		}
	} else
		fwts_skipped(fw, "GetVariable runtime service supported, skip test.");

	/* delete the variable which was set */
	setvariable.DataSize = 0;
	status = ~0ULL;
	(void)ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	variablename = malloc(sizeof(uint16_t) * variablenamesize);
	if (!variablename) {
		fwts_skipped(fw, "Unable to alloc memory for variable name");
		return FWTS_SKIP;
	}

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME)) {
		getnextvariablename.VariableNameSize = &variablenamesize;
		getnextvariablename.VariableName = variablename;
		getnextvariablename.VendorGuid = &guid;
		getnextvariablename.status = &status;
		variablename[0] = '\0';
		status = ~0ULL;

		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTVARIABLENAME, &getnextvariablename);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
				fwts_passed(fw, "UEFI GetNextVarName runtime service "
					"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetNextVarName",
						"Get the GetNextVarName runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS)
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetNextVarName",
					"Get the GetNextVarName runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
		}
	} else
		fwts_skipped(fw, "GetNextVarName runtime service supported, skip test.");

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_QUERY_VARIABLE_INFO)) {
		queryvariableinfo.Attributes = attributes;
		queryvariableinfo.MaximumVariableStorageSize = &maxvarstoragesize;
		queryvariableinfo.RemainingVariableStorageSize = &remvarstoragesize;
		queryvariableinfo.MaximumVariableSize = &maxvariablesize;
		queryvariableinfo.status = &status;
		status = ~0ULL;

		ioret = ioctl(fd, EFI_RUNTIME_QUERY_VARIABLEINFO, &queryvariableinfo);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
				fwts_passed(fw, "UEFI QueryVarInfo runtime service "
					"unsupported status test passed.");
			else {
				if (status == ~0ULL)
					fwts_skipped(fw, "Unknown error occurred, skip test.");
				else
					fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeQueryVarInfo",
						"Get the QueryVarInfo runtime service unsupported "
						"via RuntimeServicesSupported mask. "
						"But actually is supported by firmware.");
			}
		} else {
			if (status != EFI_SUCCESS)
				fwts_skipped(fw, "Unknown error occurred, skip test.");
			else {
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeQueryVarInfo",
					"Get the QueryVarInfo runtime service unsupported "
					"via RuntimeServicesSupported mask. "
					"But actually is supported by firmware.");
			}
		}
	} else
		fwts_skipped(fw, "QueryVarInfo runtime service supported, skip test.");

	if (variablename)
		free(variablename);

	return FWTS_OK;
}

static int options_check(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if ((uefi_get_variable_multiple < 1) ||
	    (uefi_get_variable_multiple > UEFI_GET_VARIABLE_MULTIPLE_MAX)) {
		fprintf(stderr, "--uefi-get-var-multiple is %" PRIu32", it "
			"should be 1..%" PRIu32 "\n",
			uefi_get_variable_multiple, UEFI_GET_VARIABLE_MULTIPLE_MAX);
		return FWTS_ERROR;
	}
	if ((uefi_set_variable_multiple < 1) ||
	    (uefi_set_variable_multiple > UEFI_SET_VARIABLE_MULTIPLE_MAX)) {
		fprintf(stderr, "--uefi-set-var-multiple is %" PRIu32", it "
			"should be 1..%" PRIu32 "\n",
			uefi_set_variable_multiple, UEFI_SET_VARIABLE_MULTIPLE_MAX);
		return FWTS_ERROR;
	}
	if ((uefi_query_variable_multiple < 1) ||
	    (uefi_query_variable_multiple > UEFI_QUERY_VARIABLE_MULTIPLE_MAX)) {
		fprintf(stderr, "--uefi-query-var-multiple is %" PRIu32", it "
			"should be 1..%" PRIu32 "\n",
			uefi_query_variable_multiple, UEFI_QUERY_VARIABLE_MULTIPLE_MAX);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int options_handler(
	fwts_framework *fw,
	int argc,
	char * const argv[],
	int option_char,
	int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);

	if (option_char == 0) {
		switch (long_index) {
		case 0:	/* --uefi-get-var-multiple */
			uefi_get_variable_multiple = strtoul(optarg, NULL, 10);
			break;
		case 1:	/* --uefi-set-var-multiple */
			uefi_set_variable_multiple = strtoul(optarg, NULL, 10);
			break;
		case 2: /* --uefi-query-var-multiple */
			uefi_query_variable_multiple = strtoul(optarg, NULL, 10);
			break;
		}
	}
	return FWTS_OK;
}

static fwts_option options[] = {
	{ "uefi-get-var-multiple",	"", 1, "Run uefirtvariable get variable test multiple times." },
	{ "uefi-set-var-multiple",	"", 1, "Run uefirtvariable set variable test multiple times." },
	{ "uefi-query-var-multiple", 	"", 1, "Run uefirtvariable query variable test multiple times." },
	{ NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test uefirtvariable_tests[] = {
	{ uefirtvariable_test1, "Test UEFI RT service get variable interface." },
	{ uefirtvariable_test2, "Test UEFI RT service get next variable name interface." },
	{ uefirtvariable_test3, "Test UEFI RT service set variable interface." },
	{ uefirtvariable_test4, "Test UEFI RT service query variable info interface." },
	{ uefirtvariable_test5, "Test UEFI RT service variable interface stress test." },
	{ uefirtvariable_test6, "Test UEFI RT service set variable interface stress test." },
	{ uefirtvariable_test7, "Test UEFI RT service query variable info interface stress test." },
	{ uefirtvariable_test8, "Test UEFI RT service get variable interface, invalid parameters." },
	{ uefirtvariable_test9, "Test UEFI RT variable services unsupported status." },
	{ NULL, NULL }
};

static fwts_framework_ops uefirtvariable_ops = {
	.description     = "UEFI Runtime service variable interface tests.",
	.init            = uefirtvariable_init,
	.deinit          = uefirtvariable_deinit,
	.minor_tests     = uefirtvariable_tests,
	.options         = options,
	.options_handler = options_handler,
	.options_check   = options_check,
};

FWTS_REGISTER("uefirtvariable", &uefirtvariable_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_XBBR)

#endif
