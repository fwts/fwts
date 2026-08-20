/*
 *
 * Copyright (C) 2025 Canonical
 * Copyright (C) 2025 Arm
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

#ifdef FWTS_ARCH_AARCH64

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "smccc_test.h"
#include "fwts_smccc.h"

/*
 * ARM PSCI tests,
 *   https://developer.arm.com/documentation/den0022/latest/
 */

#define	PSCI_VERSION            0x84000000
#define	CPU_SUSPEND             0x84000001
#define	CPU_OFF                 0x84000002
#define	CPU_ON                  0x84000003
#define	AFFINITY_INFO           0x84000004
#define	MIGRATE                 0x84000005
#define	MIGRATE_INFO_TYPE       0x84000006
#define	MIGRATE_INFO_UP_CPU     0x84000007
#define	SYSTEM_OFF              0x84000008
#define	SYSTEM_RESET            0x84000009
#define	PSCI_FEATURES           0x8400000A
#define	CPU_FREEZE              0x8400000B
#define	CPU_DEFAULT_SUSPEND     0x8400000C
#define	NODE_HW_STATE           0x8400000D
#define	SYSTEM_SUSPEND          0x8400000E
#define	PSCI_SET_SUSPEND_MODE   0x8400000F
#define	PSCI_STAT_RESIDENCY     0x84000010
#define	PSCI_STAT_COUNT         0x84000011
#define	SYSTEM_RESET2           0x84000012
#define	MEM_PROTECT             0x84000013
#define	MEM_PROTECT_CHECK_RANGE 0x84000014
#define	SYSTEM_OFF2             0x84000015

#define PSCI_VERSION_0_2 0x00002
#define PSCI_VERSION_1_0 0x10000
#define PSCI_VERSION_1_1 0x10001
#define PSCI_VERSION_1_2 0x10002

enum PSCI_ret_code {
    PSCI_SUCCESS = 0,
    PSCI_NOT_SUPPORTED = -1
};

typedef struct {
	const uint32_t	PSCI_func_id;
	const char	*PSCI_func_id_name;
	bool		implemented;
} PSCI_func_id_t;

static PSCI_func_id_t PSCI_func_id_list[] = {
    { PSCI_VERSION,             "PSCI_VERSION",             false },
    { CPU_SUSPEND,              "CPU_SUSPEND",              false },
    { CPU_OFF,                  "CPU_OFF",                  false },
    { CPU_ON,                   "CPU_ON",                   false },
    { AFFINITY_INFO,            "AFFINITY_INFO",            false },
    { MIGRATE,                  "MIGRATE",                  false },
    { MIGRATE_INFO_TYPE,        "MIGRATE_INFO_TYPE",        false },
    { MIGRATE_INFO_UP_CPU,      "MIGRATE_INFO_UP_CPU ",     false },
    { SYSTEM_OFF,               "SYSTEM_OFF",               false },
    { SYSTEM_RESET,             "SYSTEM_RESET",             false },
    { PSCI_FEATURES,            "PSCI_FEATURES",            false },
    { CPU_FREEZE,               "CPU_FREEZE",               false },
    { CPU_DEFAULT_SUSPEND,      "CPU_DEFAULT_SUSPEND",      false },
    { NODE_HW_STATE,            "NODE_HW_STATE",            false },
    { SYSTEM_SUSPEND,           "SYSTEM_SUSPEND",           false },
    { PSCI_SET_SUSPEND_MODE,    "PSCI_SET_SUSPEND_MODE",    false },
    { PSCI_STAT_RESIDENCY,      "PSCI_STAT_RESIDENCY",      false },
    { PSCI_STAT_COUNT,          "PSCI_STAT_COUNT",          false },
    { SYSTEM_RESET2,            "SYSTEM_RESET2",            false },
    { MEM_PROTECT,              "MEM_PROTECT",              false },
    { MEM_PROTECT_CHECK_RANGE,  "MEM_PROTECT_CHECK_RANGE",  false },
    { SYSTEM_OFF2,              "SYSTEM_OFF2",              false }
};

static uint16_t psci_major_version = 0;
static uint16_t psci_minor_version = 0;
static uint32_t psci_version = 0;

/*
 *  check_for_psci_support()
 *  This test checks if PSCI is supported in the system by checking the FADT table
 */
