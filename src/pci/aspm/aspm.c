/*
 * Copyright (C) 2011-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>

/* PCI Express Capability Structure Fields */
#define FWTS_PCIE_ASPM_SUPPORT_L0_FIELD	0x0400
#define FWTS_PCIE_ASPM_SUPPORT_L1_FIELD	0x0800
#define FWTS_PCIE_ASPM_SUPPORT_FIELD	(FWTS_PCIE_ASPM_SUPPORT_L0_FIELD | FWTS_PCIE_ASPM_SUPPORT_L1_FIELD)

#define FWTS_PCIE_ASPM_CONTROL_L0_FIELD	0x0001
#define FWTS_PCIE_ASPM_CONTROL_L1_FIELD	0x0002
#define FWTS_PCIE_ASPM_CONTROL_FIELD	(FWTS_PCIE_ASPM_CONTROL_L0_FIELD | FWTS_PCIE_ASPM_CONTROL_L1_FIELD)

static int facp_get_aspm_control(fwts_framework *fw)
{
	fwts_acpi_table_info *table;
	fwts_acpi_table_fadt *fadt;

	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		return FWTS_ERROR;
	}
	if (table == NULL)
		return FWTS_ERROR;
	fadt = (fwts_acpi_table_fadt*)table->data;

	if ((fadt->iapc_boot_arch & FWTS_FACP_IAPC_BOOT_ARCH_PCIE_ASPM_CONTROLS) == 0) {
		fwts_log_info(fw, "PCIe ASPM is controlled by Linux kernel.");
	} else {
		fwts_log_info(fw, "PCIe ASPM is not controlled by Linux kernel.");
		fwts_advice(fw,
			"BIOS reports that Linux kernel should not modify ASPM "
			"settings that BIOS configured. It can be intentional "
			"because hardware vendors identified some capability "
			"bugs between the motherboard and the add-on cards.");
	}

	return FWTS_OK;
}

