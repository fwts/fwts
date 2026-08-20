/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This file was originally part of the Linux-ready Firmware Developer Kit
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

#ifdef FWTS_ARCH_INTEL

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASK_4K 	0xfff

/* See https://software.intel.com/content/dam/develop/external/us/en/documents-tps/vt-directed-io-spec.pdf */

/* DMA Remapping Reporting Table (DMAR) */
struct acpi_table_dmar {
	uint8_t head[36];
	uint8_t haw;
	uint8_t flags;
	uint8_t reserved[10];
} __attribute__((packed));

#define DMAR_HEADER_SIZE sizeof(struct acpi_table_dmar)

struct acpi_dmar_entry_header {
	uint16_t     type;
	uint16_t     length;
} __attribute__((packed));

enum acpi_dmar_entry_type {
	ACPI_DMAR_DRHD = 0,
	ACPI_DMAR_RMRR,
	ACPI_DMAR_ASTR,
	ACPI_DMAR_ENTRY_COUNT
};

struct acpi_table_drhd {
	struct acpi_dmar_entry_header header;
	uint8_t      flags;  	/* BIT0: INCLUDE_ALL */
	uint8_t      reserved;
	uint16_t     segment;
	uint64_t     address; 	/* register base address for this drhd */
} __attribute__ ((packed));

struct acpi_table_rmrr {
	struct acpi_dmar_entry_header header;
	uint16_t     reserved;
	uint16_t     segment;
	uint64_t     base_address;
	uint64_t     end_address;
} __attribute__ ((packed));

enum acpi_dev_scope_type {
	ACPI_DEV_ENDPOINT = 0x01,
	ACPI_DEV_P2PBRIDGE,
	ACPI_DEV_IOAPIC,
	ACPI_DEV_HPET,
	ACPI_DEV_ENTRY_COUNT
};

struct acpi_dev_scope {
	uint8_t      dev_type;
	uint8_t      length;
	uint16_t     reserved;
	uint8_t      enumeration_id;
	uint8_t      start_bus;
} __attribute__((packed));

struct acpi_pci_path {
	uint8_t      dev;
	uint8_t      fn;
} __attribute__((packed));

#define MIN_SCOPE_LEN (sizeof(struct acpi_pci_path) + \
	sizeof(struct acpi_dev_scope))

/*
 * = -1, no such device
 * = 0, normal pci device
 * = 1, pci bridge, sec_bus gets set
 */
static int read_pci_device_secondary_bus_number(const uint8_t seg,
	const uint8_t bus, const uint8_t dev,
	const uint8_t fn, uint8_t *sec_bus)
{
	FILE *file;
	char path[PATH_MAX];
	char configs[64];
	size_t count;

	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/%04x:%02x:%02x.%" PRIu8 "/config",
		seg, bus, dev, fn);
	if ((file = fopen(path, "r")) == NULL)
		return -1;

	count = fread(configs, sizeof(char), 64, file);
	(void)fclose(file);

	if (count < 64)
		return -1;

	/* header type is at 0x0e */
	if ((configs[0xe] & 0x7f) != 1) /* not a pci bridge */
		return 0;
	*sec_bus = configs[0x19]; 	/* secondary bus number */
	return 1;
}

static int acpi_parse_one_dev_scope(fwts_framework *fw,
	struct acpi_dev_scope *scope, const uint16_t seg)
{
	struct acpi_pci_path *path;
	int count;
	uint8_t bus;
	uint8_t sec_bus = 0;
	int dev_type;

	if (scope->length < MIN_SCOPE_LEN) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidDevScope",
			"Invalid device scope entry.");
		return FWTS_ERROR;
	}

	if (scope->dev_type >= ACPI_DEV_ENTRY_COUNT) {
		fwts_warning(fw, "Unknown device scope type.");
		return FWTS_ERROR;
	}

	if (scope->dev_type > ACPI_DEV_P2PBRIDGE) {
		fwts_log_info(fw, "Unknown device scope type, "
			"the test case should be fixed.");
		return FWTS_ERROR;
	}

	bus = scope->start_bus;
	count = (scope->length - sizeof(struct acpi_dev_scope))
		/sizeof(struct acpi_pci_path);
	path = (struct acpi_pci_path *)(scope + 1);

	if (!count)
		goto error;
	dev_type = 1;
	while (count) {
		if (dev_type <= 0) /* last device isn't a pci bridge */
			goto error;
		dev_type = read_pci_device_secondary_bus_number(seg, bus,
			path->dev, path->fn, &sec_bus);
		if (dev_type < 0) {	/* no such device */
			fwts_warning(fw, "PCI device %04Xh:%02Xh:%02Xh.%02Xh is not found.",
				     seg, bus, path->dev, path->fn);
			goto error;
		}
		path++;
		count--;
		bus = sec_bus;
	}

	if ((scope->dev_type == ACPI_DEV_ENDPOINT && dev_type > 0) ||
		(scope->dev_type == ACPI_DEV_P2PBRIDGE && dev_type == 0)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DevScopeTypeNoMatch",
			"Device scope type not match.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
error:
	fwts_failed(fw, LOG_LEVEL_MEDIUM, "DevScopeDevNotFound",
		"Device scope device not found.");
	return FWTS_ERROR;
}

