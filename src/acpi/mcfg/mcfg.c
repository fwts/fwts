/*
 * Copyright (C) 2010-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>

static fwts_list *memory_map_list;
static fwts_acpi_table_info *mcfg_table;
acpi_table_init(MCFG, &mcfg_table)

static int compare_config_space(
	fwts_framework *fw,
	const fwts_acpi_mcfg_configuration *config)
{
	uint8_t *mapped_config_space;
	uint8_t config_space[16];
	size_t page_size, n;
	bool match;
	char path[PATH_MAX];
	FILE *fp;
	int i;

	page_size = fwts_page_size();

	/*
 	 * Sanity check on first config, this is enough to
	 * see if MMIO base is OK or not
	 */
	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/%4.4" PRIx16 ":00:00.0/config",
		config->pci_segment_group_number);

	if ((fp = fopen(path, "r")) == NULL) {
		fwts_warning(fw, "Could not open %s.", path);
		return FWTS_ERROR;
	}
	n = fread(config_space, 1, sizeof(config_space), fp);
	(void)fclose(fp);
	if (n != sizeof(config_space)) {
		fwts_warning(fw, "Could only read %zd bytes from %s, expecting %zd.", n, path, sizeof(config_space));
		return FWTS_ERROR;
	}

	if ((mapped_config_space = fwts_mmap(config->base_address, page_size)) == FWTS_MAP_FAILED) {
		char *data;

		if ((data = fwts_get("/sys/kernel/security/lockdown")) != NULL) {
			if (strstr(data, "[none]") == NULL) {
				free(data);
				fwts_log_info(fw, "Kernel is in lockdown mode. Aborted.");
				fwts_log_info(fw, "Please unlock the kernel before testing.");
				fwts_log_info(fw, "Make sure you disable secureboot and disable "
					"the kernel lockdown, (by kernel parameter lockdown=None).");
				return FWTS_ABORTED;
			}
		}
		free(data);

		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MMapUnmappable",
			"Cannot mmap PCI config space at 0x%" PRIx64 ".",
			config->base_address);
		return FWTS_ERROR;
	}

	/* We only need to check if just the config space is readable */
	if (fwts_safe_memread(mapped_config_space, sizeof(config_space)) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "PCIUnreadable",
			"Cannot read PCI config space at 0x%" PRIx64 ".",
			config->base_address);
		(void)fwts_munmap(mapped_config_space, page_size);
		return FWTS_ERROR;
	}

	/*
	 * Need to explicitly do byte comparisons on region
	 * memcmp() fails as this can do 64 bit reads
	 */
	for (match = true, i = 0; i < 16; i++) {
		if (config_space[i] != mapped_config_space[i]) {
			match = false;
			break;
		}
	}
	(void)fwts_munmap(mapped_config_space, page_size);

	if (match)
		fwts_passed(fw, "PCI config space verified.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"PCIConfigSpaceBad",
			"PCI config space appears to not work.");

	return FWTS_OK;
}

static int mcfg_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if (memory_map_list)
		fwts_memory_map_table_free(memory_map_list);

	return FWTS_OK;
}

