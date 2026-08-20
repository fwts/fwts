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

static int get_xscom_property(fwts_framework *fw,
			char *my_path,
			bool hex,
			char *property)
{
	int failures = 0;
	char *prop_string = strstr(my_path, "/xscom");

	if (prop_string) {
		int prop_len = 0;
		int node = fdt_path_offset(fw->fdt, prop_string);

		if (node >= 0) {
			const char *prop_buf;

			prop_buf = fdt_getprop(fw->fdt, node,
					property,
					&prop_len);
			if ((prop_len > 0) && (prop_buf)) {
				fwts_log_nl(fw);
				if ((hex) && (prop_len == 4)) {
					fwts_passed(fw,
						"OPAL CPU Info Property of"
						"\"%s\" for \"%s\" is "
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
						"OPAL CPU Info Property of"
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
						"OPAL CPU Info",
						"Property of "
						"\"%s\" for \"%s\" is not"
						" properly formatted."
						" Check your installation"
						" for proper device tree "
						"nodes.",
						property,
						my_path);
				}
			} else {
				fwts_log_nl(fw);
				fwts_skipped(fw, "OPAL CPU check for "
					"property of \"%s\" and path of \"%s\""
					" not applicable to version.",
					property, my_path);
			}
		} else {
			failures++;
			fwts_log_nl(fw);
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL CPU Info",
				"Failed to get node for xscom"
				" \"%s/%s\", please check the system for"
				" setup issues.",
				my_path, property);
		}
	} else {
		failures++;
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL CPU Info",
			"Internal processing problem at strstr"
			" my_path of \"%s\" for \"%s\", please check the"
			" system for setup issues.",
			my_path,
			property);
	}
	if (failures) {
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}
}

static int get_linux_xscom_devices(fwts_framework *fw)
{
	int count, i, failures = 0;
	struct dirent **namelist;
	bool found = false;

	count = scandir(DT_FS_PATH, &namelist, NULL, alphasort);
	if (count < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL CPU Info",
			"Scan for CPU devices in '%s' is unable to find any"
			" candidates. Check the installation for the CPU"
			" device config for missing nodes in the device tree"
			" (xscom@X) if you expect CPU devices.",
			DT_FS_PATH);
		return FWTS_ERROR;
	}

	fwts_log_nl(fw);
	fwts_log_info(fw, "STARTING checks of CPU'S");

	for (i = 0; i < count; i++) {
		struct dirent *dirent;
		char *cpus;
		char *xscom_path;

		dirent = namelist[i];

		if (dirent->d_name[0] == '.' ||
			asprintf(&cpus,
				"%s",
				dirent->d_name) < 0) {
			/* asprintf must be last condition so when it */
			/* evaluates cpus gets allocated */
			free(namelist[i]);
			continue;
		}

		if (strstr(cpus, "xscom")) {
			found = true;
			if (asprintf(&xscom_path,
				"%s/%s",
				DT_FS_PATH,
				cpus) < 0 ) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"OPAL CPU Info",
					"Internal processing problem at"
					" cpus \"%s\", please"
					" check the system for"
					" setup issues.",
					cpus);
				free(cpus);
				free(namelist[i]);
				continue;
			}
			if (get_xscom_property(fw, xscom_path, false,
						DT_PROPERTY_OPAL_SLOT_LOC)) {
				failures++;
			}

			if (get_xscom_property(fw, xscom_path, false,
						DT_PROPERTY_OPAL_PART_NUM)) {
				failures++;
			}

			if (get_xscom_property(fw, xscom_path, false,
						DT_PROPERTY_OPAL_SERIAL_NUM)) {
				failures++;
			}

			if (get_xscom_property(fw, xscom_path, false,
						DT_PROPERTY_OPAL_VENDOR)) {
				failures++;
			}

			if (get_xscom_property(fw, xscom_path, false,
						DT_PROPERTY_OPAL_BOARD_INFO)) {
				failures++;
			}
			fwts_log_nl(fw);
			free(namelist[i]);
		}
		free(cpus);
	}
	free(namelist);

	fwts_log_nl(fw);
	fwts_log_info(fw, "ENDING checks of CPU's");
	fwts_log_nl(fw);

	if (!found) {
		failures++;
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL CPU Info",
			"No CPU devices (xscom@X) were found in \"%s\"."
			" Check the system for setup issues.",
			DT_FS_PATH);
	}

	if (failures) {
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}

}

static int cpu_info_test1(fwts_framework *fw)
{
	if (get_linux_xscom_devices(fw)) {

		/* errors logged earlier */
		return FWTS_ERROR;
	} else {
		return FWTS_OK;
	}
}

static int cpu_info_init(fwts_framework *fw)
{
	if (fw->firmware_type != FWTS_FIRMWARE_OPAL) {
		fwts_skipped(fw,
			"The firmware type detected was not set"
			" to OPAL so skipping the OPAL CPU Info"
			" checks.");
		return FWTS_SKIP;
	} else {
		return FWTS_OK;
	}
}

static fwts_framework_minor_test cpu_info_tests[] = {
	{ cpu_info_test1, "OPAL CPU Info" },
	{ NULL, NULL }
};

static fwts_framework_ops cpu_info_ops = {
	.description = "OPAL CPU Info",
	.init        = cpu_info_init,
	.minor_tests = cpu_info_tests
};

FWTS_REGISTER_FEATURES("cpu_info", &cpu_info_ops, FWTS_TEST_ANYTIME,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
		FWTS_FW_FEATURE_DEVICETREE);