static int check_for_psci_support(fwts_framework *fw)
{
    static const fwts_acpi_gas null_gas;
    fwts_acpi_table_info *table = NULL;
    const fwts_acpi_table_fadt *fadt;
    int fadt_size;
    bool passed = true;

    if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
        fwts_log_error(fw, "Cannot read ACPI table FACP.");
        return FWTS_ERROR;
    }
    if (table == NULL) {
        fwts_log_error(fw, "ACPI table FACP does not exist!");
        return FWTS_ERROR;
    }

    fadt = (const fwts_acpi_table_fadt *)table->data;
    fadt_size = table->length;
    if (fadt_size == 0) {
        fwts_log_error(fw, "ACPI table FACP has zero length!");
        return FWTS_ERROR;
    }

    bool PSCI_compliant = fadt->arm_boot_flags & FWTS_FACP_ARM_BOOT_ARCH_PSCI_COMPLIANT;
    if (PSCI_compliant) {
        fwts_log_info_verbatim(fw, "Arm PSCI: FADT ARM_BOOT_ARCH PSCI compliant flag is set");
    } else {
        fwts_log_error(fw, "Arm PSCI: FADT ARM_BOOT_ARCH PSCI compliant flag is not set."
            " PSCI is not supported");
       return FWTS_ERROR;
    }

    bool HVC_Conduit = fadt->arm_boot_flags & FWTS_FACP_ARM_BOOT_ARCH_PSCI_USE_HVC;
    if (HVC_Conduit) {
        fwts_log_info_verbatim(fw, "Arm PSCI: PSCI uses HVC conduit");
    } else {
        fwts_log_info_verbatim(fw, "Arm PSCI: PSCI uses SMC conduit");
    }

    if (memcmp((const void *)&fadt->sleep_control_reg,
        (const void *)&null_gas,
        sizeof(fwts_acpi_gas))) {
        passed = false;
        fwts_log_error(fw, "Arm PSCI: FADT SLEEP_CONTROL_REG is not zeroed (Recommended)"
            ". Non-zero general register structure detected. Must be coded as all zeros");
    }

    if (memcmp((const void *)&fadt->sleep_status_reg,
           (const void *)&null_gas,
            sizeof(fwts_acpi_gas))) {
        passed = false;
        fwts_log_error(fw, "Arm PSCI: FADT SLEEP_STATUS_REG is not zeroed (Recommended)"
            ". Non-zero general register structure detected. Must be coded as all zeros");
    }

    if (passed == false)
    {
        fwts_failed(fw, LOG_LEVEL_MEDIUM, "SleepNonZero",
            "FADT SLEEP_* registers field are non-zero.");
        return (FWTS_ERROR);
    }

    fwts_passed(fw, "Check for Arm PSCI support test passed");
    return FWTS_OK;
}


/*
 *  psci_version_test()
 * test PSCI function VERSION
 */
static int psci_version_test(fwts_framework *fw)
{
    int ret;
    struct smccc_test_arg arg = { };

    arg.size = sizeof(arg);
    arg.w[0] = PSCI_VERSION;

    ret = ioctl(smccc_fd, SMCCC_TEST_PSCI_VERSION, &arg);
    if (ret < 0) {
        fwts_log_error(fw, "SMCCC test driver ioctl PSCI_TEST_VERSION "
            "failed, errno=%d (%s)\n", errno, strerror(errno));
        return FWTS_ERROR;
    }
    if (smccc_pci_conduit_check(fw, &arg) != FWTS_OK)
        return FWTS_ERROR;

    fwts_log_info_verbatim(fw, "  SMCCC conduit type: 0x%" PRIx32 " ('%s')",
        arg.conduit, smccc_pci_conduit_name(&arg));

    psci_major_version = arg.w[0] >> 16;
    psci_minor_version = arg.w[0] & 0x0000ffff;
    fwts_log_info_verbatim(fw, "Arm PSCI: Major version: %" PRIu16 " , Minor version: %" PRIu16 " ",
        psci_major_version, psci_minor_version);

    psci_version = (psci_major_version << 16) + psci_minor_version;

    fwts_passed(fw, "Arm PSCI_VERSION test passed");
    return FWTS_OK;
}


