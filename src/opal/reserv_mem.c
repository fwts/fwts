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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <inttypes.h>

#include "fwts.h"

#include <libfdt.h>

#define CONFIG_FILENAME "/usr/local/share/fwts/platform.conf"
#define MAXBUF 1024
#define DELIM "="

static const char *root_node = "/";

static const char *reserv_mem_node = "/reserved-memory/";

typedef struct reserve_region {
	const char *name;
	uint64_t start;
	uint64_t len;
} reserve_region_t;

/* Update new image names here */

typedef struct plat_config {
	uint64_t occ_common;
	uint64_t homer;
	uint64_t slw;
} plat_config_t;

static bool skip = false;

static int get_config(fwts_framework *fw,
	char *filename,
	plat_config_t *configstruct)
{
	FILE *file;
	char *p;
	char line[MAXBUF];

	file = fopen(filename, "r");
	if (!file) {
		skip = true;
		fwts_log_error(fw, "Platform config file doesn't exist, "
				"skipping region size validation check");
		return FWTS_SKIP;
	}

	while (fgets(line, sizeof(line), file) != NULL) {
		char *cfline;
		uint64_t value;

		cfline = strstr((char *)line, DELIM);
		if (!cfline) {
			fwts_log_warning(fw, "Unexpected line format: %s", line);
			continue;
		}

		cfline = cfline + strlen(DELIM);
		value = strtoul(cfline, &p, 16);

		if (strstr(line, "homer"))
			configstruct->homer = value;
		else if (strstr(line, "occ-common-area"))
			configstruct->occ_common = value;
		else if (strstr(line, "slw-image"))
			configstruct->slw = value;
	}
	fclose(file);

	return FWTS_OK;
}

static char *make_message(const char *fmt, ...)
{
	char *p;
	const size_t size = 128;
	va_list ap;

	p = malloc(size);
	if (!p)
		return NULL;

	va_start(ap, fmt);
	vsnprintf(p, size, fmt, ap);
	va_end(ap);
	return p;
}

