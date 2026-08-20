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

#include "fwts.h"

#include <libfdt.h>

#define MAX_PSTATES 256

#define CPUIDLE_STATE_MAX   10

proc_gen_t proc_gen;

static const char *power_mgt_path = "/ibm,opal/power-mgt/";

/**
 * cmp_pstates: Compares the given two pstates and determines which
 *              among them is associated with a higher pstate.
 *
 * @a,@b: The pstate ids of the pstates being compared.
 *
 * Returns: -1 : If pstate associated with @a is smaller than
 *               the pstate associated with @b.
 *      0 : If pstates associated with @a and @b are equal.
 *      1 : If pstate associated with @a is greater than
 *               the pstate associated with @b.
 */
static int (*cmp_pstates)(int a, int b);

static int cmp_positive_pstates(int a, int b)
{
	if (a > b)
		return -1;
	else if (a < b)
		return 1;

	return 0;
}

static int cmp_negative_pstates(int a, int b)
{
	return cmp_positive_pstates(b, a);
}

static int validate_dt_prop_sizes(
	fwts_framework *fw,
	const char *prop1,
	int prop1_len,
	const char *prop2,
	int prop2_len)
{
	if (prop1_len == prop2_len)
		return FWTS_OK;

	fwts_failed(fw, LOG_LEVEL_HIGH, "SizeMismatch",
		"array sizes don't match for %s len %d and %s len %d\n",
		prop1, prop1_len, prop2, prop2_len);

	return FWTS_ERROR;
}

static int power_mgmt_init(fwts_framework *fw)
{
	int ret;

	if (fw->firmware_type != FWTS_FIRMWARE_OPAL) {
		fwts_skipped(fw,
			"The firmware type detected was non OPAL "
			"so skipping the OPAL Power Management DT checks.");
		return FWTS_SKIP;
	}

	if (!fw->fdt) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "NoDeviceTree",
			"Device tree not found");
		return FWTS_ERROR;
	}

	ret = get_proc_gen(fw);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "ProcGenFail",
			"Failed to get the Processor generation");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}


