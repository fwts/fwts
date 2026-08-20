/*
 * Copyright (C) 2010-2026 Canonical
 *
 * from ideas in check-bios-nx
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
#include <string.h>

static int nx_test1(fwts_framework *fw)
{
	fwts_cpuinfo_x86 *fwts_nx_cpuinfo;

	if ((fwts_nx_cpuinfo = fwts_cpu_get_info(-1)) == NULL) {
		fwts_log_error(fw, "Cannot get CPU info");
		return FWTS_ERROR;
	}

	if (fwts_nx_cpuinfo->flags == NULL) {
		fwts_log_error(fw, "Cannot get CPU flags");
		fwts_cpu_free_info(fwts_nx_cpuinfo);
		return FWTS_ERROR;
	}

	if (strstr(fwts_nx_cpuinfo->flags," nx")) {
		fwts_passed(fw, "CPU has NX flags, BIOS is not disabling it.");
		fwts_cpu_free_info(fwts_nx_cpuinfo);
		return FWTS_OK;
	}

	if (strstr(fwts_nx_cpuinfo->flags," pae") == NULL) {
		fwts_passed(fw, "CPU is not PAE capable, so it does not have NX.");
		fwts_cpu_free_info(fwts_nx_cpuinfo);
		return FWTS_OK;
	}

	if ((fwts_nx_cpuinfo->x86 == -1) ||
	    (fwts_nx_cpuinfo->x86_model == -1)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NXNoCPUModel",
			"No model or family found for this CPU. Please check /proc/cpuinfo.");
		fwts_cpu_free_info(fwts_nx_cpuinfo);
		return FWTS_OK;
	}

	if (((fwts_nx_cpuinfo->x86 == 6)  && (fwts_nx_cpuinfo->x86_model >= 14)) ||
	    ((fwts_nx_cpuinfo->x86 == 15) && (fwts_nx_cpuinfo->x86_model >= 3))  ||
	    (fwts_nx_cpuinfo->x86 > 15)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "NXCapableNotEnabled",
			"The CPU is family %d, model %d and has NX capabilities but they are not enabled.",
			fwts_nx_cpuinfo->x86, fwts_nx_cpuinfo->x86_model);
		fwts_advice(fw,
			"The system cannot use the protection features provided by NX because "
			"the BIOS is configured to disable this capability. Please ensure this "
			"is enabled in the BIOS. For more information please consult "
			"https://wiki.ubuntu.com/Security/CPUFeatures");
	} else
		fwts_passed(fw,
			"The CPU is family %d, model %d and does not have NX capabilities.",
			fwts_nx_cpuinfo->x86, fwts_nx_cpuinfo->x86_model);

	fwts_cpu_free_info(fwts_nx_cpuinfo);
	return FWTS_OK;
}

static int nx_test2(fwts_framework *fw)
{
	int i;
	int n;
	int cpu0_has_nx = 0;
	int failed = 0;

	fwts_log_info(fw,
		"This test verifies that all CPUs have the same NX flag setting. "
		"Although rare, BIOS may set the NX flag differently "
		"per CPU.");

	if ((n = fwts_cpu_enumerate()) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot determine number of CPUs");
		return FWTS_ERROR;
	}

	if (n == 1) {
		fwts_log_info(fw, "Only one CPU, no need to run test.");
		return FWTS_OK;
	}

	for (i = 0; i < n; i++) {
		fwts_cpuinfo_x86 *fwts_nx_cpuinfo;

		if ((fwts_nx_cpuinfo = fwts_cpu_get_info(-1)) == NULL) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "NXCPUInfoRead", "Cannot get CPU%d info", i);
			fwts_cpu_free_info(fwts_nx_cpuinfo);
			return FWTS_ERROR;
		}
		if (fwts_nx_cpuinfo->flags == NULL) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "NXCPUInfoRead", "Cannot get CPU%d flags", i);
			fwts_cpu_free_info(fwts_nx_cpuinfo);
			return FWTS_ERROR;
		}
		if (i == 0) {
			cpu0_has_nx = (strstr(fwts_nx_cpuinfo->flags," nx") != NULL);
		} else {
			if (cpu0_has_nx != (strstr(fwts_nx_cpuinfo->flags," nx") != NULL)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"NXCPUFlagsInconsistent",
					"CPU%d has different NX flags to CPU0.", i);
				failed++;
			}
		}
		fwts_cpu_free_info(fwts_nx_cpuinfo);
	}

	if (!failed)
		fwts_passed(fw, "All %d CPUs have the same NX flag %s.", n, cpu0_has_nx ? "set" :  "cleared");

	return FWTS_OK;
}

static int nx_test3(fwts_framework *fw)
{
	int n;
	int i;
	int failed = 0;
	uint64_t msr_value = 0;
	const uint64_t nx_bit = 1ULL << 34;

	fwts_log_info(fw,
		"This test verifies that all CPUs have the same NX flag setting by examining the per CPU MSR register 0x1a0.");

	if ((n = fwts_cpu_enumerate()) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot determine number of CPUs");
		return FWTS_ERROR;
	}

	for (i = 0; i < n; i++) {
		fwts_cpuinfo_x86 *fwts_nx_cpuinfo;
		uint64_t val;

		if ((fwts_nx_cpuinfo = fwts_cpu_get_info(-1)) == NULL) {
			fwts_log_error(fw, "Cannot get CPU info");
			return FWTS_ERROR;
		}
		if (fwts_nx_cpuinfo->vendor_id == NULL) {
			fwts_log_error(fw, "Cannot get CPU vendor ID");
			fwts_cpu_free_info(fwts_nx_cpuinfo);
			return FWTS_ERROR;
		}
		if (strstr(fwts_nx_cpuinfo->vendor_id, "Intel") == NULL) {
			fwts_log_info(fw, "Non-Intel CPU, skipping test.");
			fwts_cpu_free_info(fwts_nx_cpuinfo);
			return FWTS_OK;
		}
		if (fwts_cpu_readmsr(fw, i, 0x1a0, &val) != FWTS_OK) {
			fwts_log_error(fw, "Cannot read msr 0x1a0 on CPU%d", i);
			fwts_cpu_free_info(fwts_nx_cpuinfo);
			return FWTS_ERROR;
		}
		if (i == 0) {
			msr_value = val;
		} else {
			if ((msr_value & nx_bit) != (val & nx_bit)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"NXCPUFlagsInconsistent",
					"CPU%d has different NX flags to CPU0.", i);
				failed++;
			}
		}
		fwts_cpu_free_info(fwts_nx_cpuinfo);
	}

	if (!failed)
		fwts_passed(fw, "All %d CPUs have the NX flag in MSR 0x1a0 %s.", n, (msr_value & nx_bit) == 0 ? "set" : "cleared");

	return FWTS_OK;
}

static fwts_framework_minor_test nx_tests[] = {
	{ nx_test1, "Test CPU NX capability." },
	{ nx_test2, "Test all CPUs have same BIOS set NX flag." },
	{ nx_test3, "Test all CPUs have same msr setting in MSR 0x1a0." },
	{ NULL, NULL }
};

static fwts_framework_ops nx_ops = {
	.description = "Test if CPU NX is disabled by the BIOS.",
	.minor_tests = nx_tests
};

FWTS_REGISTER("nx", &nx_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