static int reserv_mem_init(fwts_framework *fw)
{
	if (fw->firmware_type != FWTS_FIRMWARE_OPAL) {
		fwts_skipped(fw,
			"The firmware type detected was non OPAL "
			"so skipping the OPAL Reserve Memory DT checks.");
		return FWTS_SKIP;
	}

	/* On an OPAL based system Device tree should be present */
	if (!fw->fdt) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoDeviceTree",
			"Device tree not found");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int reserv_mem_limits_test(fwts_framework *fw)
{
	bool ok = true;
	const char *region_names;
	const uint64_t *ranges;
	reserve_region_t *regions;
	int  offset, len = 0, nr_regions, rc, j;
	plat_config_t configstruct = {0, 0, 0};

	get_config(fw, CONFIG_FILENAME, &configstruct);

	offset = fdt_path_offset(fw->fdt, root_node);
	if (offset < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
			"DT root node %s is missing", root_node);
		return FWTS_ERROR;
	}

	/* Get the number of memory reserved regions */
	nr_regions = fwts_dt_stringlist_count(fw, fw->fdt, offset,
				"reserved-names");
	if (nr_regions < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNoRegions",
			"DT No regions");
		return FWTS_ERROR;
	}

	/* Check for the reservd-names property */
	region_names = (const char *)fdt_getprop(fw->fdt, offset,
					"reserved-names", &len);
	if (!region_names) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyMissing",
			"DT Property reserved-names is missing %s",
			fdt_strerror(len));
		return FWTS_ERROR;
	}

	regions = malloc(nr_regions*sizeof(reserve_region_t));
	if (!regions) {
		fwts_skipped(fw,
			"Unable to allocate memory "
			"for reserv_region_t structure");
		return FWTS_SKIP;
	}

	for (j = 0; j < nr_regions; j++) {
		regions[j].name = strdup(region_names);
		region_names += strlen(regions[j].name) + 1;
	}

	/* Check for the reserved-ranges property */
	ranges = fdt_getprop(fw->fdt, offset, "reserved-ranges", &len);
	if (!ranges) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyMissing",
			"DT Property reserved-ranges is missing %s",
			fdt_strerror(len));
		rc = FWTS_ERROR;
		goto out_free_regions;
	}

	for (j = 0; j < nr_regions; j++) {
		regions[j].start = (uint64_t)be64toh(ranges[2 * j]);
		regions[j].len = (uint64_t)be64toh(ranges[2 * j + 1]);
		fwts_log_info(fw, "Region name %80s"
			" start: 0x%08" PRIx64 ", len: 0x%08" PRIx64 "\n",
			regions[j].name, regions[j].start, regions[j].len);
	}

	offset = fdt_path_offset(fw->fdt, reserv_mem_node);
	if (offset < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
			"reserve memory node %s is missing", reserv_mem_node);
		rc = FWTS_ERROR;
		goto out_free_regions;
	}

	/* Validate different cases */
	for (j = 0; j < nr_regions; j++) {
		char *buf = NULL;

		/* Check for zero offset's */
		if (regions[j].start == 0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "ZeroStartAddress",
				"memory region got zero start address");
			ok = false;
		}

		/* Check for zero region sizes */
		if (regions[j].len == 0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "ZeroRegionSize",
				"memory region got zero size");
			ok = false;
		}

		/* Form the reserved-memory sub nodes for all the regions*/
		if (!strstr(regions[j].name, "@"))
			buf = make_message("%s%s@%lx", reserv_mem_node,
					regions[j].name, regions[j].start);
		else
			buf = make_message("/%s/%s", reserv_mem_node,
					regions[j].name);

		if (!buf) {
			fwts_skipped(fw,
				"Unable to allocate memory for buffer");
			rc = FWTS_SKIP;
			goto out_free_regions;
		}

		/* Check all nodes got created for all the sub regions */
		offset = fdt_path_offset(fw->fdt, buf);
		if (offset < 0) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
				"reserve memory region node %s is missing",
				buf);
			ok = false;
		}
		free(buf);

		if (skip)
			continue;

		/* Validate different Known image fixed sizes here */
		if (strstr(regions[j].name, "homer-image")) {
			if (regions[j].len != configstruct.homer) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"ImageSizeMismatch",
					"Mismatch in homer-image size, "
					"expected: 0x%" PRIx64 ", actual: 0x%" PRIx64,
					configstruct.homer, regions[j].len);
				ok = false;
			} else
				fwts_log_info(fw,
					"homer-image size is validated");
		}

		if (strstr(regions[j].name, "slw-image")) {
			if (regions[j].len != configstruct.slw) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"ImageSizeMismatch",
					"Mismatch in slw-image size, "
					"expected: 0x%" PRIx64 ", actual: 0x%" PRIx64,
					configstruct.slw, regions[j].len);
				ok = false;
			} else
				fwts_log_info(fw,
					"slw-image size is validated");
		}

		if (strstr(regions[j].name, "occ-common-area")) {
			if (regions[j].len != configstruct.occ_common) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"ImageSizeMismatch",
					"Mismatch in occ-common-area size, "
					"expected: 0x%" PRIx64 ", actual: 0x%" PRIx64,
					configstruct.occ_common,
					regions[j].len);
				ok = false;
			} else
				fwts_log_info(fw,
					"occ-common-area size is validated");
		}
	}

	if (ok) {
		rc = FWTS_OK;
		fwts_passed(fw, "Reserved memory validation tests passed");
	} else {
		rc = FWTS_ERROR;
		fwts_failed(fw, LOG_LEVEL_HIGH, "ReservMemTestFail",
			"One or few Reserved Memory DT"
			" validation tests failed");
	}

out_free_regions:
	free(regions);
	return rc;
}

static fwts_framework_minor_test reserv_mem_tests[] = {
	{ reserv_mem_limits_test, "OPAL Reserved memory DT Validation Info" },
	{ NULL, NULL }
};

static fwts_framework_ops reserv_mem_tests_ops = {
	.description = "OPAL Reserved memory DT Validation Test",
	.init        = reserv_mem_init,
	.minor_tests = reserv_mem_tests
};

FWTS_REGISTER_FEATURES("reserv_mem", &reserv_mem_tests_ops, FWTS_TEST_EARLY,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
		FWTS_FW_FEATURE_DEVICETREE)
