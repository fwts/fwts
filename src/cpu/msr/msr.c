/*
 * Copyright (C) 2011-2026 Canonical
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
#include <inttypes.h>

#ifdef FWTS_ARCH_INTEL

typedef void (*msr_callback_check)(fwts_framework *fw, const uint64_t val);

static int ncpus;
static bool intel_cpu;
static bool amd_cpu;
static bool hygon_cpu;
static fwts_cpuinfo_x86 *cpuinfo;

static int msr_init(fwts_framework *fw)
{
	char *bios_vendor;

	if ((cpuinfo = fwts_cpu_get_info(-1)) == NULL) {
		fwts_log_error(fw, "Cannot get CPU info");
		return FWTS_ERROR;
	}
	if (cpuinfo->vendor_id == NULL) {
		fwts_log_error(fw, "Cannot get CPU vendor_id");
		return FWTS_ERROR;
	}
	intel_cpu = strstr(cpuinfo->vendor_id, "Intel") != NULL;
	amd_cpu = strstr(cpuinfo->vendor_id, "AuthenticAMD") != NULL;
	hygon_cpu = strstr(cpuinfo->vendor_id, "HygonGenuine") != NULL;

	if ((ncpus = fwts_cpu_enumerate()) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot detect the number of CPUs on this machine.");
		return FWTS_ABORTED;
	}

	/*
	 * Running MSR tests inside virtual machines such as QEMU with some kernel/kvm
	 * combinations have been observed to cause GPFs.  We kludge around this by
	 * avoiding MSR tests for a Bochs BIOS based QEMU virtual machine.
	 */
	if ((bios_vendor = fwts_get("/sys/class/dmi/id/bios_vendor")) != NULL) {
		if (strstr(bios_vendor, "Bochs")) {
			fwts_log_error(fw,
				"MSR test being avoiding inside a virtual machine as "
				"this is known to cause General Protection Faults on "
				"some configurations.");
			free(bios_vendor);
			return FWTS_SKIP;
		}
		free(bios_vendor);
	}
	return FWTS_OK;
}

static int msr_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_cpu_free_info(cpuinfo);

	return FWTS_OK;
}

static int msr_consistent(
	fwts_framework *fw,
	const uint32_t msr,
	const int shift,
	const uint64_t mask,
	uint64_t *const vals,
	int *const inconsistent_count,
	bool *const inconsistent)
{
	int cpu;

	*inconsistent_count = 0;

	for (cpu = 0; cpu < ncpus; cpu++) {
		uint64_t val;
		if (fwts_cpu_readmsr(fw, cpu, msr, &val) != FWTS_OK) {
			return FWTS_ERROR;
		}
		val >>= shift;
		val &= mask;
		vals[cpu] = val;
	}

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (vals[0] != vals[cpu]) {
			(*inconsistent_count)++;
			inconsistent[cpu] = true;
		} else {
			inconsistent[cpu] = false;
		}
	}
	return FWTS_OK;
}

static int msr_consistent_check(fwts_framework *fw,
	const fwts_log_level level,
	const char *const msr_name,
	const uint32_t msr,
	const int shift,
	const uint64_t mask,
	const msr_callback_check callback)
{
	uint64_t *vals;
	bool *inconsistent;
	int inconsistent_count;

	if ((vals = calloc(ncpus, sizeof(uint64_t))) == NULL) {
		fwts_log_error(fw, "Out of memory allocating msr value buffers.");
		return FWTS_ERROR;
	}
	if ((inconsistent = calloc(ncpus, sizeof(bool))) == NULL) {
		fwts_log_error(fw, "Out of memory allocating msr value buffers.");
		free(vals);
		return FWTS_ERROR;
	}
	if (msr_consistent(fw, msr, shift, mask,
		vals, &inconsistent_count, inconsistent) != FWTS_OK) {
		free(inconsistent);
		free(vals);
		return FWTS_ERROR;
	}
	if (inconsistent_count > 0) {
		int cpu;

		fwts_failed(fw, level, "MSRCPUsInconsistent",
			"MSR 0x%8.8" PRIx32 " %s has %d inconsistent values across "
			"%d CPUs (shift: %d mask: 0x%" PRIx64 ").",
			msr, msr_name, inconsistent_count,
			ncpus, shift, mask);

		for (cpu = 1; cpu < ncpus; cpu++) {
			if (inconsistent[cpu])
				fwts_log_info(fw, "MSR CPU 0 -> 0x%" PRIx64
					" vs CPU %d -> 0x%" PRIx64,
					vals[0], cpu, vals[cpu]);
		}
	} else {
		fwts_passed(fw, "MSR 0x%8.8" PRIx32 " %s "
			"is consistent across %d CPUs.",
			msr, msr_name, ncpus);
		if (callback)
			callback(fw, vals[0]);
	}

	free(inconsistent);
	free(vals);

	return FWTS_OK;
}

static int msr_pstate_ratios(fwts_framework *fw)
{
	if (intel_cpu) {
		msr_consistent_check(fw, LOG_LEVEL_HIGH, "Minimum P-State", 0xce, 8, 0xff, NULL);
		msr_consistent_check(fw, LOG_LEVEL_HIGH, "Maximum P-State", 0xce, 40, 0xff, NULL);
	} else
		fwts_skipped(fw, "Non-Intel CPU, test skipped.");

	return FWTS_OK;
}

static void msr_c1_c3_autodemotion_check(fwts_framework *fw, const uint64_t val)
{
	fwts_log_info(fw, "C1 and C3 Autodemotion %s.",
		val == 3 ? "enabled" : "disabled");
}

static int msr_c1_c3_autodemotion(fwts_framework *fw)
{
	if (intel_cpu) {
		msr_consistent_check(fw, LOG_LEVEL_HIGH, "C1 and C3 Autodemotion", 0xe2, 25, 0x03, msr_c1_c3_autodemotion_check);
	} else
		fwts_skipped(fw, "Non-Intel CPU, test skipped.");

	return FWTS_OK;
}

