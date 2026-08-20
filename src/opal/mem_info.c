/*
 * Copyright (C) 2010-2026 Canonical
 * Some of this work - Copyright (C) 2016-2021 IBM
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

#define _GNU_SOURCE /* added for asprintf */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include "fwts.h"

#include <libfdt.h>

static bool found_dimm = false;

static int get_dimm_property(
	fwts_framework *fw,
	char *my_path,
	bool hex,
	char *property)
{
	int failures = 0;
	char *prop_string = strstr(my_path, "/memory-buffer");

	if (prop_string) {
		int node = fdt_path_offset(fw->fdt, prop_string);

		if (node >= 0) {
			const char *prop_buf;
			int prop_len = 0;

			prop_buf = fdt_getprop(fw->fdt, node,
					property,
					&prop_len);
			if ((prop_len > 0) && (prop_buf)) {
				fwts_log_nl(fw);
				if ((hex) && (prop_len == 4)) {
					fwts_passed(fw, "OPAL MEM Info"
						" Property of \"%s\" for "
						"\"%s\" is "
						"\"0x%02X%02X%02X%02X\".",
						property,
						my_path,
						prop_buf[0],
						prop_buf[1],
						prop_buf[2],
						prop_buf[3]);
				} else if (!hex) {
					if (check_property_printable(fw,
						property,
						prop_buf,
						prop_len)) {
							failures++;
					}
					fwts_passed(fw,
						"OPAL MEM Info Property of"
						" \"%s\" for \"%s\" "
						"is \"%s\".",
						property,
						my_path,
						prop_buf);
				} else {
					failures++;
					fwts_log_nl(fw);
					fwts_failed(fw,
						LOG_LEVEL_CRITICAL,
						"OPAL MEM Info",
						"Property of \"%s\" for "
						"\"%s\" is not properly "
						"formatted. Check your "
						"installation for proper "
						" device tree nodes.",
						property,
						my_path);
				}
			} else {
				failures++;
				fwts_log_nl(fw);
				fwts_failed(fw,
					LOG_LEVEL_CRITICAL,
					"OPAL MEM Info",
					"Property of \"%s\" for \"%s\" was not"
					" able to be retrieved. Check the "
					"installation for the MEM device "
					"config for missing nodes in the device"
					" tree if you expect MEM devices.",
					property,
					my_path);
			}
		} else {
			failures++;
			fwts_log_nl(fw);
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"OPAL MEM Info",
					"Failed to get node for memory-buffer"
					" \"%s/%s\", please check the system"
					" for setup issues.",
					my_path, property);
		}
	} else {
		failures++;
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL MEM Info",
				"Internal processing problem at strstr"
				" my_path of \"%s\" for \"%s\", please check"
				" the system for setup issues.",
				my_path,
				property);
	}
	if (failures) {
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}
}

static int process_dimm(
	fwts_framework *fw,
	char *my_string,
	char *my_dir)
{
	int count, i, failures = 0;
	struct dirent **namelist = NULL;
	bool found = false;

	count = scandir(my_dir, &namelist, NULL, alphasort);
	if (count < 0) {
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MEM Info",
			"Scan for MEM devices in '%s' is unable to find any "
			"candidates. Check the installation "
			"for the MEM device config for missing nodes"
			" in the device tree if you expect MEM devices.",
			my_dir);
		return FWTS_ERROR;
	}

	for (i = 0; i < count; i++) {
		struct dirent *dirent;
		char *my_buffer;
		char *my_path;

		dirent = namelist[i];

		if (dirent->d_name[0] == '.' ||
			asprintf(&my_buffer,
				"%s",
				dirent->d_name) < 0) {
			/* asprintf must be last condition so when it */
			/* evaluates my_buffer gets allocated */
			free(namelist[i]);
			continue;
		}

		if (strstr(my_buffer, my_string)) {
			char my_prop_string[15];

			found = true;
			if (asprintf(&my_path,
				"%s/%s",
				my_dir,
				my_buffer) < 0 ) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL MEM Info",
						"Internal"
						" processing problem at"
						" my_link \"%s\", please"
						" check the system for"
						" setup issues.",
						my_buffer);
				free(my_buffer);
				free(namelist[i]);
				continue;
			}

			strcpy(my_prop_string, "/memory-buffer");
			if (check_status_property_okay(fw, my_path,
						my_prop_string,
						DT_PROPERTY_OPAL_STATUS)) {
				found_dimm = true;
				if (get_dimm_property(fw, my_path, false,
					DT_PROPERTY_OPAL_STATUS)) {
					failures++;
				}

				if (get_dimm_property(fw, my_path, false,
					DT_PROPERTY_OPAL_SLOT_LOC)) {
					failures++;
				}

				if (get_dimm_property(fw, my_path, false,
					DT_PROPERTY_OPAL_PART_NUM)) {
					failures++;
				}

				if (get_dimm_property(fw, my_path, false,
					DT_PROPERTY_OPAL_SERIAL_NUM)) {
					failures++;
				}

				if (get_dimm_property(fw, my_path, true,
					DT_PROPERTY_OPAL_MANUFACTURER_ID)) {
					failures++;
				}
			}
			free(my_path);
			free(namelist[i]);
		}
		free(my_buffer);
	}
	free(namelist);

	if (!found) {
		failures++;
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL MEM Info",
				"No MEM devices (memory-buffer) were found"
				" in \"%s\".  Check the system for setup"
				" issues.",
				DT_FS_PATH);
	}

	if (failures) {
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}
}

