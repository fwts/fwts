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

static int klog_init(fwts_framework *fw)
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

static int klog_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_klog_free(klog);

	return FWTS_OK;
}

static void klog_progress(fwts_framework *fw, int progress)
{
	fwts_progress(fw, progress);
}

static int klog_test1(fwts_framework *fw)
{
	int errors = 0;

	if (fwts_klog_firmware_check(fw, klog_progress, klog, &errors)) {
		fwts_log_error(fw, "Error parsing kernel log.");
		return FWTS_ERROR;
	}

	if (errors > 0)
		/* Checks will log errors as failures automatically */
		fwts_log_info(fw, "Found %d unique errors in kernel log.",
			errors);
	else
		fwts_passed(fw, "Found no errors in kernel log.");

	return FWTS_OK;
}

static fwts_framework_minor_test klog_tests[] = {
	{ klog_test1, "Kernel log error check." },
	{ NULL, NULL }
};

static fwts_framework_ops klog_ops = {
	.description = "Scan kernel log for errors and warnings.",
	.init        = klog_init,
	.deinit      = klog_deinit,
	.minor_tests = klog_tests
};

FWTS_REGISTER("klog", &klog_ops, FWTS_TEST_EARLY, FWTS_FLAG_BATCH)