static int pstate_limits_test(fwts_framework *fw)
{
	int pstate_min, pstate_max, pstates[MAX_PSTATES];
	bool ok = true;
	int  nr_pstates, offset, len, ret, i;

	switch (proc_gen) {
	case proc_gen_p8:
		cmp_pstates = cmp_negative_pstates;
		break;
	case proc_gen_p9:
		cmp_pstates = cmp_positive_pstates;
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_HIGH, "UnknownProcessorChip",
			"Unknown processor generation %d", proc_gen);
		return FWTS_ERROR;
	}

	offset = fdt_path_offset(fw->fdt, power_mgt_path);
	if (offset < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
			"power management node %s is missing", power_mgt_path);
		return FWTS_ERROR;
	}

	ret = fwts_dt_property_read_u32(fw->fdt, offset, "ibm,pstate-min",
					&pstate_min);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property ibm,pstate-min %s",
			fdt_strerror(pstate_min));
		return FWTS_ERROR;
	}

	ret = fwts_dt_property_read_u32(fw->fdt, offset, "ibm,pstate-max",
					&pstate_max);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property ibm,pstate-max %s",
			fdt_strerror(pstate_max));
		return FWTS_ERROR;
	}

	memset(pstates, 0, sizeof(pstates));
	ret = fwts_dt_property_read_u32_arr(fw->fdt, offset, "ibm,pstate-ids",
					pstates, &len);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property ibm,pstate-ids %s",
			fdt_strerror(len));
		return FWTS_ERROR;
	}

	nr_pstates = abs(pstate_max - pstate_min) + 1;

	fwts_log_info(fw, "Pstates info: "
			"Pstate min: %d "
			"Pstate max: %d "
			"nr_pstates: %d "
			"Pstate ID's:  ",
			pstate_min, pstate_max, nr_pstates);

	for (i = 0; i < nr_pstates; i++)
		fwts_log_info(fw, " %d ", pstates[i]);

	if (nr_pstates <= 1)
		fwts_log_warning(fw, "Pstates range %d is not valid",
				 nr_pstates);

	if (proc_gen == proc_gen_p8 && nr_pstates > 128)
		fwts_log_warning(fw,
				"More than 128 pstates found, nr_pstates = %d",
				 nr_pstates);

	if (proc_gen == proc_gen_p9 && nr_pstates > 255)
		fwts_log_warning(fw,
				"More than 255 pstates found, nr_pstates = %d",
				 nr_pstates);

	if (len != nr_pstates)
		fwts_log_warning(fw, "Wrong number of pstates."
				"Expected %d pstates, found %d pstates",
				nr_pstates, len);

	/* Avoid reads outside of pstates[] */
	if (nr_pstates > MAX_PSTATES) {
		nr_pstates = MAX_PSTATES;
		fwts_log_warning(fw, "Truncating to %d pstates\n", nr_pstates);
	}

	for (i = 0; i < nr_pstates; i++) {
		if (cmp_pstates(pstate_max, pstates[i]) < 0) {
			fwts_log_warning(fw, "Invalid Pstate id %d "
					"greater than max pstate %d",
					pstates[i], pstate_max);
			ok = false;
		}
		if (cmp_pstates(pstates[i], pstate_min) < 0) {
			fwts_log_warning(fw, "Invalid Pstate id %d "
					"lesser than min pstate %d",
					pstates[i], pstate_min);
			ok = false;
		}
	}

	/* Pstates should be in monotonic descending order */
	for (i = 0; i < nr_pstates; i++) {
		if ((i == 0) && (cmp_pstates(pstates[i], pstate_max) != 0)) {
			fwts_log_warning(fw, "Pstates mismatch: "
					"Expected Pmax %d,"
					"Actual Pmax %d",
					pstate_max, pstates[i]);
			ok = false;
		} else if ((i == nr_pstates - 1) &&
			(cmp_pstates(pstates[i], pstate_min) != 0)) {
			fwts_log_warning(fw, "Pstates mismatch: "
					"Expected Pmin %d,"
					"Actual Pmin %d",
					pstate_min, pstates[i]);
			ok = false;
		} else if (i != 0 && i != nr_pstates) {
			int previous_pstate;
			previous_pstate = pstates[i-1];
			if (cmp_pstates(pstates[i], previous_pstate) > 0) {
				fwts_log_warning(fw, "Non monotonicity ...,"
						"Pstate %d greater then"
						" previous Pstate %d",
						pstates[i], previous_pstate);
				ok = false;
			}
		}
	}

	if (!ok) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "CPUPstateLimitsTestFail",
		"One or few CPU Pstates DT validation tests failed");
		return FWTS_ERROR;
	}
	fwts_passed(fw, "CPU Frequency pstates are validated");
	return FWTS_OK;

}