static int process_mba(
	fwts_framework *fw,
	char *my_string,
	char *my_dir)
{
	int count, i, failures = 0;
	struct dirent **namelist;
	bool found = false;

	count = scandir(my_dir, &namelist, NULL, alphasort);
	if (count < 0) {
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MEM Info",
			"Scan for MEM devices for \"%s\" in \"%s\" is"
			" unable to find any candidates. Check the"
			" installation for the MEM device config for missing"
			" nodes in the device tree if you expect MEM devices.",
			my_string,
			my_dir);
		return FWTS_ERROR;
	}

	for (i = 0; i < count; i++) {
		struct dirent *dirent;
		char *my_buffer;
		char *my_path;

		dirent = namelist[i];

		if (dirent->d_name[0] == '.' ||
			asprintf(&my_buffer,
				"%s",
				dirent->d_name) < 0) {
			/* asprintf must be last condition so when it */
			/* evaluates my_buffer gets allocated */
			free(namelist[i]);
			continue;
		}

		if (strstr(my_buffer, my_string)) {
			found = true;
			if (asprintf(&my_path,
				"%s/%s",
				my_dir,
				my_buffer) < 0 ) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL MEM Info",
						"Internal"
						" processing problem at"
						" my_link \"%s\", please"
						" check the system for"
						" setup issues.",
						my_buffer);
				free(my_buffer);
				free(namelist[i]);
				continue;
			}
			if (process_dimm(fw, "dimm", my_path)) {
				failures++;
			}
			free(my_path);
			free(namelist[i]);
		}
		free(my_buffer);
	}
	free(namelist);

	if (!found) {
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MEM Info",
			"Failed to find any device"
			" tree nodes for mba@X from \"%s\"."
			" Check the installation "
			"for the MEM device config for missing nodes"
			" in the device tree if you expect MEM devices.",
			my_dir);
	}

	if ((!found) || (failures)) {
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}

}

static int get_linux_mem_devices(fwts_framework *fw)
{
	int count, i, failures = 0;
	struct dirent **namelist = NULL;
	bool found = false;

	count = scandir(DT_FS_PATH, &namelist, NULL, alphasort);
	if (count < 0) {
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MEM Info",
			"Scan for MEM devices in '%s' is unable to find any "
			"candidates. Check the installation "
			"for the MEM device config for missing nodes"
			" in the device tree if you expect MEM devices.",
			DT_FS_PATH);
		return FWTS_ERROR;
	}

	fwts_log_nl(fw);
	fwts_log_info(fw, "STARTING checks of MEM devices");

	for (i = 0; i < count; i++) {
		struct dirent *dirent;
		char *mem_buffer;
		char *mba_path;

		dirent = namelist[i];

		mem_buffer = NULL;
		if (dirent->d_name[0] == '.' ||
			asprintf(&mem_buffer,
				"%s",
				dirent->d_name) < 0) {
			/* asprintf must be last condition so when it */
			/* evaluates mem_buffer gets allocated */
			free(namelist[i]);
			continue;
		}

		if (strstr(mem_buffer, "memory-buffer")) {
			found = true;
			if (asprintf(&mba_path,
				"%s/%s",
				DT_FS_PATH,
				mem_buffer) < 0 ) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL MEM Info",
						"Internal"
						" processing problem at"
						" mem_buffer \"%s\", please"
						" check the system for"
						" setup issues.",
						mem_buffer);
				free(mem_buffer);
				free(namelist[i]);
				continue;
			}
			if (process_mba(fw, "mba", mba_path)) {
				failures++;
			}
			free(mba_path);
			free(namelist[i]);
		}
		free(mem_buffer);
	}
	free(namelist);

	fwts_log_nl(fw);
	fwts_log_info(fw, "ENDING checks of MEM devices");
	fwts_log_nl(fw);

	if (!found) {
		fwts_skipped(fw, "OPAL MEM Info MEM devices "
				"(memory-buffer@X) found in \"%s\","
				" not applicable for version.",
				DT_FS_PATH);
	}

	if (!found_dimm) {
		fwts_log_nl(fw);
		fwts_skipped(fw, "OPAL MEM Info MEM DIMM devices "
				"(memory-buffer@X) found in \"%s\","
				" not applicable for version.",
				DT_FS_PATH);
	}

	if (failures) {
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}

}

static int mem_info_test1(fwts_framework *fw)
{

	if (get_linux_mem_devices(fw)) {
		/* errors logged earlier */
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}
}

static int mem_info_init(fwts_framework *fw)
{
	if (fw->firmware_type != FWTS_FIRMWARE_OPAL) {
		fwts_skipped(fw,
			"The firmware type detected was not set"
			" to OPAL so skipping the OPAL PCI Info"
			" checks.");
		return FWTS_SKIP;
	} else {
		return FWTS_OK;
	}
}

static fwts_framework_minor_test mem_info_tests[] = {
	{ mem_info_test1, "OPAL MEM Info" },
	{ NULL, NULL }
};

static fwts_framework_ops mem_info_ops = {
	.description = "OPAL MEM Info",
	.init        = mem_info_init,
	.minor_tests = mem_info_tests
};

FWTS_REGISTER_FEATURES("mem_info", &mem_info_ops, FWTS_TEST_ANYTIME,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
		FWTS_FW_FEATURE_DEVICETREE);