static int acpi_parse_dev_scope(fwts_framework *fw,
	uint8_t *start, const uint8_t *end, const uint16_t seg)
{
	while (start < end) {
		struct acpi_dev_scope *scope = (struct acpi_dev_scope *)start;
		int ret = acpi_parse_one_dev_scope(fw, scope, seg);
		if (ret)
			return ret;
		start += scope->length;
	}
	return FWTS_OK;
}

static int acpi_parse_one_drhd(fwts_framework *fw,
	struct acpi_dmar_entry_header *header)
{
	struct acpi_table_drhd *drhd = (struct acpi_table_drhd*)header;

	if (drhd->address & MASK_4K) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidDRHDRegAddr",
			"Invalid drhd register address.");
		return FWTS_ERROR;
	}
	if (drhd->flags & 1) {
		static int include_all;

		if (include_all == 1) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "MultipleDRHDSFlag",
				"Multiple drhds have include_all flag set.");
			return FWTS_ERROR;
		}
		include_all = 1;
	} else {
		return acpi_parse_dev_scope(fw, (uint8_t *)(drhd + 1),
			((uint8_t *)drhd) + header->length, drhd->segment);
	}
	return FWTS_OK;
}

static int acpi_parse_one_rmrr(fwts_framework *fw,
	struct acpi_dmar_entry_header *header)
{
	struct acpi_table_rmrr *rmrr = (struct acpi_table_rmrr *)header;

	if ((rmrr->base_address & MASK_4K)
		|| (rmrr->end_address < rmrr->base_address)
		|| ((rmrr->end_address - rmrr->base_address + 1) & MASK_4K)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidRMRRRangeAddr",
			"Invalid rmrr range address.");
		return FWTS_ERROR;
	}
	return acpi_parse_dev_scope(fw, (uint8_t *)(rmrr + 1),
		((uint8_t *)rmrr) + header->length, rmrr->segment);
}

static int dmar_acpi_table_check(fwts_framework *fw)
{
	uint8_t *table_ptr;
	struct acpi_dmar_entry_header *header;
	fwts_acpi_table_info *table;
	int failed = 0;

	if (fwts_acpi_find_table(fw, "DMAR", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot load ACPI table.");
		return FWTS_ERROR;
	}
	if (table == NULL) {
		fwts_skipped(fw, "No DMAR table. This is not necessarily a "
			"failure as most systems do not have this table.");
		return FWTS_SKIP;
	}

	table_ptr = (uint8_t*)table->data;
	if (table->length <= DMAR_HEADER_SIZE) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "InvalidDMAR",
			"Invalid DMAR ACPI table.");
		return FWTS_ERROR;
	}

	header = (struct acpi_dmar_entry_header *)(table_ptr+DMAR_HEADER_SIZE);
	while ((unsigned long)header <
	       (unsigned long)(table_ptr + table->length)) {
		if ((header->type == ACPI_DMAR_DRHD) &&
		    (acpi_parse_one_drhd(fw, header) != FWTS_OK)) {
			failed++;
			break;
		}
		if ((header->type == ACPI_DMAR_RMRR) &&
		    (acpi_parse_one_rmrr(fw, header) != FWTS_OK)) {
			failed++;
			break;
		}
		header = (struct acpi_dmar_entry_header *)
				(((char *)header) + header->length);
	}

	if (!failed)
		fwts_passed(fw, "DMAR ACPI table has passed test.");

	return FWTS_OK;
}

static void acpiinfo_check(fwts_framework *fw,
	char *line, int repeated, char *prevline, void *private, int *errors)
{
	FWTS_UNUSED(repeated);
	FWTS_UNUSED(prevline);
	FWTS_UNUSED(private);
	FWTS_UNUSED(errors);

        if (strstr(line, "DMAR:[fault reason"))
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DMARError",
		"Found DMAR error: %s", line);
}

static int dmar_test1(fwts_framework *fw)
{
	if (dmar_acpi_table_check(fw) == FWTS_OK) {
		fwts_list *klog;
		int errors = 0;

		if ((klog = fwts_klog_read()) == NULL) {
                	fwts_log_error(fw, "Cannot read kernel log.");
                	return FWTS_ERROR;
        	}
		if (fwts_klog_scan(fw, klog, acpiinfo_check,
			NULL, NULL, &errors)) {
                	fwts_log_error(fw, "Failed to scan kernel log.");
			fwts_klog_free(klog);
                	return FWTS_ERROR;
		}
		if (errors == 0)
			fwts_passed(fw, "Found no DMAR errors in kernel log.");
		fwts_klog_free(klog);
        }

	return FWTS_OK;
}

static fwts_framework_minor_test dmar_tests[] = {
	{ dmar_test1, "DMA Remapping test." },
	{ NULL, NULL }
};

static fwts_framework_ops dmar_ops = {
	.description = "DMA Remapping (VT-d) test.",
	.minor_tests = dmar_tests
};

FWTS_REGISTER("dmar", &dmar_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI | FWTS_FLAG_ROOT_PRIV)

#endif
