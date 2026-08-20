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
#include <sys/ioctl.h>

#include "fwts.h"

#ifdef HAVE_LIBFDT
#include <libfdt.h>
#endif

#ifdef HAVE_ASM_OPAL_PRD_H
#include <asm/opal-prd.h>
#endif

static const char *prd_devnode = "/dev/opal-prd";

static bool prd_present(const int fwts_prd_flags)
{
	return !access(prd_devnode, fwts_prd_flags);
}

#ifdef HAVE_ASM_OPAL_PRD_H
static int prd_dev_query(fwts_framework *fw)
{

	int fd = 0;
	struct opal_prd_info info;

	if ((fd = open(prd_devnode, O_RDWR)) < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL PRD Info",
			"Cannot get data from the OPAL PRD "
			" device interface,"
			" check if opal-prd daemon may be in use "
			"or check your user privileges.");
		return FWTS_ERROR;
	}

	memset(&info, 0, sizeof(info));

	if (ioctl(fd, OPAL_PRD_GET_INFO, &info)) {
		(void)close(fd);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL PRD Info",
			"Cannot get data from the"
			" OPAL PRD device interface.");
		return FWTS_ERROR;
	} else {
		fwts_log_info(fw, "OPAL PRD Version is %lu",
			info.version);
		(void)close(fd);
		return FWTS_OK;
	}
}
#endif

static int prd_service_check(fwts_framework *fw, int *restart)
{
	int rc = FWTS_OK, status = 0;
	char *command;
	char *output = NULL;

	command = "systemctl status opal-prd.service 2>&1";
	status = fwts_exec2(command, &output);

	if (output) {
		free(output);
		output = NULL;
	}

	switch (status) {
	case -1: /* status when nothing was successful */
		fwts_failed(fw, LOG_LEVEL_HIGH, "OPAL PRD Info",
			"Attempt was made to stop the "
			"opal-prd.service but was not "
			"successful. Try to "
			"\"sudo systemctl stop "
			"opal-prd.service\" and retry.");
		rc = FWTS_ERROR;
		goto out;
	case 0: /* "running" */
		command = "systemctl stop opal-prd.service 2>&1";
		fwts_exec2(command, &output);

		if (output)
			free(output);

		*restart = 1;
		break;
	default:
		break;
	}

out:
	return rc;
}

static int prd_restart(fwts_framework *fw)
{
	int status = 0;
	char *command;
	char *output = NULL;

	command = "systemctl start opal-prd.service 2>&1";
	status = fwts_exec2(command, &output);

	if (output)
		free(output);

	if (status) {
		fwts_log_info(fw, "OPAL PRD service (opal-prd.service)"
			" was restarted after stopping it to allow "
			"checks.  Please re-check since processing "
			"was not able to be confirmed, "
			"\"sudo systemctl status opal-prd.service\"");
	} else {
		fwts_log_info(fw, "OPAL PRD service (opal-prd.service)"
			" was restarted after stopping it to allow "
			"checks.  This is informational only, "
			"no action needed.");
	}

	return status; /* ignored by caller */
}

static int prd_info_test1(fwts_framework *fw)
{

	int restart = 0;

	if (prd_service_check(fw, &restart)) {
		/* failures logged in subroutine */
		return FWTS_ERROR;
	}

	if (!prd_present(R_OK | W_OK)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL PRD Info",
			"Cannot read and write to the OPAL PRD"
			" device interface %s, check your system"
			" installation and user privileges.", prd_devnode);
		return FWTS_ERROR;
	}

#ifdef HAVE_ASM_OPAL_PRD_H
	if (prd_dev_query(fw)) {
		/* failures logged in subroutine */
		return FWTS_ERROR;
	}
#endif

	if (restart) {
		prd_restart(fw); /* ignore rc */
	}

	fwts_passed(fw, "OPAL PRD info passed.");

	return FWTS_OK;
}

static int prd_info_init(fwts_framework *fw)
{
	if (fw->fdt) {
#ifdef HAVE_LIBFDT
		int node;
		node = fdt_path_offset(fw->fdt,
			"/ibm,opal/diagnostics");
		if (node >= 0) {
			if (!fdt_node_check_compatible(fw->fdt, node,
				"ibm,opal-prd")) {
				return FWTS_OK;
			} else {
				return FWTS_SKIP;
			}
		}
#endif
	} else {
		fwts_log_info(fw, "The OPAL PRD device tree node is not"
			" able to be detected so skipping the prd_info"
			" test.  There may be tools missing such as"
			" libfdt-dev or dtc, check that the packages"
			" are installed and re-build if needed."
			" If this condition persists try running the"
			" dt_base test to further diagnose. If dt_base"
			" test is not available this is probably a"
			" setup problem.");
		return FWTS_SKIP;
	}

	/* only run test when fdt node is confirmed */
	return FWTS_SKIP;
}

static fwts_framework_minor_test prd_info_tests[] = {
	{ prd_info_test1, "OPAL Processor Recovery Diagnostics Info" },
	{ NULL, NULL }
};

static fwts_framework_ops prd_info_ops = {
	.description = "OPAL Processor Recovery Diagnostics Info",
	.init        = prd_info_init,
	.minor_tests = prd_info_tests
};

FWTS_REGISTER_FEATURES("prd_info", &prd_info_ops, FWTS_TEST_EARLY,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
		FWTS_FW_FEATURE_DEVICETREE)
