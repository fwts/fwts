/*
 * Copyright (C) 2016-2026 Canonical
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

#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <inttypes.h>

#include "fwts_uefi.h"

#define FWTS_ESRT_DIR_PATH		"/sys/firmware/efi/esrt"
#define FWTS_ESRT_ENTRY_PATH		"/sys/firmware/efi/esrt/entries"
#define FWTS_ESRT_RES_COUNT_PATH	"/sys/firmware/efi/esrt/fw_resource_count"
#define FWTS_ESRT_RES_COUNT_MAX_PATH	"/sys/firmware/efi/esrt/fw_resource_count_max"
#define FWTS_ESRT_RES_VERSION_PATH	"/sys/firmware/efi/esrt/fw_resource_version"

/* Current Entry Version */
#define ESRT_FIRMWARE_RESOURCE_VERSION 1

static int esrt_init(fwts_framework *fw)
{

	if (fw->firmware_type != FWTS_FIRMWARE_UEFI) {
		fwts_log_info(fw, "Cannot detect any UEFI firmware. Aborted.");
		return FWTS_ABORTED;
	}

	DIR *dir = opendir(FWTS_ESRT_DIR_PATH);

	if (dir) {
		/* Directory exists. */
		closedir(dir);
		return FWTS_OK;
	} else if (ENOENT == errno) {
		/* Directory does not exist. */
		fwts_log_info(fw, "Cannot find ESRT table, firmware seems not supported. Aborted.");
		return FWTS_ABORTED;
	} else {
		/* opendir() failed for some other reason. */
		fwts_log_info(fw, "Cannot open ESRT directory on /sys/firmware/efi/, Aborted.");
		return FWTS_ABORTED;
	}
}

static void check_entries(fwts_framework *fw, bool *passed)
{

	DIR *dir;
	struct dirent *entry;
	bool entry_found = false;

	if (!(dir = opendir(FWTS_ESRT_ENTRY_PATH))) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotOpenDir",
			"Cannot open directory %s", FWTS_ESRT_ENTRY_PATH);
		*passed = false;
		return;
	}

	do {
		entry = readdir(dir);
		if (entry && strstr(entry->d_name, "entry")) {
			char path[PATH_MAX];
			char *str;
			uint32_t fwversion = 0;
			uint32_t lowest_sp_fwversion;
			bool fwversions_found = true;

			entry_found = true;

			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/fw_class", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetFwClass",
						"Missing or failed to get FwClass on %s.", entry->d_name);
				*passed = false;
			} else
				free(str);

			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/fw_type", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetFwType",
						"Missing or failed to get FwType on %s.", entry->d_name);
				*passed = false;
			} else {
				uint32_t fwtype = strtoul(str, NULL, 10);

				if (fwtype > ESRT_FW_TYPE_UEFIDRIVER) {
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidValue",
						"The FwType value on %s is %" PRIu32
						", which is undefined on UEFI Spec."
						, entry->d_name, fwtype);
					*passed = false;
				}
				free(str);
			}
			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/fw_version", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetFwVersion",
						"Missing or failed to get FwVersion on %s.", entry->d_name);
				*passed = false;
				fwversions_found = false;
			} else {
				fwversion = strtoul(str, NULL, 10);
				free(str);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/lowest_supported_fw_version", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetLowestSupportedFwVersion",
						"Missing or failed to get LowestSupportedFwVersion on %s.", entry->d_name);
				*passed = false;
				fwversions_found = false;
			} else {
				lowest_sp_fwversion = strtoul(str, NULL, 10);
				free(str);
			}

			if (fwversions_found)
				if (fwversion < lowest_sp_fwversion) {
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidValue",
						"The FwVersion is %" PRIu32
						", the LowestSupportedFwVersion is %" PRIu32
						", FwVersion shouldn't lower than the "
						"LowestSupportedFwVersion on %s."
						, fwversion, lowest_sp_fwversion, entry->d_name);
					*passed = false;
			}

			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/capsule_flags", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetCapsuleFlags",
						"Missing or failed to get CapsuleFlags on %s.", entry->d_name);
				*passed = false;
			} else
				free(str);

			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/last_attempt_version", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetLastAttemptVersion",
						"Missing or failed to get LastAttemptVersion on %s.", entry->d_name);
				*passed = false;
			} else
				free(str);

			snprintf(path, sizeof(path), FWTS_ESRT_ENTRY_PATH "/%s/last_attempt_status", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetLastAttemptStatus",
						"Missing or failed to get LastAttemptStatus on %s.", entry->d_name);
				*passed = false;
			} else {
				uint32_t lastattemptst = strtoul(str, NULL, 10);

				if (lastattemptst > LAST_ATTEMPT_STATUS_ERR_UNSATISFIED_DEPENDENCIES &&
					(lastattemptst < LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL_VENDOR_RANGE_MIN ||
					lastattemptst > LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL_VENDOR_RANGE_MAX)) {
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidValue",
						"The LastAttemptStatus value on %s is 0x%" PRIu32
						", which is undefined on UEFI Spec."
						, entry->d_name, lastattemptst);
					*passed = false;
				}
			}
		}
	} while (entry);

	if (!entry_found) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,  "CannotFindEntries",
				"Cannot find any entries on directory %s", FWTS_ESRT_ENTRY_PATH);
		*passed = false;
	}

	closedir(dir);

	return;

}

static int esrt_test1(fwts_framework *fw)
{
	char *str;
	uint32_t count = 0;
	uint32_t countmax = 0;
	bool passed = true;

	if ((str = fwts_get(FWTS_ESRT_RES_COUNT_PATH)) == NULL) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetCount", "Failed to get FwResourceCount.");
		passed = false;
	} else {
		count = strtoul(str, NULL, 10);
		if (count == 0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidValue",
				"The FwResourceCount value should not be 0.");
			passed = false;
		}
		free(str);
	}

	if ((str = fwts_get(FWTS_ESRT_RES_COUNT_MAX_PATH)) == NULL) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetCountMax", "Failed to get FwResourceCount.");
		passed = false;
	} else {
		countmax = strtoul(str, NULL, 10);
		if (countmax == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidValue",
				"The FwResourceCountMax value should not be 0.");
		free(str);
	}

	if (count > countmax) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidValue",
			"The FwResourceCount shouldn't be larger than FwResourceCountMax.");
		passed = false;
	}

	if ((str = fwts_get(FWTS_ESRT_RES_VERSION_PATH)) == NULL) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CannotGetVersion", "Failed to get FwResourceVersion.");
		passed = false;
	} else {
		uint64_t version = strtoull(str, NULL, 10);

		if (version != ESRT_FIRMWARE_RESOURCE_VERSION) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidVersion",
				"The FwResourceVersion is %" PRIu64
				", should be the same as current entries version 1."
				, version);
			passed = false;
		}
		free(str);
	}

	check_entries(fw, &passed);
	if (passed)
		fwts_passed(fw, "No issues found in ESRT table.");
	return FWTS_OK;

}

static fwts_framework_minor_test esrt_tests[] = {
	{ esrt_test1, "Sanity check UEFI ESRT Table." },
	{ NULL, NULL }
};

static fwts_framework_ops esrt_ops = {
	.description = "Sanity check UEFI ESRT Table.",
	.init        = esrt_init,
	.minor_tests = esrt_tests
};

FWTS_REGISTER("esrt", &esrt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_XBBR)

#endif
