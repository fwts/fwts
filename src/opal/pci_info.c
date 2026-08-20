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

#include <pci/pci.h>
#include <libfdt.h>

static struct pci_access *pcia = NULL;
static struct pci_dev *dev;

static int pci_get_dev_info(fwts_framework *fw,
	char *property,
	char *pci_dt_path,
	char *sys_slot,
	const char *pci_slot_buf,
	char *pci_domain)
{
	char namebuf[PATH_MAX], *device_name;
	char vendorbuf[PATH_MAX], *vendor_name;
	char classbuf[PATH_MAX], *class_name;
	char *tmp_pci_domain;
	bool found = false;

	for (dev = pcia->devices; dev; dev = dev->next) {
		unsigned int pin;

		pci_fill_info(dev,
			PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
		pin = pci_read_byte(dev, PCI_INTERRUPT_PIN);
		if (asprintf(&tmp_pci_domain,
			"%04x:%02x:%02x.%d",
			dev->domain,
			dev->bus,
			dev->dev,
			dev->func) < 0) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL PCI Info",
						"Internal"
						" processing problem"
						" at tmp_pci_domain,"
						" please check the system"
						" for setup issues.");
				free(tmp_pci_domain);
				return FWTS_ERROR;
		} else {
			if (strstr(pci_domain, tmp_pci_domain)) {
				found = true;
				device_name = pci_lookup_name(pcia,
						namebuf,
						sizeof(namebuf),
						PCI_LOOKUP_DEVICE,
						dev->vendor_id,
						dev->device_id);
				vendor_name = pci_lookup_name(pcia,
						vendorbuf,
						sizeof(vendorbuf),
						PCI_LOOKUP_VENDOR,
						dev->vendor_id,
						dev->device_id);
				class_name = pci_lookup_name(pcia,
						classbuf,
						sizeof(classbuf),
						PCI_LOOKUP_CLASS,
						dev->device_class);
				fwts_advice(fw,
					"Property \"%s\" of \"%s\" from device"
					" tree node %s%s is \"%s\","
					" device_name=\"%s\", device_id=%04x,"
					" vendor_name=\"%s\", vendor_id=%04x,"
					" device_class_name=\"%s\","
					" device_class=%04x,"
					" irq=%d (pin %d), base0=%lx.  Please"
					" verify the data matches what you"
					" desire.",
					property, sys_slot, DT_FS_PATH,
					pci_dt_path, pci_slot_buf,
					device_name, dev->device_id,
					vendor_name, dev->vendor_id,
					class_name, dev->device_class,
					dev->irq, pin,
					(long) dev->base_addr[0]);
				fwts_infoonly(fw);
			}
			free(tmp_pci_domain);
		}
	}
	if (!found) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL PCI Info",
				"A match was not found"
				" for \"%s\" from the libpci-dev tools."
				" Check the \"lspci\" output to review"
				" if there is a setup issue."
				" If the \"lscpi\" output looks good"
				" then report a bug in the libpci-dev tools.",
				pci_domain);
		fwts_advice(fw,
			"Property \"%s\" of \"%s\" from device"
			" tree node %s%s is \"%s\".  Please"
			" verify the data matches what you desire.",
			property, sys_slot, DT_FS_PATH,
			pci_dt_path, pci_slot_buf);
		fwts_infoonly(fw);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int pci_get_slot_info(fwts_framework *fw,
	char *property,
	char *pci_dt_path,
	char *sys_slot,
	char *pci_domain)
{
	int node, pci_slot_len;

	node = fdt_path_offset(fw->fdt,
			pci_dt_path);
	if (node >= 0) {
		const char *pci_slot_buf;

		pci_slot_buf = fdt_getprop(fw->fdt, node,
			property, &pci_slot_len);
		if (pci_slot_buf) {
			fwts_log_nl(fw);
			if (pci_get_dev_info(fw,
					property,
					pci_dt_path,
					sys_slot,
					pci_slot_buf,
					pci_domain)) {
				return FWTS_ERROR;
			}
		} else {
			fwts_log_nl(fw);
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"OPAL PCI Info",
					"Internal processing problem"
					" at pci_slot_buf for \"%s\""
					" for \"%s%s\", please"
					" check the system for setup"
					" issues.",
					property,
					DT_FS_PATH,
					pci_dt_path);
			fwts_log_nl(fw);
			return FWTS_ERROR;
		}
	} else {
		fwts_log_nl(fw);
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL PCI Info",
				"The OPAL device tree node is not able"
				" to be detected so skipping the pci_info"
				" test. There may be tools missing such as"
				" libfdt-dev or dtc, check that the packages"
				" are installed and re-build if needed."
				" If this condition persists try running the"
				" dt_base test to further diagnose. If dt_base"
				" test is not available this is probably a"
				" setup problem.  Validation of the PCI"
				" devices in the device tree \"%s\" cannot"
				" be run.",
				DT_FS_PATH);
		fwts_log_nl(fw);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int get_linux_pci_devices(fwts_framework *fw)
{
	int count, i, failures = 0;
	ssize_t bytes = 0;
	struct dirent **namelist;
	bool found = false;

	count = scandir(DT_LINUX_PCI_DEVICES, &namelist, NULL, alphasort);
	if (count < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL PCI Info",
			"Scan for PCI devices in '%s' is unable to find any "
			"candidates. Check the installation "
			"for the PCI device config for missing nodes"
			" in the device tree if you expect PCI devices.",
			DT_LINUX_PCI_DEVICES);
		return FWTS_ERROR;
	}

	fwts_log_nl(fw);
	fwts_log_info(fw, "STARTING checks of PCI devices");

	for (i = 0; i < count; i++) {
		struct dirent *dirent;
		char *pci_slot;
		char *of_node_link;
		char of_node_path[PATH_MAX+1];
		char *sys_slot = NULL;

		memset(of_node_path, 0, sizeof(of_node_path));

		dirent = namelist[i];

		if (dirent->d_name[0] == '.' ||
			asprintf(&pci_slot,
				"%s",
				dirent->d_name) < 0) {
			/* asprintf must be last condition so when it */
			/* evaluates pci_slot gets allocated */
			free(namelist[i]);
			continue;
		}

		if (strstr(pci_slot, "00:00.0")) {
			found = true;
			if (asprintf(&of_node_link,
				"%s/%s/of_node",
				DT_LINUX_PCI_DEVICES,
				pci_slot) < 0 ) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL PCI Info",
						"Internal"
						" processing problem at"
						" pci_slot \"%s\", please"
						" check the system for"
						" setup issues.",
						pci_slot);
				failures++;
				free(pci_slot);
				free(namelist[i]);
				continue;
			}

			bytes = readlink(of_node_link,
					of_node_path,
					sizeof(of_node_path) -1);
			if (bytes < 0) {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL PCI Info",
						"Internal"
						" processing problem at "
						"readlink of_node_link \"%s\","
						" please check the system"
						" for setup issues.",
						of_node_link);
				failures++;
				free(of_node_link);
				free(pci_slot);
				free(namelist[i]);
				continue;
			}
			of_node_path[bytes] = '\0';
			sys_slot = strstr(of_node_path, "/pciex");
			if (sys_slot) {
				int rc = pci_get_slot_info(fw,
						DT_PROPERTY_OPAL_PCI_SLOT,
						sys_slot,
						of_node_link,
						pci_slot);
				if (rc == FWTS_ERROR) {
					failures++;
				}
			} else {
				fwts_log_nl(fw);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
						"OPAL PCI Info",
						"Internal"
						" processing problem at strstr"
						" of_node_path of \"%s\", "
						"please check the system"
						" for setup issues.",
						of_node_path);
				failures++;
				free(of_node_link);
				free(pci_slot);
				free(namelist[i]);
				continue;

			}
			free(of_node_link);
		}
		free(pci_slot);
		free(namelist[i]);
	}
	free(namelist);

	fwts_log_nl(fw);
	fwts_log_info(fw, "ENDING checks of PCI devices");
	fwts_log_nl(fw);

	if (!found) {
		failures++;
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL PCI Info",
				"No root PCI devices (xxxx:00:00.0) were found"
				" in \"%s\". Check the system for setup"
				" issues.",
				DT_LINUX_PCI_DEVICES);
	}

	return failures ? FWTS_ERROR : FWTS_OK;
}

static int pci_info_test1(fwts_framework *fw)
{
	pcia = pci_alloc();
	pci_init(pcia);
	pci_scan_bus(pcia);

	if (!get_linux_pci_devices(fw)) {
		pci_cleanup(pcia);
		return FWTS_OK;
	} else {
		pci_cleanup(pcia);
		return FWTS_ERROR;
	}
}

static int pci_info_init(fwts_framework *fw)
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

static fwts_framework_minor_test pci_info_tests[] = {
	{ pci_info_test1, "OPAL PCI Info" },
	{ NULL, NULL }
};

static fwts_framework_ops pci_info_ops = {
	.description = "OPAL PCI Info",
	.init        = pci_info_init,
	.minor_tests = pci_info_tests
};

FWTS_REGISTER_FEATURES("pci_info", &pci_info_ops, FWTS_TEST_ANYTIME,
	FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
	FWTS_FW_FEATURE_DEVICETREE);
