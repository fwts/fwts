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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "fwts.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static fwts_list *klog;

static int oops_init(fwts_framework *fw)
{
	if (fw->klog)
		klog = fwts_file_open_and_read(fw->klog);
	else
		klog = fwts_klog_read();

	if (klog == NULL) {
		fwts_log_error(fw, "Cannot read kernel log.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int oops_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_klog_free(klog);

	return FWTS_OK;
}

static int oops_test1(fwts_framework *fw)
{
	int oopses = 0;
	int warn_ons = 0;

	if (fwts_oops_check(fw, klog, &oopses, &warn_ons) != FWTS_OK) {
		fwts_log_error(fw, "Error parsing kernel log.");
		return FWTS_ERROR;
	}

	if (oopses > 0)
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"KernelOops", "Found %d oopses in kernel log.", oopses);
	else
		fwts_passed(fw, "Found no oopses in kernel log.");

	if (warn_ons > 0)
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"KernelWarnOns", "Found %d WARN_ON warnings in kernel log.", warn_ons);
	else
		fwts_passed(fw, "Found no WARN_ON warnings in kernel log.");

	return FWTS_OK;
}

static fwts_framework_minor_test oops_tests[] = {
	{ oops_test1, "Kernel log oops check." },
	{ NULL, NULL }
};

static fwts_framework_ops oops_ops = {
	.description = "Scan kernel log for Oopses.",
	.init        = oops_init,
	.deinit      = oops_deinit,
	.minor_tests = oops_tests
};

FWTS_REGISTER("oops", &oops_ops, FWTS_TEST_EARLY, FWTS_FLAG_BATCH)
