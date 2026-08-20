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
#include "fwts.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static fwts_list *olog;

static int olog_init(fwts_framework *fw)
{
	if (fw->olog) {
		olog = fwts_file_open_and_read(fw->olog);
		if (olog == NULL) {
			fwts_log_error(fw, "OLOG -o file %s may not exist, please check that the file exits and is good.", fw->olog);
			return FWTS_ERROR;
		}
	}
	else {
		olog = fwts_olog_read(fw);
		if (olog == NULL) {
			fwts_log_error(fw, "OLOG without any parameters on the platform you are running does nothing, please specify -o for custom log analysis.");
			fwts_log_error(fw, "PPC supports dump and analysis of the default firmware logs.");
			return FWTS_SKIP;
		}
	}

	return FWTS_OK;
}

static int olog_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_klog_free(olog);

	return FWTS_OK;
}

static void olog_progress(fwts_framework *fw, int progress)
{
	fwts_progress(fw, progress);
}

static int olog_test1(fwts_framework *fw)
{
	int errors = 0;

	if (fwts_olog_firmware_check(fw, olog_progress, olog, &errors)) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"olog_firmware_check",
			"Problem in the OLOG processing, see earlier in"
			" this log for details on the problem.");
		return FWTS_ERROR;
	}

	if (errors > 0)
		/* Checks will log errors as failures automatically */
		fwts_log_info(fw, "OLOG scan and analysis found %d unique issue(s).",
			errors);
	else
		fwts_passed(fw, "OLOG scan and analysis passed.");

	return FWTS_OK;
}

static fwts_framework_minor_test olog_tests[] = {
	{ olog_test1, "OLOG scan and analysis checks results." },
	{ NULL, NULL }
};

static fwts_framework_ops olog_ops = {
	.description = "Run OLOG scan and analysis checks.",
	.init        = olog_init,
	.deinit      = olog_deinit,
	.minor_tests = olog_tests
};

FWTS_REGISTER("olog", &olog_ops, FWTS_TEST_EARLY, FWTS_FLAG_BATCH)
