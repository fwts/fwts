/*
 * Copyright (C) 2010-2026 Canonical
 * Copyright (C) 2018-2021 9elements Cyber Security
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static fwts_list *clog_list;

static int clog_init(fwts_framework *fw)
{
	if (!fwts_clog_available(fw)) {
		fwts_log_info(fw, "coreboot log not available, skipping test");
		return FWTS_SKIP;
	}

	clog_list = fwts_clog_read(fw);
	if (clog_list == NULL) {
		fwts_log_error(fw, "Cannot read coreboot log.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int clog_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_clog_free(clog_list);

	return FWTS_OK;
}

static void clog_progress(fwts_framework *fw, int progress)
{
	fwts_progress(fw, progress);
}

static int clog_test1(fwts_framework *fw)
{
	int errors = 0;

	if (fwts_clog_firmware_check(fw, clog_progress, clog_list, &errors)) {
		fwts_log_error(fw, "Error parsing coreboot log.");
		return FWTS_ERROR;
	}

	if (errors > 0)
		/* Checks will log errors as failures automatically */
		fwts_log_info(fw, "Found %d unique errors in coreboot log.",
			errors);
	else
		fwts_passed(fw, "Found no errors in coreboot log.");

	return FWTS_OK;
}

static fwts_framework_minor_test clog_tests[] = {
	{ clog_test1, "coreboot log error check." },
	{ NULL, NULL }
};

static fwts_framework_ops clog_ops = {
	.description = "Scan coreboot log for errors and warnings.",
	.init        = clog_init,
	.deinit      = clog_deinit,
	.minor_tests = clog_tests
};

FWTS_REGISTER("clog", &clog_ops, FWTS_TEST_EARLY, FWTS_FLAG_BATCH)

