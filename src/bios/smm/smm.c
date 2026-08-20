/*
 * Copyright (C) 2021-2026 Canonical
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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#define FWTS_INTEL_HOST_PATH	"/sys/bus/pci/devices/0000:00:00.0/config"
#define FWTS_INTEL_IGD_PATH	"/sys/bus/pci/devices/0000:00:02.0"

#define FWTS_GGC		0x50
#define FWTS_TSEGMB		0xB8
#define FWTS_TOLUD		0xBC
#define FWTS_LOCK_FIELD		0x01

static bool smm_has_intel_igd(fwts_framework *fw)
{
	char path[256];
	char *class_code_str;

	FWTS_UNUSED(fw);

	snprintf(path, sizeof(path), "%s/class", FWTS_INTEL_IGD_PATH);
	class_code_str = fwts_get(path);
	if (class_code_str == NULL)
		return false;

	/* Display controller class code starts with 0x03 */
	if (strncmp(class_code_str, "0x03", 4) == 0) {
		free(class_code_str);
		return true;
	}

	free(class_code_str);
	return false;
}

static int smm_init(fwts_framework *fw)
{
	bool intel;

	if (fwts_cpu_is_Intel(&intel) != FWTS_OK) {
		fwts_log_error(fw, "Cannot determine processor type.");
		return FWTS_ERROR;
	}

	if (!intel) {
		fwts_log_info(fw, "The SMM test currently only supports Intel platforms.");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

static int smm_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	return FWTS_OK;
}

static int smm_test0(fwts_framework *fw)
{
	uint8_t config[256];
	bool passed = true;
	int fd;

	if ((fd = open(FWTS_INTEL_HOST_PATH, O_RDONLY)) < 0) {
		fwts_log_warning(fw, "Could not open PCI HOST bridge config data\n");
		return FWTS_ERROR;
	}
	if (read(fd, config, sizeof(config)) < 0) {
		fwts_log_warning(fw, "Could not read PCI HOST bridge config data\n");
		(void)close(fd);
		return FWTS_ERROR;
	}
	(void)close(fd);

	if (!smm_has_intel_igd(fw)) {
		fwts_log_info(fw, "Intel integrated graphics device not found, "
			"skipping client-specific SMM lock checks.");
		fwts_skipped(fw, "SMM test not applicable to this platform.");
		return FWTS_SKIP;
	}

	if ((config[FWTS_GGC] & FWTS_LOCK_FIELD) != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SMMGGCNotLocked",
			"Graphics Control register is not locked (GGCLCK != 1).");
		passed = false;
	}

	if ((config[FWTS_TSEGMB] & FWTS_LOCK_FIELD) != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SMMTSEGNotLocked",
			"TSEG Memory Base register is not locked.");
		passed = false;
	}

	if ((config[FWTS_TOLUD] & FWTS_LOCK_FIELD) != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SMMTOLUDNotLocked",
			"Top of Low Usable DRAM (TOLUD) register is not locked.");
		passed = false;
	}

	if (passed)
		fwts_passed(fw, "No issues found in SMM locks.");

	return FWTS_OK;
}

static fwts_framework_minor_test smm_tests[] = {
	{ smm_test0, "Validate the System management mode (SMM) locks." },
	{ NULL, NULL }
};

static fwts_framework_ops smm_ops = {
	.description = "SMM tests.",
	.init        = smm_init,
	.deinit      = smm_deinit,
	.minor_tests = smm_tests
};

FWTS_REGISTER("smm", &smm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
