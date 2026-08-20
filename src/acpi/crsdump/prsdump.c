/*
 * Copyright (C) 2014-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include "fwts_acpi_object_eval.h"
#include "crsdump.h"

/*
 *  prsdump_init()
 *	initialize ACPI
 */
static int prsdump_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  prsdump_deinit
 *	de-intialize ACPI
 */
static int prsdump_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

static int prsdump_test1(fwts_framework *fw)
{
	return resource_dump(fw, "_PRS");
}

static fwts_framework_minor_test prsdump_tests[] = {
	{ prsdump_test1, "Dump ACPI _PRS (Possible Resource Settings)." },
	{ NULL, NULL }
};

static fwts_framework_ops prsdump_ops = {
	.description = "Dump ACPI _PRS resources.",
	.init        = prsdump_init,
	.deinit      = prsdump_deinit,
	.minor_tests = prsdump_tests
};

FWTS_REGISTER("prsdump", &prsdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)

#endif