static int mcfg_test1(fwts_framework *fw)
{
	int nr, i;
	fwts_acpi_table_mcfg *mcfg = (fwts_acpi_table_mcfg*)mcfg_table->data;
	fwts_acpi_mcfg_configuration *config;
	bool failed = false;
	ssize_t mcfg_size;
	const char *memory_map_name;

	memory_map_name = fwts_memory_map_name(fw->firmware_type);

	fwts_log_info(fw,
		"This test tries to validate the MCFG table by comparing the first "
		"16 bytes in the MMIO mapped config space with the 'traditional' config "
		"space of the first PCI device (root bridge). The MCFG data is only "
		"trusted if it is marked reserved in the %s",
		memory_map_name);
	fwts_log_nl(fw);

	if ((memory_map_list = fwts_memory_map_table_load(fw)) == NULL) {
		/* Not fatal, just means test will be less comprehensive */
		fwts_log_warning(fw, "No memory map table found");
	} else {
		fwts_memory_map_table_dump(fw, memory_map_list);
		fwts_log_nl(fw);
	}

	mcfg_size = mcfg_table->length;
	mcfg_size -= sizeof(fwts_acpi_table_mcfg);

	if (mcfg_size < 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "MCFGInvalidSize",
			"Invalid MCFG ACPI table size: got %zd bytes expecting more",
			mcfg_size + sizeof(fwts_acpi_table_mcfg));
		fwts_advice(fw,
			"MCFG table must be least %zd bytes (header size) with "
			"multiples of %zd bytes for each MCFG entry.",
			sizeof(fwts_acpi_table_mcfg),
			sizeof(fwts_acpi_mcfg_configuration));
		return FWTS_ERROR;
	}
	nr = mcfg_size / sizeof(fwts_acpi_mcfg_configuration);

	if (!nr) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MCFGNoEntries",
			"No MCFG ACPI table entries");
		return FWTS_ERROR;
	}

	if (mcfg_size != (ssize_t)(nr * sizeof(fwts_acpi_mcfg_configuration))) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "MCFGInvalidSize2",
			"MCFG table is not a multiple of record size");
		return FWTS_ERROR;
	}

	fwts_log_info(fw,
		"MCFG table found, size is %zd bytes (excluding header) (%i entries).",
		mcfg_size, nr);

	if (mcfg == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "MCFGInvalidTable",
			"Invalid MCFG ACPI table");
		return FWTS_ERROR;
	}

	if (memory_map_list == NULL)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MMapUnreadable",
			"Cannot check MCFG MMIO space against memory map table: could not read memory map table.");

	config = &mcfg->configuration[0];
	for (i = 0; i < nr; i++, config++) {
		fwts_log_info_verbatim(fw, "Configuration Entry #%d:", i);
		fwts_log_info_verbatim(fw, "  Base Address  : 0x%" PRIx64, config->base_address);
		fwts_log_info_verbatim(fw, "  Segment       : %" PRIu16, config->pci_segment_group_number);
		fwts_log_info_verbatim(fw, "  Start bus     : %" PRIu8, config->start_bus_number);
		fwts_log_info_verbatim(fw, "  End bus       : %" PRIu8, config->end_bus_number);

		if ((memory_map_list != NULL) &&
		    (!fwts_memory_map_is_reserved(memory_map_list, config->base_address))) {

			fwts_failed(fw, LOG_LEVEL_HIGH, "MCFGMMIONotReserved",
				"MCFG MMIO config space at 0x%" PRIx64
				" is not reserved in the memory map table",
				config->base_address);
			fwts_advice(fw,
				"The PCI Express specification states that the "
				"PCI Express configuration space should "
				"be defined in the MCFG table and *maybe* "
				"optionally defined in the %s if ACPI MCFG is "
				"present. Linux checks if the region is reserved "
				"in the memory map table and will reject the "
				"MMCONFIG if there is a discrepancy between MCFG "
				"and the memory map table for the PCI Express region. "
				"[See arch/x86/pci/mmconfig-shared.c pci_mmcfg_reject_broken()]. "
				"It is recommended that this is defined in the "
				"%s table for Linux.",
				memory_map_name, memory_map_name);
			failed = true;
		}
	}
	if (!failed)
		fwts_passed(fw, "MCFG MMIO config space is reserved in memory map table.");

	return FWTS_OK;
}

#define SINGLE_CONFIG_TABLE_SIZE \
	(sizeof(fwts_acpi_table_mcfg) + sizeof(fwts_acpi_mcfg_configuration))

static int mcfg_test2(fwts_framework *fw)
{
	fwts_acpi_table_mcfg *mcfg = (fwts_acpi_table_mcfg *)mcfg_table->data;

	if (mcfg == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "MCFGInvalidTable",
			"Invalid MCFG ACPI table");
		return FWTS_ERROR;
	}

	/* Need at least one config entry */
	if (mcfg_table->length < SINGLE_CONFIG_TABLE_SIZE) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MCFGNoEntries",
			"No MCFG ACPI table entries");
		return FWTS_ERROR;
	}

	return compare_config_space(fw, &mcfg->configuration[0]);
}

static fwts_framework_minor_test mcfg_tests[] = {
	{ mcfg_test1, "Validate MCFG table." },
	{ mcfg_test2, "Validate MCFG PCI config space." },
	{ NULL, NULL }
};

static fwts_framework_ops mcfg_ops = {
	.description = "MCFG PCI Express* memory mapped config space test.",
	.init        = MCFG_init,
	.deinit      = mcfg_deinit,
	.minor_tests = mcfg_tests
};

FWTS_REGISTER("mcfg", &mcfg_ops, FWTS_TEST_ANYTIME,
	      FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_ACPI  | FWTS_FLAG_SBBR)

#endif
