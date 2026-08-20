/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This file was original part of the Linux-ready Firmware Developer Kit
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>

static int maxreadreq_init(fwts_framework *fw)
{
	if (access(FWTS_PCI_DEV_PATH, R_OK) < 0) {
		fwts_log_info(fw, "Could not access %s, skipping test",
			FWTS_PCI_DEV_PATH);
		return FWTS_SKIP;
	}
	return FWTS_OK;
}

/*
 * This test checks if MaxReadReq is set > 128 for non-internal stuff
 * A too low value hurts performance
 */
static int maxreadreq_test1(fwts_framework *fw)
{
	DIR *dirp;
	struct dirent *entry;
	int warnings = 0;

	if ((dirp = opendir(FWTS_PCI_DEV_PATH)) == NULL) {
		fwts_log_warning(fw, "Could not open %s.", FWTS_PCI_DEV_PATH);
		return FWTS_ERROR;
	}

	while ((entry = readdir(dirp)) != NULL) {
		char path[PATH_MAX];
		uint8_t config[256];
		int fd;
		ssize_t n;
		uint8_t offset = 0;
		uint16_t vendor_id;

		if (entry->d_name[0] == '.')
			continue;

		/* Read Config space */
		snprintf(path, sizeof(path), FWTS_PCI_DEV_PATH "/%s/config", entry->d_name);
		if ((fd = open(path, O_RDONLY)) < 0) {
			fwts_log_warning(fw, "Could not open %s PCI config data\n", entry->d_name);
			continue;
		}
		if ((n = read(fd, config, sizeof(config))) < 0) {
			fwts_log_warning(fw, "Could not read %s PCI config data\n", entry->d_name);
			(void)close(fd);
			continue;
		}
		(void)close(fd);

		/* Ignore Host Bridge */
		if ((config[FWTS_PCI_CONFIG_CLASS_CODE] == FWTS_PCI_CLASS_CODE_BRIDGE_CONTROLLER) &&
		    (config[FWTS_PCI_CONFIG_SUBCLASS] == FWTS_PCI_SUBCLASS_CODE_HOST_BRIDGE))
			continue;
		/* Ignore PCI Bridge */
		if ((config[FWTS_PCI_CONFIG_CLASS_CODE] == FWTS_PCI_CLASS_CODE_BRIDGE_CONTROLLER) &&
		    (config[FWTS_PCI_CONFIG_SUBCLASS] == FWTS_PCI_SUBCLASS_CODE_PCI_TO_PCI_BRIDGE))
			continue;
		/* Ignore System Peripheral */
		if ((config[FWTS_PCI_CONFIG_CLASS_CODE] == FWTS_PCI_CLASS_CODE_BASE_SYSTEM_PERIPHERALS) &&
		    (config[FWTS_PCI_CONFIG_SUBCLASS] == FWTS_PCI_SUBCLASS_CODE_OTHER_SYSTEM_PERIPHERAL))
			continue;
		/* Ignore Audio Device */
		if ((config[FWTS_PCI_CONFIG_CLASS_CODE] == FWTS_PCI_CLASS_CODE_MULTIMEDIA_CONTROLLER) &&
		    (config[FWTS_PCI_CONFIG_SUBCLASS] == FWTS_PCI_SUBCLASS_CODE_AUDIO_DEVICE))
			continue;

		/* Ignore Intel's Video Device */
		vendor_id = config[FWTS_PCI_CONFIG_VENDOR_ID] + ((config[FWTS_PCI_CONFIG_VENDOR_ID  + 1]) << 8);
		if ((config[FWTS_PCI_CONFIG_CLASS_CODE] == FWTS_PCI_CLASS_CODE_DISPLAY_CONTROLLER) &&
		    (vendor_id == FWTS_PCI_INTEL_VENDOR_ID))
			continue;

		/* config region too small, do next */
		if (n < FWTS_PCI_CONFIG_TYPE0_CAPABILITIES_POINTER)
			continue;
		offset = config[FWTS_PCI_CONFIG_TYPE0_CAPABILITIES_POINTER];

		/*
		 *  Step through capability structures and
		 *  examine MaxReadReq settings
		 */
		while ((offset != FWTS_PCI_CAPABILITIES_LAST_ID) &&
		       (offset > 0) && ((ssize_t)(offset + sizeof(fwts_pcie_capability)) <= n)) {
			fwts_pcie_capability *cap = (fwts_pcie_capability *)&config[offset];

			if (cap->pcie_cap_id == FWTS_PCI_EXPRESS_CAP_ID) {
				uint32_t max_readreq = 128 << ((cap->device_contrl >> 12) & 0x3);
				if (max_readreq <= 128) {
					fwts_log_warning(fw,
						"MaxReadReq for %s is low (%" PRIu32 ").",
						entry->d_name, max_readreq);
					warnings++;
				}
			}
			offset = cap->next_cap_point;
		}
	}
	(void)closedir(dirp);

	if (warnings > 0) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"LowMaxReadReq",
			"%d devices have low MaxReadReq settings. "
			"Firmware may have configured these too low.",
			warnings);
		fwts_advice(fw,
			"The MaxReadRequest size is set too low and will affect performance. "
			"It will provide excellent bus sharing at the cost of bus data transfer "
			"rates. Although not a critical issue, it may be worth considering setting "
			"the MaxReadRequest size to 256 or 512 to increase throughput on the PCI "
			"Express bus. Some drivers (for example the Brocade Fibre Channel driver) "
			"allow one to override the firmware settings. Where possible, this BIOS "
			"configuration setting is worth increasing it a little more for better "
			"performance at a small reduction of bus sharing. Note it is safe to "
			"ignore this failure if the register is hardwired to zero per PCIe spec.");
	} else
		fwts_passed(fw, "All devices have MaxReadReq set > 128.");

	return FWTS_OK;
}

static fwts_framework_minor_test maxreadreq_tests[] = {
	{ maxreadreq_test1, "Test firmware settings MaxReadReq for PCI Express devices." },
	{ NULL, NULL },
};

static fwts_framework_ops maxreadreq_ops = {
	.init        = maxreadreq_init,
	.description = "Test firmware has set PCI Express MaxReadReq to a higher value on non-motherboard devices.",
	.minor_tests = maxreadreq_tests
};

FWTS_REGISTER("maxreadreq", &maxreadreq_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)