static int msr_smrr(fwts_framework *fw)
{
	uint64_t val;

	if (intel_cpu) {
		if (fwts_cpu_readmsr(fw, 0, 0xfe, &val) != FWTS_OK) {
			fwts_skipped(fw, "Cannot read MSR 0xfe.");
			return FWTS_ERROR;
		}

		if (((val >> 11) & 1) == 0)
			fwts_log_info(fw, "SMRR not supported by this CPU.");
		else {
			msr_consistent_check(fw, LOG_LEVEL_HIGH, "SMRR_PHYSBASE", 0x1f2, 12, 0xfffff, NULL);
			msr_consistent_check(fw, LOG_LEVEL_HIGH, "SMRR_TYPE", 0x1f2, 0, 0x7, NULL);
			msr_consistent_check(fw, LOG_LEVEL_HIGH, "SMRR_PHYSMASK", 0x1f3, 12, 0xfffff, NULL);
			msr_consistent_check(fw, LOG_LEVEL_HIGH, "SMRR_VALID", 0x1f3, 11, 0x1, NULL);

			if (fwts_cpu_readmsr(fw, 0, 0x1f2, &val) == FWTS_OK) {
				uint64_t physbase = val & 0xfffff000;
				uint64_t type = val & 7;
				if ((physbase & 0xfff) != 0)
					fwts_failed(fw, LOG_LEVEL_HIGH, "MSRSMRR_PHYSBASE4KBBoundary",
						"SMRR: SMRR_PHYSBASE is NOT on a 4KB boundary: %" PRIx64 ".",
						physbase);
				if (type != 6)
					fwts_failed(fw, LOG_LEVEL_HIGH, "MSRSMRR_TYPE",
						"SMRR: SMRR_TYPE is 0x%" PRIx64 ", should be 0x6 (Write-Back).", type);
			}
			if (fwts_cpu_readmsr(fw, 0, 0x1f3, &val) == FWTS_OK) {
				uint64_t physmask = val & 0xfffff000;
				uint64_t valid = (val >> 11) & 1;

				if (physmask < 0x80000) {
					fwts_failed(fw, LOG_LEVEL_HIGH, "MSRSMRRRegion",
						"SMRR: region needs to be at least 8MB, "
						"SMRR_PHYSMASK = %" PRIx64 ".",
						physmask);
				}
				if (!valid)
					fwts_failed(fw, LOG_LEVEL_HIGH, "MSRSMRRValidBit",
						"SMRR: valid bit is 0, it should be 1 (enabled).");
			}
		}
	} else
		fwts_skipped(fw, "Non-Intel CPU, test skipped.");

	return FWTS_OK;
}


typedef struct {
	const char *const name;
	const uint32_t msr;
	const uint64_t mask;
	const msr_callback_check callback;
} msr_info;

/* From AMD Architecture Programmer's Manual, Volume 2: System Programming, Appending A */
static const msr_info AMD_MSRs[] = {
	{ "MTRRCAP",			0x000000fe,	0x0000000000000fffULL, NULL },
	/*
	 * LP#1582005 - Do not check sysenter MSRs, they will be different on
	 * each CPU, so checking them across CPUs is incorrect
	 *
	{ "SYSENTER_CS",		0x00000174,	0x000000000000ffffULL, NULL },
	{ "SYSENTER_ESP",		0x00000175,	0xffffffffffffffffULL, NULL },
	{ "SYSENTER_EIP",		0x00000176,	0xffffffffffffffffULL, NULL },
	 */
	 /*
	  * MCG_CAP is read-only and can differ from core to core
	{ "MCG_CAP",			0x00000179,	0x0000000001ff0fffULL, NULL },
	  */
	{ "MCG_STATUS",			0x0000017a,	0xffffffffffffffffULL, NULL },
	/* MCG_CTL differs in Ryzen 4000 series and probably later series
	{ "MCG_CTL",			0x0000017b,	0xffffffffffffffffULL, NULL },
	*/
	{ "MTRR_PHYSBASE0",		0x00000200,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK0",		0x00000201,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE1",		0x00000202,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK1",		0x00000203,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE2",		0x00000204,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK2",		0x00000205,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE3",		0x00000206,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK3",		0x00000207,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE4",		0x00000208,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK4",		0x00000209,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE5",		0x0000020a,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK5",		0x0000020b,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE6",		0x0000020c,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK6",		0x0000020d,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE7",		0x0000020e,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK7",		0x0000020f,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX64K_000",		0x00000250,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX16K_800",		0x00000258,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX16K_a00",		0x00000259,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_C000",		0x00000268,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_C800",		0x00000269,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_D000",		0x0000026a,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_D800",		0x0000026b,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_E000",		0x0000026c,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_E800",		0x0000026d,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_F000",		0x0000026e,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_F800",		0x0000026f,	0xffffffffffffffffULL, NULL },
	{ "PAT",			0x00000277,	0x0707070707070703ULL, NULL },
	{ "MTRR_DEF_TYPE",		0x000002ff,	0x0000000000000c0fULL, NULL },
	{ "EFER",			0xc0000080,	0x0000000000000d01ULL, NULL },
	/* Do not check syscall MSRs, they will be different on
	 * each CPU, so checking them across CPUs is incorrect
	{ "STAR",			0xc0000081,	0xffffffffffffffffULL, NULL },
	{ "LSTAR",			0xc0000082,	0xffffffffffffffffULL, NULL },
	{ "FMASK",			0xc0000084,	0xffffffffffffffffULL, NULL },
	 */
	//{ "FS_BASE",			0xc0000100,	0xffffffffffffffffULL, NULL },
	//{ "GS_BASE",			0xc0000101, 	0xffffffffffffffffULL, NULL },
	{ "KERNEL_GS_BASE",		0xc0000102, 	0xffffffffffffffffULL, NULL },
	//{ "TSC_AUX",			0xc0000103, 	0x00000000ffffffffULL, NULL },
	{ "SYSCFG",			0xc0010010,	0xffffffffffffffffULL, NULL },
	{ "IORRBase0",			0xc0010016,	0xffffffffffffffffULL, NULL },
	{ "IORRMask0",			0xc0010017,	0xffffffffffffffffULL, NULL },
	{ "IORRBase1",			0xc0010018,	0xffffffffffffffffULL, NULL },
	{ "IORRMask1",			0xc0010019,	0xffffffffffffffffULL, NULL },
	{ "TOP_MEM",			0xc001001a,	0xffffffffffffffffULL, NULL },
	{ "TOP_MEM2",			0xc001001d,	0xffffffffffffffffULL, NULL },
	{ "Processor_Name_String",	0xc0010030,	0xffffffffffffffffULL, NULL },
	{ "Processor_Name_String",	0xc0010031,	0xffffffffffffffffULL, NULL },
	{ "Processor_Name_String",	0xc0010032,	0xffffffffffffffffULL, NULL },
	{ "Processor_Name_String",	0xc0010033,	0xffffffffffffffffULL, NULL },
	{ "Processor_Name_String",	0xc0010034,	0xffffffffffffffffULL, NULL },
	{ "Processor_Name_String",	0xc0010035,	0xffffffffffffffffULL, NULL },
	{ "TSC Ratio",			0xc0010104,	0xffffffffffffffffULL, NULL },
	//{ "SMBASE",			0xc0010111,	0xffffffffffffffffULL, NULL },
	{ "SMM_ADDR",			0xc0010112,	0xffffffffffffffffULL, NULL },
	{ "SMM_MASK",			0xc0010113,	0xffffffffffffffffULL, NULL },
	{ "VM_CR",			0xc0010114,	0xffffffffffffffffULL, NULL },
	{ "IGNNE",			0xc0010115,	0xffffffffffffffffULL, NULL },
	{ "SMM_CTL",			0xc0010116,	0xffffffffffffffffULL, NULL },
	{ "SVM_KEY_MSR",		0xc0010118,	0xffffffffffffffffULL, NULL },
	{ "OSVW_ID_Length",		0xc0010140,	0xffffffffffffffffULL, NULL },
	{ NULL,				0x00000000,	0, NULL }
};