static int pcie_compare_rp_dev_aspm_registers(fwts_framework *fw,
	fwts_pci_device *rp,
	fwts_pci_device *dev)
{
	fwts_pcie_capability *rp_cap, *device_cap;
	uint8_t rp_aspm_cntrl, device_aspm_cntrl;
	uint8_t next_cap;
	int ret = FWTS_OK;
	bool l0s_disabled = false, l1_disabled = false;

	next_cap = rp->config[FWTS_PCI_CONFIG_TYPE1_CAPABILITY_POINTER];
	rp_cap = (fwts_pcie_capability *) &rp->config[next_cap];
	while (rp_cap && rp_cap->pcie_cap_id != FWTS_PCI_EXPRESS_CAP_ID) {
		if (rp_cap->next_cap_point == FWTS_PCI_CAPABILITIES_LAST_ID)
			break;
		next_cap = rp_cap->next_cap_point;
		rp_cap = (fwts_pcie_capability *) &rp->config[next_cap];
	}

	if (!rp_cap) {
		fwts_log_warning(fw, "Could not get  pcie root port capability.");
		return FWTS_ERROR;
	}

	uint8_t device_type = (rp_cap->pcie_cap_reg & FWTS_PCI_EXP_FLAGS_TYPE) >> 4;

	if ((device_type != FWTS_PCI_EXP_TYPE_ROOT_PORT) &&
	    (device_type != FWTS_PCI_EXP_TYPE_DOWNSTREAM_PORT) &&
	    (device_type != FWTS_PCI_EXP_TYPE_PCIE_BRIDGE)) {
		return ret;
	}

	next_cap = dev->config[FWTS_PCI_CONFIG_TYPE1_CAPABILITY_POINTER];
	device_cap = (fwts_pcie_capability *)&dev->config[next_cap];
	while (device_cap && device_cap->pcie_cap_id != FWTS_PCI_EXPRESS_CAP_ID) {
		if (device_cap->next_cap_point == FWTS_PCI_CAPABILITIES_LAST_ID)
			break;
		next_cap = device_cap->next_cap_point;
		device_cap = (fwts_pcie_capability *)&dev->config[next_cap];
	}


	if (((rp_cap->link_cap & FWTS_PCIE_ASPM_SUPPORT_L0_FIELD) >> 10) !=
		(rp_cap->link_contrl & FWTS_PCIE_ASPM_CONTROL_L0_FIELD)) {
		fwts_warning(fw, "RP %04Xh:%02Xh:%02Xh.%02Xh L0s not enabled.",
			 rp->segment, rp->bus, rp->dev, rp->func);
		l0s_disabled = true;
	}

	if (((rp_cap->link_cap & FWTS_PCIE_ASPM_SUPPORT_L1_FIELD) >> 10) !=
		(rp_cap->link_contrl & FWTS_PCIE_ASPM_CONTROL_L1_FIELD)) {
		fwts_warning(fw, "RP %04Xh:%02Xh:%02Xh.%02Xh L1 not enabled.",
			rp->segment, rp->bus, rp->dev, rp->func);
		l1_disabled = true;
	}

	if (!device_cap) {
		fwts_log_warning(fw, "Could not get pcie device capability.");
		return FWTS_ERROR;
	}

	if (((device_cap->link_cap & FWTS_PCIE_ASPM_SUPPORT_L0_FIELD) >> 10) !=
		(device_cap->link_contrl & FWTS_PCIE_ASPM_CONTROL_L0_FIELD)) {
		fwts_warning(fw, "Device %04Xh:%02Xh:%02Xh.%02Xh L0s not enabled.",
			dev->segment, dev->bus, dev->dev, dev->func);
		l0s_disabled = true;
	}

	if (((device_cap->link_cap & FWTS_PCIE_ASPM_SUPPORT_L1_FIELD) >> 10) !=
		(device_cap->link_contrl & FWTS_PCIE_ASPM_CONTROL_L1_FIELD)) {
		fwts_warning(fw, "Device %04Xh:%02Xh:%02Xh.%02Xh L1 not enabled.",
			dev->segment, dev->bus, dev->dev, dev->func);
		l1_disabled = true;
	}

	if (l0s_disabled) {
		fwts_advice(fw,
			"The ASPM L0s low power Link state is optimized for "
			"short entry and exit latencies, while providing "
			"substantial power savings. Disabling L0s of a PCIe "
			"device may increase power consumption, and will  "
			"impact the battery life of a mobile system.");
	}

	if (l1_disabled) {
		fwts_advice(fw,
			"The ASPM L1 low power Link state is optimized for "
			"maximum power savings with longer entry and exit "
			"latencies. Disabling L1 of a PCIe device may "
			"increases power consumption, and will impact the "
			"battery life of a mobile system significantly.");
	}

	rp_aspm_cntrl = rp_cap->link_contrl & FWTS_PCIE_ASPM_CONTROL_FIELD;
	device_aspm_cntrl = device_cap->link_contrl & FWTS_PCIE_ASPM_CONTROL_FIELD;
	if (rp_aspm_cntrl != device_aspm_cntrl) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "PCIEASPM_Unmatched",
			"PCIe ASPM setting was not matched.");
		fwts_log_error(fw, "RP %04Xh:%02Xh:%02Xh.%02Xh has ASPM = %02Xh."
			, rp->segment, rp->bus, rp->dev, rp->func, rp_aspm_cntrl);
		fwts_log_error(fw, "Device %04Xh:%02Xh:%02Xh.%02Xh has ASPM = %02Xh.",
			dev->segment, dev->bus, dev->dev, dev->func, device_aspm_cntrl);
		fwts_advice(fw,
			"ASPM control registers between root port and device "
			"must match in order for ASPM to be active. Unmatched "
			"configuration indicates software did not configure "
			"ASPM correctly and the system is not saving power "
			"at its full potential.");
	} else {
		fwts_passed(fw, "PCIe ASPM setting matched was matched.");
	}

	return ret;
}

