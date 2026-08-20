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

#include <unistd.h>

#include "fwts.h"

static int fwts_bmc_info_check(
	fwts_framework *fw)
{
	struct fwts_ipmi_rsp fwts_bmc_info;

	fwts_progress(fw, 50);

	if ((fwts_ipmi_base_query(&fwts_bmc_info))) {
		return FWTS_ERROR;
	}

	fwts_log_info(fw, "IPMI Version is %x.%x \n",
		IPMI_DEV_IPMI_VERSION_MAJOR(fwts_bmc_info.ipmi_version),
		IPMI_DEV_IPMI_VERSION_MINOR(fwts_bmc_info.ipmi_version));

	fwts_progress(fw, 100);

	return FWTS_OK;
}

static int bmc_info_test1(fwts_framework *fw)
{

	if (!fwts_ipmi_present(R_OK | W_OK)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "BMC Info",
			"Cannot read and write to the IPMI device interface,"
			" check your user privileges.");
		return FWTS_ERROR;
	}
	if (fwts_bmc_info_check(fw)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "BMC Info",
			"Internal processing errors.");
		return FWTS_ERROR;
	}

	fwts_passed(fw, "BMC info passed.");

	return FWTS_OK;
}

static fwts_framework_minor_test bmc_info_tests[] = {
	{ bmc_info_test1, "BMC Info" },
	{ NULL, NULL }
};

static fwts_framework_ops bmc_info_ops = {
	.description = "BMC Info",
	.minor_tests = bmc_info_tests
};

FWTS_REGISTER_FEATURES("bmc_info", &bmc_info_ops, FWTS_TEST_EARLY,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV, FWTS_FW_FEATURE_IPMI)
