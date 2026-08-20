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

#include <sys/types.h>
#include <dirent.h>

#include "fwts_uefi.h"

#define FWTS_ESRT_DIR_PATH		"/sys/firmware/efi/esrt/entries"
#define FWTS_ESRT_RES_COUNT_PATH	"/sys/firmware/efi/esrt/fw_resource_count"
#define FWTS_ESRT_RES_COUNT_MAX_PATH	"/sys/firmware/efi/esrt/fw_resource_count_max"
#define FWTS_ESRT_RES_VERSION_PATH	"/sys/firmware/efi/esrt/fw_resource_version"

static int esrtdump_init(fwts_framework *fw)
{
	if (fw->firmware_type != FWTS_FIRMWARE_UEFI) {
		fwts_log_info(fw, "Cannot detect any UEFI firmware. Aborted.");
		return FWTS_ABORTED;
	}
	return FWTS_OK;
}

static int get_entries_info(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *entry;

	if (!(dir = opendir(FWTS_ESRT_DIR_PATH))) {
		fwts_log_error(fw, "Cannot open directory %s", FWTS_ESRT_DIR_PATH);
		return FWTS_ERROR;
	}

	do {
		entry = readdir(dir);
		if (entry && strstr(entry->d_name, "entry")) {
			char path[PATH_MAX];
			char *str, *str_info;
			int count = -1;

			fwts_log_nl(fw);
			fwts_log_info_verbatim(fw, "%s", entry->d_name);

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/fw_class", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_log_error(fw, "Failed to get FwClass");
			} else {
				fwts_log_info_verbatim(fw, "  FwClass:                  %s", str);
				free(str);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/fw_type", entry->d_name);
			if ((fwts_get_int(path, &count)) != FWTS_OK) {
				fwts_log_error(fw, "Failed to get FwType");
			} else {
				switch (count) {
				case ESRT_FW_TYPE_UNKNOWN:
					str_info = "(Unknown)";
					break;
				case ESRT_FW_TYPE_SYSTEMFIRMWARE:
					str_info = "(System Firmware)";
					break;
				case ESRT_FW_TYPE_DEVICEFIRMWARE:
					str_info = "(Device Firmware)";
					break;
				case ESRT_FW_TYPE_UEFIDRIVER:
					str_info = "(UEFI Driver)";
					break;
				default:
					str_info = "";
					break;
				}
				fwts_log_info_verbatim(fw, "  FwType:                   %d %s", count, str_info);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/fw_version", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_log_error(fw, "Failed to get FwVersion");
			} else {
				fwts_log_info_verbatim(fw, "  FwVersion:                %s", str);
				free(str);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/lowest_supported_fw_version", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_log_error(fw, "Failed to get LowestSupportedFwVersion");
			} else {
				fwts_log_info_verbatim(fw, "  LowestSupportedFwVersion: %s", str);
				free(str);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/capsule_flags", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_log_error(fw, "Failed to get CapsuleFlags");
			} else {
				fwts_log_info_verbatim(fw, "  CapsuleFlags:             %s", str);
				free(str);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/last_attempt_version", entry->d_name);
			if ((str = fwts_get(path)) == NULL) {
				fwts_log_error(fw, "Failed to get LastAttemptVersion");
			} else {
				fwts_log_info_verbatim(fw, "  LastAttemptVersion:       %s", str);
				free(str);
			}

			snprintf(path, sizeof(path), FWTS_ESRT_DIR_PATH "/%s/last_attempt_status", entry->d_name);
			if ((fwts_get_int(path, &count)) != FWTS_OK) {
				fwts_log_error(fw, "Failed to get LastAttemptStatus");
			} else {
				switch (count) {
				case LAST_ATTEMPT_STATUS_SUCCESS:
					str_info = "(Success)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL:
					str_info = "(Unsuccessful)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_INSUFFICIENT_RESOURCES:
					str_info = "(Insufficient Resources)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_INCORRECT_VERSION:
					str_info = "(Incorrect Version)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_INVALID_FORMAT:
					str_info = "(Invalid Format)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_AUTH_ERROR:
					str_info = "(Auth Error)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_PWR_EVT_AC:
					str_info = "(PWR EVT AC)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_PWR_EVT_BATT:
					str_info = "(PWR EVT BATT)";
					break;
				case LAST_ATTEMPT_STATUS_ERR_UNSATISFIED_DEPENDENCIES:
					str_info = "(Unsatisfied Dependencies)";
					break;
				default:
					if (count >= LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL_VENDOR_RANGE_MIN
						&& count <= LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL_VENDOR_RANGE_MAX)
						str_info = "(Unsuccessful Vendor Range)";
					else
						str_info = "";
					break;
				}
				fwts_log_info_verbatim(fw, "  LastAttemptStatus:        %d %s", count, str_info);
			}
		}
	} while (entry);

	closedir(dir);
	return FWTS_OK;
}

static int esrtdump_test1(fwts_framework *fw)
{
	char *str;

	if ((str = fwts_get(FWTS_ESRT_RES_COUNT_PATH)) == NULL)
		fwts_log_error(fw, "Failed to get FwResourceCount");
	else {
		fwts_log_info_verbatim(fw, "  FwResourceCount:          %s", str);
		free(str);
	}

	if ((str = fwts_get(FWTS_ESRT_RES_COUNT_MAX_PATH)) == NULL)
		fwts_log_error(fw, "Failed to get FwResourceCountMax");
	else {
		fwts_log_info_verbatim(fw, "  FwResourceCountMax:       %s", str);
		free(str);
	}

	if ((str = fwts_get(FWTS_ESRT_RES_VERSION_PATH)) == NULL)
		fwts_log_error(fw, "Failed to get FwResourceVersion");
	else {
		fwts_log_info_verbatim(fw, "  FwResourceVersion:        %s", str);
		free(str);
	}

	/* find entries */
	return get_entries_info(fw);

}

static fwts_framework_minor_test esrtdump_tests[] = {
	{ esrtdump_test1, "Dump ESRT Table." },
	{ NULL, NULL }
};

static fwts_framework_ops esrtdump_ops = {
	.description = "Dump ESRT table.",
	.init        = esrtdump_init,
	.minor_tests = esrtdump_tests
};

FWTS_REGISTER("esrtdump", &esrtdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS | FWTS_FLAG_ROOT_PRIV)

#endif