static int pcie_check_aspm_registers(fwts_framework *fw)
{
	DIR *dirp;
	struct dirent *entry;
	fwts_list dev_list;
	fwts_list_link *lcur, *ltarget;

	fwts_list_init(&dev_list);

	if ((dirp = opendir(FWTS_PCI_DEV_PATH)) == NULL) {
		fwts_log_warning(fw, "Could not open %s.", FWTS_PCI_DEV_PATH);
		return FWTS_ERROR;
	}
	while ((entry = readdir(dirp)) != NULL) {
		uint8_t bus, dev, func;
		uint16_t segment;

		if (entry->d_name[0] == '.')
			continue;

		if (sscanf(entry->d_name, "%" SCNx16 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8, &segment, &bus, &dev, &func) == 4) {
			int fd;
			char path[PATH_MAX];
			fwts_pci_device *device;

			device = (fwts_pci_device *)calloc(1, sizeof(fwts_pci_device));
			if (device == NULL) {
				fwts_list_free_items(&dev_list, free);
				closedir(dirp);
				return FWTS_ERROR;
			}
			device->segment = segment;
			device->bus = bus;
			device->dev = dev;
			device->func = func;

			snprintf(path, sizeof(path), FWTS_PCI_DEV_PATH "/%s/config", entry->d_name);
			if ((fd = open(path, O_RDONLY)) < 0) {
				fwts_log_warning(fw, "Could not open config from PCI device %s\n", entry->d_name);
				free(device);
				continue;
			}

			if (read(fd, device->config, sizeof(device->config)) < 0) {
				fwts_log_warning(fw, "Could not read config from PCI device %s\n", entry->d_name);
				free(device);
				(void)close(fd);
				continue;
			}
			(void)close(fd);
			fwts_list_append(&dev_list, device);
		}
	}
	closedir(dirp);

	/* Check aspm registers from the list of pci devices */
	for (lcur = dev_list.head; lcur; lcur = lcur->next) {
		fwts_pci_device *cur = (fwts_pci_device *)lcur->data;

		/* Find PCI Bridge (PCIE Root Port) and the attached device  */
		if (cur->config[FWTS_PCI_CONFIG_HEADER_TYPE] & 0x01) {
			for (ltarget = dev_list.head; ltarget; ltarget = ltarget->next) {
				fwts_pci_device *target = (fwts_pci_device *)ltarget->data;
				if (target->segment == cur->segment) {
					if (target->bus == cur->config[FWTS_PCI_CONFIG_TYPE1_SECONDARY_BUS_NUMBER]) {
						pcie_compare_rp_dev_aspm_registers(fw, cur, target);
						break;
					}
				}
			}
		}
	}
	fwts_list_free_items(&dev_list, free);

	return FWTS_OK;
}

static int aspm_check_configuration(fwts_framework *fw)
{
	int ret;

	ret = facp_get_aspm_control(fw);
	if (ret == FWTS_ERROR)
		fwts_skipped(fw, "No valid FACP information present: cannot test ASPM.");

	return ret;
}

static int aspm_pcie_register_configuration(fwts_framework *fw)
{
	int ret;

	ret = pcie_check_aspm_registers(fw);
	if (ret == FWTS_ERROR)
		fwts_skipped(fw, "Cannot sanity check PCIe register configuration.");

	return ret;
}

static fwts_framework_minor_test aspm_tests[] = {
	{ aspm_check_configuration, "PCIe ASPM ACPI test." },
	{ aspm_pcie_register_configuration, "PCIe ASPM registers test." },
	{ NULL, NULL }
};

static fwts_framework_ops aspm_ops = {
	.description = "PCIe ASPM test.",
	.minor_tests = aspm_tests
};

FWTS_REGISTER("aspm", &aspm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