/*
 * get_implementation_status()
 * Query the implementation status by function_id
*/
static bool get_implementation_status(const uint32_t func_id)
{
    int k=0;
    for (k=0; k < FWTS_ARRAY_SIZE(PSCI_func_id_list); k++) {
        if (PSCI_func_id_list[k].PSCI_func_id == func_id)
            return (PSCI_func_id_list[k].implemented);
    }

    return false;
}

/*
 * psci_features_bbr_check()
 * Asserts based on the BBR specification rules in
 * Section 4.2 which refers to the table 6.9 in the
 * PSCI specification https://developer.arm.com/documentation/den0022/latest/
 * This function can be run only if SBBR or EBBR flag is set
 * while running FWTS
 */

 static int psci_features_bbr_check(fwts_framework *fw)
 {
    bool bbr_check_failed = false;
    uint16_t assert_failed_count = 0;
    int i=0;

    fwts_log_info_verbatim(fw, "Arm BBR PSCI implementation check - the detected PSCI version is %" PRIu16 ".%" PRIu16 "",
        psci_major_version, psci_minor_version);

    for (i=0; i < FWTS_ARRAY_SIZE(PSCI_func_id_list); i++) {
        if(PSCI_func_id_list[i].implemented == false) {
            switch (PSCI_func_id_list[i].PSCI_func_id) {
                case PSCI_VERSION:
                case CPU_SUSPEND:
                case CPU_OFF:
                case CPU_ON:
                case AFFINITY_INFO:
                case SYSTEM_OFF:
                case SYSTEM_RESET: {
                    /* These PSCI functions are mandatory for all versions */
                    bbr_check_failed = true;
                }
                break;

                case PSCI_FEATURES: {
                    /* PSCI_FEATURES is mandatory for all PSCI versions >= 1.0 */
                    if (psci_version >= PSCI_VERSION_1_0)
                    {
                        bbr_check_failed = true;
                    }
                }
                break;

                default: {
                    fwts_log_info_verbatim(fw, "Info: \"Optional\" Function: 0x%8.8" PRIx32 " (%s) is not Implemented.",
                        PSCI_func_id_list[i].PSCI_func_id,
                        PSCI_func_id_list[i].PSCI_func_id_name);
                }
            }
            if (bbr_check_failed == true) {
                fwts_log_error(fw, "failed: As per Arm BBR specifciation, "
                    "the implementation of %s is \"Mandatory\" in PSCI v%d.%d but is not implemented"
                    , PSCI_func_id_list[i].PSCI_func_id_name
                    , psci_major_version, psci_minor_version);
                bbr_check_failed = false;

                assert_failed_count++;
            }
        }

        /* Additional checks */
        /* Some PSCI functions are mandatory conditionally. Such cases are checked below */
        if (PSCI_func_id_list[i].implemented == true) {
            switch (PSCI_func_id_list[i].PSCI_func_id) {
                case MIGRATE: {
                    /* If MIGRATE is implemented then MIGRATE_INFO_UP_CPU is mandatory */
                    if (false == get_implementation_status(MIGRATE_INFO_UP_CPU)) {
                        fwts_log_error(fw, "failed: MIGRATE is implemented however MIGRATE_INFO_UP_CPU"
                            " is not implemented");
                        assert_failed_count++;
                    }
                }
                break;

                case PSCI_STAT_RESIDENCY: {
                    /* If PSCI_STAT_RESIDENCY is implemented then PSCI_STAT_COUNT is mandatory */
                    if (false == get_implementation_status(PSCI_STAT_COUNT)) {
                        fwts_log_error(fw, "failed: PSCI_STAT_RESIDENCY is implemented however PSCI_STAT_COUNT"
                            " is not implemented");
                        assert_failed_count++;
                    }
                }
                break;

                case PSCI_STAT_COUNT: {
                    /* If PSCI_STAT_COUNT is implemented then PSCI_STAT_RESIDENCY is mandatory */
                    if (false == get_implementation_status(PSCI_STAT_RESIDENCY)) {
                        fwts_log_error(fw, "failed: PSCI_STAT_COUNT is implemented however PSCI_STAT_RESIDENCY"
                            " is not implemented");
                        assert_failed_count++;
                    }
                }
                break;

                case MEM_PROTECT: {
                    /* If MEM_PROTECT is implemented then MEM_PROTECT_CHECK_RANGE is mandatory */
                    if (false == get_implementation_status(MEM_PROTECT_CHECK_RANGE)) {
                        fwts_log_error(fw, "failed: MEM_PROTECT is implemented however MEM_PROTECT_CHECK_RANGE"
                            " is not implemented");
                        assert_failed_count++;
                    }
                }
                break;

                case MEM_PROTECT_CHECK_RANGE: {
                    /* If MEM_PROTECT_CHECK_RANGE is implemented then MEM_PROTECT is mandatory */
                    if (false == get_implementation_status(MEM_PROTECT)) {
                        fwts_log_error(fw, "failed: MEM_PROTECT_CHECK_RANGE is implemented however MEM_PROTECT"
                            " is not implemented");
                        assert_failed_count++;
                    }
                }
                break;
            }

        }
    }

    if (assert_failed_count > 0) {
        fwts_log_error(fw, "Arm SBBR check for Arm PSCI_FEATURES failed. %d checks failed",
            assert_failed_count);
        return FWTS_ERROR;
    }

    fwts_log_info_verbatim(fw, "Arm SBBR check for Arm PSCI_FEATURES test passed");
    return FWTS_OK;
 }