static int cpuidle_states_test(fwts_framework *fw)
{
	int offset, len, test_len, ret;
	int latency_ns[CPUIDLE_STATE_MAX];
	int residency_ns[CPUIDLE_STATE_MAX];
	int flags[CPUIDLE_STATE_MAX];
	uint64_t pm_cr[CPUIDLE_STATE_MAX];
	uint64_t pm_cr_mask[CPUIDLE_STATE_MAX];
	bool has_stop_inst = false;
	bool ok = true;
	char *control_prop, *mask_prop;

	switch (proc_gen) {
	case proc_gen_p8:
		has_stop_inst = false;
		control_prop = "ibm,cpu-idle-state-pmicr";
		mask_prop = "ibm,cpu-idle-state-pmicr-mask";
		break;
	case proc_gen_p9:
		has_stop_inst = true;
		control_prop = "ibm,cpu-idle-state-psscr";
		mask_prop = "ibm,cpu-idle-state-psscr-mask";
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_HIGH, "UnknownProcessorChip",
			"Unknown processor generation %d", proc_gen);
		return FWTS_ERROR;
	}

	offset = fdt_path_offset(fw->fdt, power_mgt_path);
	if (offset < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
			"power management node %s is missing", power_mgt_path);
		return FWTS_ERROR;
	}

	/* In P9 ibm,enabled-stop-levels present in /ibm,opal/power-mgt/ */
	if (has_stop_inst) {
		const int *buf;

		buf = fdt_getprop(fw->fdt, offset, "ibm,enabled-stop-levels",
					&len);
		if (!buf) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyMissing",
				"ibm,enabled-stop-levels missing under %s",
				power_mgt_path);
			return FWTS_ERROR;
		}
	}

	/* Validate ibm,cpu-idle-state-flags property */
	ret = fwts_dt_property_read_u32_arr(fw->fdt, offset,
			"ibm,cpu-idle-state-flags", flags, &len);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property ibm,cpu-idle-state-flags %s",
			fdt_strerror(len));
		return FWTS_ERROR;
	}

	if (len < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNoIdleStates",
			"No idle states found in DT");
		return FWTS_ERROR;
	}

	if (len > CPUIDLE_STATE_MAX-1) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTMoreIdleStates",
			"More idle states found in DT than the expected");
		return FWTS_ERROR;
	}

	/* Validate ibm,cpu-idle-state-latencies-ns property */
	ret = fwts_dt_property_read_u32_arr(fw->fdt, offset,
			"ibm,cpu-idle-state-latencies-ns",
			latency_ns, &test_len);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property"
			" ibm,cpu-idle-state-latencies-ns %s",
			fdt_strerror(test_len));
		return FWTS_ERROR;
	}

	if (validate_dt_prop_sizes(fw, "ibm,cpu-idle-state-flags", len,
		"ibm,cpu-idle-state-latencies-ns", test_len) != FWTS_OK)
		ok = false;

	/* Validate ibm,cpu-idle-state-names property */
	test_len = fwts_dt_stringlist_count(fw, fw->fdt, offset,
				"ibm,cpu-idle-state-names");
	if (test_len > 0) {
		if (validate_dt_prop_sizes(fw, "ibm,cpu-idle-state-flags", len,
			"ibm,cpu-idle-state-names", test_len) != FWTS_OK)
			ok = false;
	}

	/* Validate ibm,cpu-idle-state-residency-ns property */
	ret = fwts_dt_property_read_u32_arr(fw->fdt, offset,
			"ibm,cpu-idle-state-residency-ns",
			residency_ns, &test_len);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property "
			"ibm,cpu-idle-state-residency-ns %s",
			fdt_strerror(test_len));
		return FWTS_ERROR;
	}

	if (validate_dt_prop_sizes(fw, "ibm,cpu-idle-state-flags", len,
		"ibm,cpu-idle-state-residency-ns", test_len) != FWTS_OK)
		ok = false;

	/* Validate pmicr and psscr value and mask bits */
	ret = fwts_dt_property_read_u64_arr(fw->fdt, offset,
			control_prop, pm_cr, &test_len);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property %s rc: %s", control_prop,
			fdt_strerror(test_len));
		return FWTS_ERROR;
	}

	if (validate_dt_prop_sizes(fw, "ibm,cpu-idle-state-flags", len,
			control_prop, test_len) != FWTS_OK)
		ok = false;

	ret = fwts_dt_property_read_u64_arr(fw->fdt, offset,
		mask_prop, pm_cr_mask, &test_len);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property %s rc: %s", mask_prop,
			fdt_strerror(test_len));
		return FWTS_ERROR;
	}

	if (validate_dt_prop_sizes(fw, "ibm,cpu-idle-state-flags", len,
			mask_prop, test_len) != FWTS_OK)
		ok = false;

	if (!ok) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "CPUIDLEStatesFail",
			"One or few CPU IDLE DT Validation tests failed");
		return FWTS_ERROR;
	}
	fwts_passed(fw, "CPU IDLE States are validated");
	return FWTS_OK;
}

static fwts_framework_minor_test power_mgmt_tests[] = {
	{ pstate_limits_test, "OPAL Processor Frequency States Info" },
	{ cpuidle_states_test, "OPAL Processor Idle States Info" },
	{ NULL, NULL }
};

static fwts_framework_ops power_mgmt_tests_ops = {
	.description = "OPAL Processor Power Management DT Validation Tests",
	.init        = power_mgmt_init,
	.minor_tests = power_mgmt_tests
};

FWTS_REGISTER_FEATURES("power_mgmt", &power_mgmt_tests_ops, FWTS_TEST_EARLY,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
		FWTS_FW_FEATURE_DEVICETREE)