static const msr_info IA32_MSRs[] = {
	//{ "P5_MC_ADDR",		0x00000000,	0xffffffffffffffffULL, NULL },
	{ "P5_MC_TYPE",			0x00000001,	0xffffffffffffffffULL, NULL },
	{ "MONITOR_FILTER_SIZE",	0x00000006,	0xffffffffffffffffULL, NULL },
	//{ "TIME_STAMP_COUNTER",	0x00000010,	0xffffffffffffffffULL, NULL },
	{ "PLATFORM_ID",		0x00000017,	0x001c000000000000ULL, NULL },
	{ "APIC_BASE",			0x0000001b,	0xfffffffffffffeffULL, NULL },
	{ "FEATURE_CONTROL",		0x0000003a,	0x000000000016ff07ULL, NULL },
	{ "TSC_ADJUST",		0x0000003b,	0xffffffffffffffffULL, NULL },
	{ "BIOS_SIGN_ID",		0x0000008b,	0xffffffff00000000ULL, NULL },
	{ "SMM_MONITOR_CTL",            0x0000009b,     0x00000000fffff005ULL, NULL },
	{ "MTRRCAP",			0x000000fe,	0x0000000000001dffULL, NULL },
	/*
	 * LP#1582005 - Do not check sysenter MSRs, they will be different on
	 * each CPU, so checking them across CPUs is incorrect
	 *
	{ "SYSENTER_CS",		0x00000174,	0x000000000000ffffULL, NULL },
	{ "SYSENTER_ESP",		0x00000175,	0xffffffffffffffffULL, NULL },
	{ "SYSENTER_EIP",		0x00000176,	0xffffffffffffffffULL, NULL },
	 */
	{ "MCG_CAP",			0x00000179,	0x0000000001ff0fffULL, NULL },
	{ "MCG_STATUS",			0x0000017a,	0xffffffffffffffffULL, NULL },
	{ "MCG_CTL",			0x0000017b,	0xffffffffffffffffULL, NULL },
	{ "CLOCK_MODULATION",		0x0000019a,	0x000000000000001fULL, NULL },
	{ "THERM_INTERRUPT",		0x0000019b,	0x0000000001ffff1fULL, NULL },
	//{ "THERM_STATUS",		0x0000019c,	0x0000000080000fffULL, NULL },
	{ "MISC_ENABLE",		0x000001a0,	0x0000000400c51889ULL, NULL },
	{ "PACKAGE_THERM_INTERRUPT",	0x000001b2,	0x0000000001ffff17ULL, NULL },
	{ "SMRR_PHYSBASE",		0x000001f2,	0x00000000fffff0ffULL, NULL },
	{ "SMRR_PHYSMASK",		0x000001f3,	0x00000000fffff800ULL, NULL },
	{ "PLATFORM_DCA_CAP",		0x000001f8,	0xffffffffffffffffULL, NULL },
	{ "CPU_DCA_CAP",		0x000001f9,	0xffffffffffffffffULL, NULL },
	{ "DCA_O_CAP",			0x000001fa,	0x000000000501e7ffULL, NULL },
	{ "MTRR_PHYSBASE0",		0x00000200,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK0",		0x00000201,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE1",		0x00000202,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK1",		0x00000203,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE2",		0x00000204,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK2",		0x00000205,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE3",		0x00000206,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK3",		0x00000207,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE4",		0x00000208,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK4",		0x00000209,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE5",		0x0000020a,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK5",		0x0000020b,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE6",		0x0000020c,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK6",		0x0000020d,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE7",		0x0000020e,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK7",		0x0000020f,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE8",		0x00000210,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK8",		0x00000211,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSBASE9",		0x00000212,	0xffffffffffffffffULL, NULL },
	{ "MTRR_PHYSMASK9",		0x00000213,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX64K_000",		0x00000250,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX16K_800",		0x00000258,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX16K_a00",		0x00000259,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_C000",		0x00000268,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_C800",		0x00000269,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_D000",		0x0000026a,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_D800",		0x0000026b,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_E000",		0x0000026c,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_E800",		0x0000026d,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_F000",		0x0000026e,	0xffffffffffffffffULL, NULL },
	{ "MTRR_FIX4K_F800",		0x0000026f,	0xffffffffffffffffULL, NULL },
	{ "PAT",			0x00000277,	0x0707070707070707ULL, NULL },
	{ "MC0_CTL2",			0x00000280,	0x0000000040007fffULL, NULL },
	{ "MC1_CTL2",			0x00000281,	0x0000000040007fffULL, NULL },
	{ "MC2_CTL2",			0x00000282,	0x0000000040007fffULL, NULL },
	{ "MC3_CTL2",			0x00000283,	0x0000000040007fffULL, NULL },
	{ "MC4_CTL2",			0x00000284,	0x0000000040007fffULL, NULL },
	{ "MC5_CTL2",			0x00000285,	0x0000000040007fffULL, NULL },
	{ "MC6_CTL2",			0x00000286,	0x0000000040007fffULL, NULL },
	{ "MC7_CTL2",			0x00000287,	0x0000000040007fffULL, NULL },
	{ "MC8_CTL2",			0x00000288,	0x0000000040007fffULL, NULL },
	{ "MC9_CTL2",			0x00000289,	0x0000000040007fffULL, NULL },
	{ "MC10_CTL2",			0x0000028a,	0x0000000040007fffULL, NULL },
	{ "MC11_CTL2",			0x0000028b,	0x0000000040007fffULL, NULL },
	{ "MC12_CTL2",			0x0000028c,	0x0000000040007fffULL, NULL },
	{ "MC13_CTL2",			0x0000028d,	0x0000000040007fffULL, NULL },
	{ "MC14_CTL2",			0x0000028e,	0x0000000040007fffULL, NULL },
	{ "MC15_CTL2",			0x0000028f,	0x0000000040007fffULL, NULL },
	{ "MC16_CTL2",			0x00000290,	0x0000000040007fffULL, NULL },
	{ "MC17_CTL2",			0x00000291,	0x0000000040007fffULL, NULL },
	{ "MC18_CTL2",			0x00000292,	0x0000000040007fffULL, NULL },
	{ "MC19_CTL2",			0x00000293,	0x0000000040007fffULL, NULL },
	{ "MC20_CTL2",			0x00000294,	0x0000000040007fffULL, NULL },
	{ "MC21_CTL2",			0x00000295,	0x0000000040007fffULL, NULL },
	{ "MC22_CTL2",			0x00000296,	0x0000000040007fffULL, NULL },
	{ "MC23_CTL2",			0x00000297,	0x0000000040007fffULL, NULL },
	{ "MC24_CTL2",			0x00000298,	0x0000000040007fffULL, NULL },
	{ "MC25_CTL2",			0x00000299,	0x0000000040007fffULL, NULL },
	{ "MC26_CTL2",			0x0000029a,	0x0000000040007fffULL, NULL },
	{ "MC27_CTL2",			0x0000029b,	0x0000000040007fffULL, NULL },
	{ "MC28_CTL2",			0x0000029c,	0x0000000040007fffULL, NULL },
	{ "MC29_CTL2",			0x0000029d,	0x0000000040007fffULL, NULL },
	{ "MC30_CTL2",			0x0000029e,	0x0000000040007fffULL, NULL },
	{ "MC31_CTL2",			0x0000029f,	0x0000000040007fffULL, NULL },
	{ "MTRR_DEF_TYPE",		0x000002ff,	0x0000000000000c07ULL, NULL },
	{ "PEBS_ENABLE",		0x000003f1,	0x0000000000000001ULL, NULL },
	{ "VMX_BASIC",			0x00000480,	0xffffffffffffffffULL, NULL },
	{ "VMX_PINPASED_CTLS",		0x00000481,	0xffffffffffffffffULL, NULL },
	{ "VMX_PROCBASED_CTLS",		0x00000482,	0xffffffffffffffffULL, NULL },
	{ "VMX_EXIT_CTLS",		0x00000483,	0xffffffffffffffffULL, NULL },
	{ "VMX_ENTRY_CTLS",		0x00000484,	0xffffffffffffffffULL, NULL },
	{ "VMX_MISC",			0x00000485,	0xffffffffffffffffULL, NULL },
	{ "VMX_CR0_FIXED0",		0x00000486,	0xffffffffffffffffULL, NULL },
	{ "VMX_CR0_FIXED1",		0x00000487,	0xffffffffffffffffULL, NULL },
	{ "VMX_CR4_FIXED0",		0x00000488,	0xffffffffffffffffULL, NULL },
	{ "VMX_CR4_FIXED1",		0x00000489,	0xffffffffffffffffULL, NULL },
	{ "VMX_VMX_VMCS_ENUM",		0x0000048a,	0xffffffffffffffffULL, NULL },
	{ "VMX_PROCBASED_CTLS2",	0x0000048b,	0xffffffffffffffffULL, NULL },
	{ "VMX_EPT_VPID_CAP",		0x0000048c,	0xffffffffffffffffULL, NULL },
	{ "VMX_TRUE_PINBASED_CTLS",	0x0000048d,	0xffffffffffffffffULL, NULL },
	{ "VMX_TRUE_PROCBASED_CTLS",	0x0000048e,	0xffffffffffffffffULL, NULL },
	{ "VMX_TRUE_EXIT_CTLS",		0x0000048f,	0xffffffffffffffffULL, NULL },
	{ "VMX_TRUE_ENTRY_CTLS",	0x00000490,	0xffffffffffffffffULL, NULL },
	{ "VMX_VMFUNC",	0x00000491,	0xffffffffffffffffULL, NULL },
	/* Do not check syscall MSRs, they will be different on
	 * each CPU, so checking them across CPUs is incorrect
	{ "A_PMC4",			0x000004c5,	0xffffffffffffffffULL, NULL },
	{ "A_PMC5",			0x000004c6,	0xffffffffffffffffULL, NULL },
	{ "A_PMC6",			0x000004c7,	0xffffffffffffffffULL, NULL },
	{ "A_PMC7",			0x000004c8,	0xffffffffffffffffULL, NULL },
	*/
	{ "EFER",			0xc0000080,	0x0000000000000d01ULL, NULL },
	/* Do not check syscall MSRs, they will be different on
	 * each CPU, so checking them across CPUs is incorrect
	{ "STAR",			0xc0000081,	0xffffffffffffffffULL, NULL },
	{ "LSTAR",			0xc0000082,	0xffffffffffffffffULL, NULL },
	{ "FMASK",			0xc0000084,	0xffffffffffffffffULL, NULL },
	 */
	//{ "FS_BASE",			0xc0000100,	0xffffffffffffffffULL, NULL },
	//{ "GS_BASE",			0xc0000101, 	0xffffffffffffffffULL, NULL },
	{ "KERNEL_GS_BASE",		0xc0000102, 	0xffffffffffffffffULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_atom_MSRs[] = {
	{ "BIOS_UPDT_TRIG",		0x00000079,	0xffffffffffffffffULL, NULL },
	{ "MSR_FSB_FREQ",		0x000000cd,	0x0000000000000007ULL, NULL },
	{ "MSR_BBL_CR_CTL3",		0x0000011e,	0x0000000000800101ULL, NULL },
	{ "MSR_THERM2_CTL",		0x0000019d,	0x0000000000010000ULL, NULL },
	{ "MISC_ENABLE",		0x000001a0,	0x0000000400d53c89ULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_silvermont_MSRs[] = {
	{ "MSR_PMG_IO_CAPTURE_BASE",	0x000000e4,	0x000000000007ffffULL, NULL },
	{ "MSR_BBL_CR_CTL3",		0x0000011e,	0x0000000000800101ULL, NULL },
	{ "MSR_FEATURE_CONFIG",		0x0000013c,	0x0000000000000003ULL, NULL },
	{ "MISC_ENABLE",		0x000001a0,	0x0000004400c51889ULL, NULL },
	{ "MSR_TEMPERATURE_TARGET",	0x000001a2,	0x000000003fff0000ULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_nehalem_MSRs[] = {
	{ "BIOS_UPDT_TRIG",		0x00000079,	0xffffffffffffffffULL, NULL },
	{ "MSR_PLATFORM_INFO",		0x000000ce,	0x0000ff003001ff00ULL, NULL },
	{ "MSR_PKG_CST_CONFIG_CONTROL",	0x000000e2,	0x0000000007008407ULL, NULL },
	{ "MSR_PMG_IO_CAPTURE_BASE",	0x000000e4,	0x000000000007ffffULL, NULL },
	{ "MSR_TEMPERATURE_TARGET",	0x000001a2,	0x0000000000ff0000ULL, NULL },
	{ "MSR_OFFCORE_RSP_0",		0x000001a6,	0xffffffffffffffffULL, NULL },
	{ "MSR_MISC_PWR_MGMT",		0x000001aa,	0x0000000000000003ULL, NULL },
	{ "MSR_TURBO_POWER_CURRENT_LIMIT",0x000001ac,	0x00000000ffffffffULL, NULL },
	{ "MSR_TURBO_RATIO_LIMIT",	0x000001ad,	0x00000000ffffffffULL, NULL },
	{ "MSR_POWER_CTL",		0x000001fc,	0x0000000000000002ULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_sandybridge_MSRs[] = {
	{ "BIOS_UPDT_TRIG",		0x00000079,	0xffffffffffffffffULL, NULL },
	{ "MSR_PLATFORM_INFO",		0x000000ce,	0x0000ff003001ff00ULL, NULL },
	{ "MSR_PKG_CST_CONFIG_CONTROL",	0x000000e2,	0x0000000007008407ULL, NULL },
	{ "MSR_PMG_IO_CAPTURE_BASE",	0x000000e4,	0x000000000007ffffULL, NULL },
	{ "MSR_TEMPERATURE_TARGET",	0x000001a2,	0x0000000000ff0000ULL, NULL },
	{ "MSR_OFFCORE_RSP_0",		0x000001a6,	0xffffffffffffffffULL, NULL },
	{ "MSR_TURBO_RATIO_LIMIT",	0x000001ad,	0x00000000ffffffffULL, NULL },
	{ "MSR_POWER_CTL",		0x000001fc,	0x0000000000000002ULL, NULL },
	{ "MSR_PKGC3_IRTL",		0x0000060a,	0x0000000000009fffULL, NULL },
	{ "MSR_PKGC6_IRTL",		0x0000060b,	0x0000000000009fffULL, NULL },
	{ "MSR_PKGC7_IRTL",		0x0000060c,	0x0000000000009fffULL, NULL },
	{ "MSR_PKG_RAPL_POWER_LIMIT",	0x00000610,	0xffffffffffffffffULL, NULL },
	{ "MSR_PKG_RAPL_POWER_INFO",	0x00000614,	0xffffffffffffffffULL, NULL },
	{ "MSR_PP0_POWER_LIMIT",	0x00000638,	0xffffffffffffffffULL, NULL },
	{ "MSR_PP0_POLICY",		0x0000063a,	0xffffffffffffffffULL, NULL },
	{ "MSR_PP1_POWER_LIMIT",	0x00000640,	0xffffffffffffffffULL, NULL },
	{ "MSR_PP1_POLICY",		0x00000642,	0xffffffffffffffffULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_ivybridge_MSRs[] = {
	{ "MSR_PLATFORM_INFO",		0x000000ce,	0x00ffff073000ff00ULL, NULL },
	{ "MSR_PKG_CST_CONFIG_CONTROL",	0x000000e2,	0x000000001e008407ULL, NULL },
	{ "MSR_CONFIG_TDP_NOMINAL",	0x00000648,	0x00000000000000ffULL, NULL },
	{ "MSR_CONFIG_TDP_LEVEL1",	0x00000649,	0x7fff7fff00ff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_LEVEL2",	0x0000064a,	0x7fff7fff00ff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_CONTROL",	0x0000064b,	0x0000000080000003ULL, NULL },
	{ "MSR_TURBO_ACTIVATION_RATIO",	0x0000064c,	0x00000000800000ffULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_ivybridge_ep_MSRs[] = {
	{ "MSR_PLATFORM_INFO",		0x000000ce,	0x00ffff073000ff00ULL, NULL },
	{ "MSR_PKG_CST_CONFIG_CONTROL",	0x000000e2,	0x000000001e008407ULL, NULL },
	{ "MSR_ERROR_CONTROL",		0x0000017f,	0x0000000000000002ULL, NULL },
	/* Not sure about the following, commented out for the moment */
	/*
	{ "MSR_MC5_CTL",		0x00000414,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC5_STATUS",		0x00000415,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC5_ADDR",		0x00000416,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC5_MISC",		0x00000417,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC6_CTL",		0x00000418,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC6_STATUS",		0x00000419,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC6_ADDR",		0x0000041a,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC6_MISC",		0x0000041b,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC7_CTL",		0x0000041c,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC7_STATUS",		0x0000041d,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC7_ADDR",		0x0000041e,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC7_MISC",		0x0000041f,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC8_CTL",		0x00000420,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC8_STATUS",		0x00000421,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC8_ADDR",		0x00000422,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC8_MISC",		0x00000423,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC9_CTL",		0x00000424,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC9_STATUS",		0x00000425,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC9_ADDR",		0x00000426,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC9_MISC",		0x00000427,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC10_CTL",		0x00000428,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC10_STATUS",		0x00000429,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC10_ADDR",		0x0000042a,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC10_MISC",		0x0000042b,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC11_CTL",		0x0000042c,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC11_STATUS",		0x0000042d,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC11_ADDR",		0x0000042e,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC11_MISC",		0x0000042f,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC12_CTL",		0x00000430,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC12_STATUS",		0x00000431,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC12_ADDR",		0x00000432,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC12_MISC",		0x00000433,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC13_CTL",		0x00000434,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC13_STATUS",		0x00000435,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC13_ADDR",		0x00000436,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC13_MISC",		0x00000437,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC14_CTL",		0x00000438,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC14_STATUS",		0x00000439,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC14_ADDR",		0x0000043a,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC14_MISC",		0x0000043b,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC15_CTL",		0x0000043c,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC15_STATUS",		0x0000043d,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC15_ADDR",		0x0000043e,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC15_MISC",		0x0000043f,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC16_CTL",		0x00000440,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC16_STATUS",		0x00000441,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC16_ADDR",		0x00000442,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC16_MISC",		0x00000443,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC17_CTL",		0x00000444,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC17_STATUS",		0x00000445,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC17_ADDR",		0x00000446,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC17_MISC",		0x00000447,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC18_CTL",		0x00000448,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC18_STATUS",		0x00000449,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC18_ADDR",		0x0000044a,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC18_MISC",		0x0000044b,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC19_CTL",		0x0000044c,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC19_STATUS",		0x0000044d,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC19_ADDR",		0x0000044e,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC19_MISC",		0x0000044f,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC20_CTL",		0x00000450,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC20_STATUS",		0x00000451,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC20_ADDR",		0x00000452,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC20_MISC",		0x00000453,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC21_CTL",		0x00000454,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC21_STATUS",		0x00000455,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC21_ADDR",		0x00000456,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC21_MISC",		0x00000457,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC22_CTL",		0x00000458,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC22_STATUS",		0x00000459,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC22_ADDR",		0x0000045a,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC22_MISC",		0x0000045b,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC23_CTL",		0x0000045c,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC23_STATUS",		0x0000045d,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC23_ADDR",		0x0000045e,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC23_MISC",		0x0000045f,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC24_CTL",		0x00000460,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC24_STATUS",		0x00000461,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC24_ADDR",		0x00000462,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC24_MISC",		0x00000463,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC25_CTL",		0x00000464,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC25_STATUS",		0x00000465,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC25_ADDR",		0x00000466,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC25_MISC",		0x00000467,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC26_CTL",		0x00000468,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC26_STATUS",		0x00000469,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC26_ADDR",		0x0000046a,	0xffffffffffffffffULL, NULL },
	{ "MSR_MC26_MISC",		0x0000046b,	0xffffffffffffffffULL, NULL },
	*/
	{ "MSR_PKG_PERF_STATUS",	0x00000613,	0x00000000ffffffffULL, NULL },
	{ "MSR_DRAM_POWER_LIMIT",	0x00000618,	0x0000000080ffffffULL, NULL },
	{ "MSR_DRAM_ENERGY_STATUS",	0x00000619,	0x00000000ffffffffULL, NULL },
	{ "MSR_DRAM_PERF_STATUS",	0x0000061b,	0x00000000ffffffffULL, NULL },
	{ "MSR_DRAM_POWER_INFO",	0x0000061c,	0x00ff7fff7fff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_NOMINAL",	0x00000648,	0x00000000000000ffULL, NULL },
	{ "MSR_CONFIG_TDP_LEVEL1",	0x00000649,	0x7fff7fff00ff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_LEVEL2",	0x0000064a,	0x7fff7fff00ff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_CONTROL",	0x0000064b,	0x0000000080000003ULL, NULL },
	{ "MSR_TURBO_ACTIVATION_RATIO",	0x0000064c,	0x00000000800000ffULL, NULL },
	{ NULL,				0x00000000,	0, NULL },
};

static const msr_info IA32_haswell_MSRs[] = {
	{ "MSR_PLATFORM_INFO",		0x000000ce,	0x00ffff073000ff00ULL, NULL },
	//{ "IA32_VMX_FMFUNC",		0x00000491,	0x0000000000000000ULL, NULL },
	{ "MSR_CONFIG_TDP_NOMINAL",	0x00000648,	0x00000000000000ffULL, NULL },
	{ "MSR_CONFIG_TDP_LEVEL1",	0x00000649,	0x7fff7fff00ff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_LEVEL2",	0x0000064a,	0x7fff7fff00ff7fffULL, NULL },
	{ "MSR_CONFIG_TDP_CONTROL",	0x0000064b,	0x0000000080000003ULL, NULL },
	{ "MSR_TURBO_ACTIVATION_RATIO", 0x0000064c,	0x00000000800000ffULL, NULL },
	{ "MSR_PKG_C8_RESIDENCY",	0x00000630,	0x0fffffffffffffffULL, NULL },
	{ "MSR_PKG_C9_RESIDENCY",	0x00000630,	0x0fffffffffffffffULL, NULL },
	{ "MSR_PKG_C10_RESIDENCY",	0x00000630,	0x0fffffffffffffffULL, NULL },
	{ "MSR_SMM_MCA_CAP",		0x0000017d,	0x0c00000000000000ULL, NULL },
	{ "MSR_TURBO_RATIO_LIMIT",	0x000001ad,	0x00000000ffffffffULL, NULL },
	{ "MSR_UNC_PERF_GLOBAL_CTRL",	0x00000391,	0x00000000e000001fULL, NULL },
	//{ "MSR_UNC_PERF_GLOBAL_STATUS",0x00000392,	0x000000000000000fULL, NULL },
	{ "MSR_UNC_PERF_FIXED_CTRL",	0x00000394,	0x0000000005000000ULL, NULL },
	//{ "MSR_UNC_PERF_FIXED_CTR",	0x00000395,	0x0000ffffffffffffULL, NULL },
	{ "MSR_UNC_CB0_CONFIG",		0x00000396,	0x000000000000000fULL, NULL },
	//{ "MSR_UNC_ARB_PER_CTR0",	0x000003b0,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_ARB_PER_CTR1",	0x000003b1,	0xffffffffffffffffULL, NULL },

	//{ "MSR_UNC_ARB_PERFEVTSEL0",	0x000003b2,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_ARB_PERFEVTSEL1",	0x000003b3,	0xffffffffffffffffULL, NULL },
	{ "MSR_UNC_PERF_GLOBAL_CTRL",	0x00000391,	0x00000000e000000fULL, NULL },
	{ "MSR_UNC_PERF_FIXED_CTR",	0x00000395,	0x0000ffffffffffffULL, NULL },
	{ "MSR_SMM_FEATURE_CONTROL",	0x000004e0,	0x0000000000000005ULL, NULL },
	{ "MSR_SMM_DELAYED",		0x000004e2,	0xffffffffffffffffULL, NULL },
	{ "MSR_SMM_BLOCKED",		0x000004e3,	0xffffffffffffffffULL, NULL },
	{ "MSR_PP1_POWER_LIMIT",	0x00000640,	0x0000000080ffffffULL, NULL },
	{ "MSR_PP1_ENERGY_STATUS",	0x00000641,	0x00000000ffffffffULL, NULL },
	{ "MSR_PP1_POLICY",		0x00000652,	0x000000000000000fULL, NULL },
	//{ "MSR_UNC_CB0_0_PERFEVTSEL0"	0x00000700,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_0_PERFEVTSEL1",0x00000701,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_0_PER_CTR0",	0x00000706,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_0_PER_CTR0",	0x00000707,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_1_PERFEVTSEL0"	0x00000710,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_1_PERFEVTSEL1",0x00000711,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_1_PER_CTR0",	0x00000716,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_1_PER_CTR0",	0x00000717,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_1_PERFEVTSEL0"	0x00000720,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_1_PERFEVTSEL1",0x00000721,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_1_PER_CTR0",	0x00000726,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_1_PER_CTR0",	0x00000727,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_1_PERFEVTSEL0"	0x00000730,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_1_PERFEVTSEL1",0x00000731,	0xffffffffffffffffULL, NULL },
	//{ "MSR_UNC_CB0_1_PER_CTR0",	0x00000736,	0xffffffffffffffffULL, NULL },`
	//{ "MSR_UNC_CB0_1_PER_CTR0",	0x00000737,	0xffffffffffffffffULL, NULL },`
	{ NULL,				0x00000000,	0, NULL },
};

static int msr_table_check(fwts_framework *fw, const msr_info *const info)
{
	int i;

	for (i = 0; info[i].name != NULL; i++)
		msr_consistent_check(fw, LOG_LEVEL_MEDIUM,
			info[i].name, info[i].msr, 0, info[i].mask, info[i].callback);

	return FWTS_OK;
}

static int msr_cpu_generic(fwts_framework *fw)
{
	if (intel_cpu)
		msr_table_check(fw, IA32_MSRs);
	else if (amd_cpu || hygon_cpu)
		msr_table_check(fw, AMD_MSRs);
	else
		fwts_skipped(fw, "Not an AMD or Intel CPU, test skipped.");

	return FWTS_OK;
}

typedef struct {
	const char *const microarch;
	const uint8_t family;
	const uint8_t model;
	const msr_info *const info;
} cpu_to_msr;

/*
 * Intel's CPU-specific MSRs can be found in
 * Intel® 64 and IA-32 ArchitecturesSoftware Developer’s Manual Volume 4
 */
static const cpu_to_msr cpu_msr_map[] = {
	{ "Pentium",			0x05, 0x01,	NULL },
	{ "Pentium",			0x05, 0x02,	NULL },
	{ "Pentium",			0x05, 0x04,	NULL },
	{ "Pentium Pro",		0x06, 0x01,	NULL },
	{ "Pentium II",			0x06, 0x03,	NULL },
	{ "Pentium II",			0x06, 0x05,	NULL },
	{ "Pentium III",		0x06, 0x07,	NULL },
	{ "Pentium III",		0x06, 0x08,	NULL },
	{ "Pentium M",			0x06, 0x09,	NULL },
	{ "Pentium III",		0x06, 0x0a,	NULL },
	{ "Pentium III",		0x06, 0x0b,	NULL },
	{ "Pentium 4, Xeon",		0x06, 0x00,	NULL },
	{ "Pentium 4, Xeon",		0x0f, 0x01,	NULL },
	{ "Pentium 4, Xeon",		0x0f, 0x02,	NULL },
	{ "Pentium 4, Pentium D, Xeon", 0x0f, 0x03,	NULL },
	{ "Pentium 4, Pentium D, Xeon", 0x0f, 0x04,	NULL },
	{ "Pentium 4, Pentium D, Xeon", 0x0f, 0x06,	NULL },
	{ "Nehalem",			0x06, 0x1a,	IA32_nehalem_MSRs },
	{ "Nehalem",			0x06, 0x1e,	IA32_nehalem_MSRs },
	{ "Nehalem",			0x06, 0x1f,	IA32_nehalem_MSRs },
	{ "Nehalem",			0x06, 0x2e,	IA32_nehalem_MSRs },
	{ "Westmere",			0x06, 0x25,	IA32_nehalem_MSRs },
	{ "Westmere",			0x06, 0x2c,	IA32_nehalem_MSRs },
	{ "Westmere",			0x06, 0x2f,	IA32_nehalem_MSRs },
	{ "Atom",			0x06, 0x1c,	IA32_atom_MSRs },
	{ "Atom",			0x06, 0x26,	IA32_atom_MSRs },
	{ "Atom",			0x06, 0x27,	IA32_atom_MSRs },
	{ "Atom",			0x06, 0x35,	IA32_atom_MSRs },
	{ "Atom",			0x06, 0x36,	IA32_atom_MSRs },
	{ "Atom",			0x06, 0x37,	IA32_silvermont_MSRs },
	{ "Atom",			0x06, 0x4a,	IA32_silvermont_MSRs },
	{ "Atom",			0x06, 0x4d,	IA32_silvermont_MSRs },
	{ "Atom",			0x06, 0x5a,	IA32_silvermont_MSRs },
	{ "Atom",			0x06, 0x5d,	IA32_silvermont_MSRs },
	{ "Sandybridge",		0x06, 0x2a,	IA32_sandybridge_MSRs },
	{ "Sandybridge",		0x06, 0x2d,	IA32_sandybridge_MSRs },
	{ "Ivybridge",			0x06, 0x3a, 	IA32_ivybridge_MSRs },
	{ "Ivybridge-EP",		0x06, 0x3e, 	IA32_ivybridge_ep_MSRs },
	{ "Haswell",			0x06, 0x3c,	IA32_haswell_MSRs },
	{ "Haswell",			0x06, 0x45,	IA32_haswell_MSRs },
	{ "Haswell",			0x06, 0x46,	IA32_haswell_MSRs },

	{ NULL,				0x00, 0x00,	NULL }
};

static int msr_cpu_specific(fwts_framework *fw)
{
	if (intel_cpu) {
		int i;
		bool checked = false;


		for (i = 0; cpu_msr_map[i].microarch; i++) {
			if ((cpu_msr_map[i].model == cpuinfo->x86_model) &&
			    (cpu_msr_map[i].family == cpuinfo->x86) &&
			    (cpu_msr_map[i].info)) {
				fwts_log_info(fw, "CPU family: 0x%x, model: 0x%x (%s)",
					cpu_msr_map[i].family,
					cpu_msr_map[i].model,
					cpu_msr_map[i].microarch);
				msr_table_check(fw, cpu_msr_map[i].info);
				checked = true;
			}
		}

		if (!checked)
			fwts_log_info(fw, "No model specific tests for model 0x%x.", cpuinfo->x86_model);
	} else
		fwts_skipped(fw, "Non-Intel CPU, test skipped.");

	return FWTS_OK;
}

static fwts_framework_minor_test msr_tests[] = {
	{ msr_cpu_generic,		"Test CPU generic MSRs." },
	{ msr_cpu_specific,		"Test CPU specific model MSRs." },
	{ msr_pstate_ratios, 		"Test all P State Ratios." },
	{ msr_c1_c3_autodemotion, 	"Test C1 and C3 autodemotion." },
	{ msr_smrr,			"Test SMRR MSR registers." },
	{ NULL, NULL }
};

static fwts_framework_ops msr_ops = {
	.description = "MSR register tests.",
	.init        = msr_init,
	.deinit	     = msr_deinit,
	.minor_tests = msr_tests
};

FWTS_REGISTER("msr", &msr_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