/*
 *  psci_features_test()
 *  test PSCI function PSCI_FEATURES for each PSCI function
 */
 static int psci_features_test(fwts_framework *fw)
 {
    int ret;
    int i = 0;
    struct smccc_test_arg arg = { };
    bool feaatures_test_failed = 0;

    if (psci_version == PSCI_VERSION_0_2) {
        fwts_skipped(fw, "PSCI_FEATURES is Not Applicable for PSCI version %" PRIu16 ".%" PRIu16 "",
            psci_major_version, psci_minor_version);
        return FWTS_SKIP;
    }

    fwts_log_info_verbatim(fw, "========================= PSCI_FEATURES query ===========================");
    fwts_log_info_verbatim(fw, " #       Function Name                   Function ID     Implementation Status");
    for (i = 0; i < FWTS_ARRAY_SIZE(PSCI_func_id_list); i++) {
        memset(&arg, 0, sizeof(arg));
        arg.size = sizeof(arg);
        arg.w[0] = PSCI_FEATURES;

        /* arg.w[1] should contain, the function id of the
           PSCI function to query about */
        arg.w[1] = PSCI_func_id_list[i].PSCI_func_id;

        ret = ioctl(smccc_fd, SMCCC_TEST_PSCI_FEATURES, &arg);
        if (ret < 0) {
            fwts_log_error(fw, "SMCCC test driver ioctl PSCI_FEATURES "
                "failed, ret=%d, errno=%d (%s)\n", ret, errno, strerror(errno));
            return FWTS_ERROR;
        }
        if (smccc_pci_conduit_check(fw, &arg) != FWTS_OK)
            return FWTS_ERROR;

        int32_t return_value = arg.w[0];

        if (return_value == PSCI_NOT_SUPPORTED) {
            fwts_log_info_verbatim(fw, "%2d       %-24s        0x%8.8" PRIx32"      NOT IMPLEMENTED",
                i+1,
                PSCI_func_id_list[i].PSCI_func_id_name,
                PSCI_func_id_list[i].PSCI_func_id);
            PSCI_func_id_list[i].implemented = false;
        } else if (((arg.w[0] >> 31) & 0x1) == 0) { /* if bit 31 is 0, then the function is supported */
            fwts_log_info_verbatim(fw, "%2d       %-24s        0x%8.8" PRIx32"      IMPLEMENTED",
                i+1,
                PSCI_func_id_list[i].PSCI_func_id_name,
                PSCI_func_id_list[i].PSCI_func_id);
            PSCI_func_id_list[i].implemented = true;
        } else if (((arg.w[0] >> 31) & 0x1) == 1) { /* if bit 31 is 1, then the implementation is incorrect as per spec */
            fwts_log_info_verbatim(fw, "%2d       %-24s        0x%8.8" PRIx32"    "
                "  Error Invalid return - Please check the implementation",
                i+1,
                PSCI_func_id_list[i].PSCI_func_id_name,
                PSCI_func_id_list[i].PSCI_func_id);
                PSCI_func_id_list[i].implemented = false;
            feaatures_test_failed = 1;
        }
    }
    fwts_log_nl(fw);
    /* if --sbbr flag or --ebbr is set, additionally call the BBR assertions function */
    if (fw->flags & FWTS_FLAG_SBBR || fw->flags & FWTS_FLAG_EBBR) {
        if (psci_features_bbr_check(fw) == FWTS_ERROR) {
            fwts_failed(fw, LOG_LEVEL_HIGH, "psci_features_bbr_check check ",
                "for Arm BBR specification rules failed.\n");
            return (FWTS_ERROR);
        }
    }

    if (feaatures_test_failed == 1) {
        fwts_failed(fw, LOG_LEVEL_HIGH, "Arm Arm PSCI_FEATURES test", "failed");
        return FWTS_ERROR;
    }

    fwts_passed(fw, "Arm PSCI_FEATURES test passed");
    return FWTS_OK;
 }

