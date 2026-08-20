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

static int example_init(fwts_framework *fw)
{
	/* Put pre-test initialistion code here */

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do tests
	 */
	return FWTS_OK;
}

static int example_deinit(fwts_framework *fw)
{
	/* Put post-test de-initialistion code here */

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do tests
	 */
	return FWTS_OK;
}

static int example_test1(fwts_framework *fw)
{
	/* Do your test */

	/* Log success or failure */
	fwts_passed(fw, "Test passed, hurrah!");
	/*
	fwts_failed(fw, LOG_LEVEL_HIGH, "ExampleUniqueTestMessageIdentifier", "Test failed!");
	*/

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do test
	 */
	return FWTS_OK;
}

static int example_test2(fwts_framework *fw)
{
	/* Do your test */

	/* Log success or failure */
	fwts_passed(fw, "Test passed, hurrah!");
	/*
	fwts_failed(fw, LOG_LEVEL_HIGH, "ExampleUniqueTestMessageIdentifier", "Test failed!");
	*/

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do test
	 */
	return FWTS_OK;
}

/*
 *  Null terminated array of tests to run, in this
 *  scenario, we just have one test.
 */
static fwts_framework_minor_test example_tests[] = {
	{ example_test1, "Example sub test1 name." },
	{ example_test2, "Example sub test2 name." },
	{ NULL, NULL }
};

static fwts_framework_ops example_ops = {
	.description = "Example name.", /* Simple short description of test */
	.init        = example_init,	/* Can be NULL and/or omitted if not required */
	.deinit      = example_deinit,	/* Can be NULL  and/or omitted if not required */
	.minor_tests = example_tests    /* Array of tests to run */
};

/*
 *   See fwts_framework.h for flags,
 */
FWTS_REGISTER("example", &example_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH);