static inline uint64_t read_mpidr_el1(void)
{
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
    return mpidr;
}

static int affinity_info_test(fwts_framework *fw)
{
    int ret;
    struct smccc_test_arg arg = { };

    uint64_t mpidr = read_mpidr_el1();
    fwts_log_info_verbatim(fw, "Info: mpidr = 0x%16.16" PRIx64"", mpidr);


    uint32_t aff0 = mpidr & 0xFF;
    uint32_t aff1 = (mpidr >> 8) & 0xFF;
    uint32_t aff2 = (mpidr >> 16) & 0xFF;
    /* aff3 is unused */

    uint32_t target_affinity = 0;
    target_affinity |= aff0;
    target_affinity |= (aff1 << 8);
    target_affinity |= (aff2 << 16);

    fwts_log_info_verbatim(fw, "Info: target_affinity = 0x%8.8" PRIx32"", target_affinity);

    uint32_t lowest_affinity_level = 0; /* All affinity levels are valid */

    arg.size = sizeof(arg);
    arg.w[0] = AFFINITY_INFO;
    arg.w[1] = target_affinity;
    arg.w[2] = lowest_affinity_level;

    ret = ioctl(smccc_fd, SMCCC_TEST_AFFINITY_INFO, &arg);
    if (ret < 0) {
        fwts_log_error(fw, "SMCCC test driver ioctl SMCCC_TEST_AFFINITY_INFO "
            "failed, errno=%d (%s)\n", errno, strerror(errno));
        return FWTS_ERROR;
    }
    if (smccc_pci_conduit_check(fw, &arg) != FWTS_OK)
        return FWTS_ERROR;

    fwts_log_info_verbatim(fw, "  SMCCC conduit type: 0x%x ('%s')",
        arg.conduit, smccc_pci_conduit_name(&arg));

    int32_t result = arg.w[0];
    switch(result) {
        case 2: {
            fwts_log_info_verbatim(fw,
                "Test AFFINITY_INFO returned value = %d (ON_PENDING)", result);
        }
        break;
        case 1: {
            fwts_log_info_verbatim(fw,
                "Test AFFINITY_INFO returned value = %d (OFF)", result);
        }
        break;
        case 0: {
            fwts_log_info_verbatim(fw,
                "Test AFFINITY_INFO returned value = %d (ON)", result);
        }
        break;
        case -2: {
            fwts_log_info_verbatim(fw,
                "Test AFFINITY_INFO returned value = %d (INVALID_PARAMETERS)", result);
        }
        break;
        case -8: {
            fwts_log_info_verbatim(fw,
                "Test AFFINITY_INFO returned value = %d (DISABLED)", result);
        }
        break;
        default: {
            fwts_log_info_verbatim(fw,
                "Test AFFINITY_INFO returned value = %d , an unexpected value", result);
            fwts_failed(fw, LOG_LEVEL_HIGH, "Arm PSCI AFFINITY_INFO ",
                "test failed");
            return FWTS_ERROR;
        }
    }

    fwts_passed(fw, "Arm PSCI AFFINITY_INFO test passed");
    return FWTS_OK;
}

static fwts_framework_minor_test psci_tests[] = {
    { check_for_psci_support,   "Check for Arm PSCI support" },
    { psci_version_test,        "Test PSCI_VERSION" },
    { psci_features_test,       "Test PSCI_FEATURES" },
    { affinity_info_test,       "Test AFFINITY_INFO" },
    { NULL, NULL }
};

static fwts_framework_ops psciops = {
    .description = "ARM64 PSCI tests.",
    .init        = smccc_init,
    .deinit      = smccc_deinit,
    .minor_tests = psci_tests
};

FWTS_REGISTER("psci", &psciops, FWTS_TEST_ANYTIME, FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV)

#endif
