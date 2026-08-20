/*
 * Copyright (C) 2013-2026 Canonical
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

#if defined(FWTS_HAS_UEFI)

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "fwts_uefi.h"
#include "fwts_efi_runtime.h"
#include "fwts_efi_module.h"

#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET 0x00010000
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE 0x00020000
#define CAPSULE_FLAGS_INITIATE_RESET 0x00040000

static uint32_t uefi_get_mn_count_multiple = 50;
#define UEFI_GET_NEXT_HIGH_MN_COUNT_MULTIPLE_MAX	(10000)

#define EFI_CAPSULE_GUID \
{ \
	0x3B6686BD, 0x0D76, 0x4030, {0xB7, 0x0E, 0xB5, \
						0x51, 0x9E, 0x2F, 0xC5, 0xA0} \
}

static int fd;
static EFI_GUID gEfiCapsuleHeaderGuid = EFI_CAPSULE_GUID;

static uint32_t runtimeservicessupported;

static int uefirtmisc_init(fwts_framework *fw)
{
	if (fwts_lib_efi_runtime_module_init(fw, &fd) == FWTS_ABORTED)
		return FWTS_ABORTED;

	fwts_uefi_rt_support_status_get(fd, &runtimeservicessupported);

	return FWTS_OK;
}

static int uefirtmisc_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_lib_efi_runtime_close(fd);
	fwts_lib_efi_runtime_unload_module(fw);

	return FWTS_OK;
}

static int getnexthighmonotoniccount_test(fwts_framework *fw, uint32_t multitesttime)
{
	uint64_t status;
	struct efi_getnexthighmonotoniccount getnexthighmonotoniccount;
	uint32_t highcount;
	uint32_t i;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_HIGH_MONOTONIC_COUNT)) {
		fwts_skipped(fw, "Skipping test, GetNextHighMonotonicCount "
			"runtime service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getnexthighmonotoniccount.HighCount = &highcount;
	getnexthighmonotoniccount.status = &status;

	for (i = 0; i < multitesttime; i++) {
		status = ~0ULL;
		long ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTHIGHMONOTONICCOUNT, &getnexthighmonotoniccount);

		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Skipping test, GetNextHighMonotonicCount runtime "
					"service is not supported on this platform.");
				return FWTS_SKIP;
			}
			if (status == EFI_OUT_OF_RESOURCES) {
				fwts_skipped(fw, "Skipping test, run out of resources for "
					"GetNextHighMonotonicCount runtime service test.");
				return FWTS_SKIP;
			}
			fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetNextHighMonotonicCount",
				"Failed to get high monotonic count with UEFI runtime service.");
			fwts_uefi_print_status_info(fw, status);
			return FWTS_ERROR;
		}
	}

	return FWTS_OK;
}

static int querycapsulecapabilities_test(fwts_framework *fw, uint32_t multitesttime, uint32_t flag)
{
	uint64_t status;
	uint32_t i;

	struct efi_querycapsulecapabilities querycapsulecapabilities;
	EFI_RESET_TYPE resettype;
	EFI_CAPSULE_HEADER *pcapsuleheaderarray[2];
	EFI_CAPSULE_HEADER capsuleheader;
	uint64_t maxcapsulesize;
	uint64_t capsulecount;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_QUERY_CAPSULE_CAPABILITIES)) {
		fwts_skipped(fw, "Skipping test, QueryCapsuleCapabilities "
			"runtime service is not supported on this platform.");
		return FWTS_SKIP;
	}

	memset(&capsuleheader, 0, sizeof(capsuleheader));

	pcapsuleheaderarray[0] = &capsuleheader;
	pcapsuleheaderarray[1] = NULL;
	pcapsuleheaderarray[0]->CapsuleGuid = gEfiCapsuleHeaderGuid;
	pcapsuleheaderarray[0]->CapsuleImageSize = sizeof(EFI_CAPSULE_HEADER);
	pcapsuleheaderarray[0]->HeaderSize = sizeof(EFI_CAPSULE_HEADER);
	pcapsuleheaderarray[0]->Flags = flag;
	querycapsulecapabilities.status = &status;
	querycapsulecapabilities.CapsuleHeaderArray = pcapsuleheaderarray;
	capsulecount = 1;
	querycapsulecapabilities.CapsuleCount = capsulecount;
	querycapsulecapabilities.MaximumCapsuleSize = &maxcapsulesize;
	querycapsulecapabilities.ResetType = &resettype;

	for (i = 0; i < multitesttime; i++) {
		status = ~0ULL;
		long ioret = ioctl(fd, EFI_RUNTIME_QUERY_CAPSULECAPABILITIES, &querycapsulecapabilities);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED) {
				fwts_skipped(fw, "Not support the UEFI QueryCapsuleCapabilities runtime interface"
						 " with flag value 0x%" PRIx32 ": cannot test.", flag);
				fwts_advice(fw, "Firmware also needs to check if the revision of system table is correct or not."
						" Linux kernel returns EFI_UNSUPPORTED as well, if the FirmwareRevision"
						" of system table is less than EFI_2_00_SYSTEM_TABLE_REVISION.");
				return FWTS_SKIP;
			} else {
				fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeQueryCapsuleCapabilities",
					"Failed to query capsule capabilities with UEFI runtime service"
					" with flag value 0x%" PRIx32 ".", flag);
				fwts_uefi_print_status_info(fw, status);
				return FWTS_ERROR;
			}
		}
	}

	return FWTS_OK;
}

static int uefirtmisc_test1(fwts_framework *fw)
{
	int ret;
	uint32_t multitesttime = 1;
	uint32_t i;

	static const uint32_t flag[] = {
		0,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET | CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET | CAPSULE_FLAGS_INITIATE_RESET,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET | CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE | CAPSULE_FLAGS_INITIATE_RESET
	};

	fwts_log_info(fw, "Testing UEFI runtime service GetNextHighMonotonicCount interface.");
	ret = getnexthighmonotoniccount_test(fw, multitesttime);
	if (ret == FWTS_OK)
		fwts_passed(fw, "UEFI runtime service GetNextHighMonotonicCount"
			" interface test passed.");

	fwts_log_info(fw, "Testing UEFI runtime service QueryCapsuleCapabilities interface.");
	for (i = 0; i < (sizeof(flag)/(sizeof flag[0])); i++) {
		ret = querycapsulecapabilities_test(fw, multitesttime, flag[i]);
		if (ret == FWTS_SKIP)
			continue;
		if (ret != FWTS_OK)
			return ret;

		fwts_passed(fw, "UEFI runtime service QueryCapsuleCapabilities"
			" interface test with flag value 0x%"PRIx32 " passed.",
			flag[i]);
	}

	return ret;
}

static int uefirtmisc_test2(fwts_framework *fw)
{
	int ret;
	uint32_t multitesttime = uefi_get_mn_count_multiple;
	uint32_t i;

	static const uint32_t flag[] = {
		0,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET | CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET | CAPSULE_FLAGS_INITIATE_RESET,
		CAPSULE_FLAGS_PERSIST_ACROSS_RESET | CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE | CAPSULE_FLAGS_INITIATE_RESET
	};

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_HIGH_MONOTONIC_COUNT)) {
		fwts_skipped(fw, "Skipping test, GetNextHighMonotonicCount "
			"runtime service is not supported on this platform.");
		return FWTS_SKIP;
	}

	fwts_log_info(fw, "Stress testing for UEFI runtime service "
		"GetNextHighMonotonicCount interface %" PRIu32 " times.",
		multitesttime);
	ret = getnexthighmonotoniccount_test(fw, multitesttime);
	if (ret == FWTS_OK)
		fwts_passed(fw, "UEFI runtime service GetNextHighMonotonicCount"
			" interface test passed.");

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_QUERY_CAPSULE_CAPABILITIES)) {
		fwts_skipped(fw, "Skipping test, QueryCapsuleCapabilities "
			"runtime service is not supported on this platform.");
		return FWTS_SKIP;
	}

	fwts_log_info(fw, "Stress testing UEFI runtime service QueryCapsuleCapabilities interface.");
	for (i = 0; i < (sizeof(flag)/(sizeof flag[0])); i++) {
		ret = querycapsulecapabilities_test(fw, multitesttime, flag[i]);
		if (ret == FWTS_SKIP)
			continue;
		if (ret != FWTS_OK)
			return ret;

		fwts_passed(fw, "UEFI runtime service QueryCapsuleCapabilities"
			" interface test with flag value 0x%"PRIx32 " passed.",
			flag[i]);
	}

	return ret;
}

static int uefirtmisc_test3(fwts_framework *fw)
{
	uint64_t status = ~0ULL;
	long ioret;
	struct efi_getnexthighmonotoniccount getnexthighmonotoniccount;

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_HIGH_MONOTONIC_COUNT)) {
		fwts_skipped(fw, "Skipping test, GetNextHighMonotonicCount "
			"runtime service is not supported on this platform.");
		return FWTS_SKIP;
	}

	getnexthighmonotoniccount.HighCount = NULL;
	getnexthighmonotoniccount.status = &status;

	ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTHIGHMONOTONICCOUNT, &getnexthighmonotoniccount);
	if (ioret == -1) {
		if (status == EFI_UNSUPPORTED) {
			fwts_skipped(fw, "Skipping test, GetNextHighMonotonicCount runtime "
				"service is not supported on this platform.");
			return FWTS_SKIP;
		}
		if (status == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "Test with invalid NULL parameter returned "
				"EFI_INVALID_PARAMETER as expected.");
			return FWTS_OK;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetNextHighMonotonicCountInvalid",
			"Failed to get correct return status from UEFI runtime service, expecting EFI_INVALID_PARAMETER.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	fwts_failed(fw, LOG_LEVEL_HIGH, "UEFIRuntimeGetNextHighMonotonicCountInvalid",
		"Failed to get error return status from UEFI runtime service, expected EFI_INAVLID_PARAMETER.");
	return FWTS_ERROR;
}

static int uefirtmisc_test4(fwts_framework *fw)
{

	if (!(runtimeservicessupported & EFI_RT_SUPPORTED_GET_NEXT_HIGH_MONOTONIC_COUNT)) {
		long ioret;
		uint64_t status;
		struct efi_getnexthighmonotoniccount getnexthighmonotoniccount;
		uint32_t highcount;

		getnexthighmonotoniccount.HighCount = &highcount;
		getnexthighmonotoniccount.status = &status;
		status = ~0ULL;

		ioret = ioctl(fd, EFI_RUNTIME_GET_NEXTHIGHMONOTONICCOUNT, &getnexthighmonotoniccount);
		if (ioret == -1) {
			if (status == EFI_UNSUPPORTED)
				fwts_passed(fw, "UEFI GetNextHighMonotonicCount runtime "
					"service unsupported status test passed.");
			else {
				if (status == ~0ULL) {
					fwts_skipped(fw, "Unknown error occurred, skip test.");
					return FWTS_SKIP;
				} else {
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"UEFIRuntimeGetNextHighMonotonicCount",
						"Get the GetNextHighMonotonicCount runtime "
						"service unsupported via RuntimeServicesSupported "
						"mask. But actually is supported by firmware.");
					return FWTS_ERROR;
				}
			}
		} else {
			if (status != EFI_SUCCESS ) {
				fwts_skipped(fw, "Unknown error occurred, skip test.");
				return FWTS_SKIP;
			} else {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"UEFIRuntimeGetNextHighMonotonicCount",
					"Get the GetNextHighMonotonicCount runtime "
					"service unsupported via RuntimeServicesSupported "
					"mask. But actually is supported by firmware.");
				return FWTS_ERROR;
			}
		}
	} else {
		fwts_skipped(fw, "GetNextHighMonotonicCount runtime service supported, skip test.");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

static int options_check(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if ((uefi_get_mn_count_multiple < 1) ||
	    (uefi_get_mn_count_multiple >
	    UEFI_GET_NEXT_HIGH_MN_COUNT_MULTIPLE_MAX)) {
		fprintf(stderr, "uefi_get_mn_count_multiple is %"
			PRIu32 ", it should be 1..%" PRIu32 "\n",
			uefi_get_mn_count_multiple,
			UEFI_GET_NEXT_HIGH_MN_COUNT_MULTIPLE_MAX);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int options_handler(
	fwts_framework *fw,
	int argc,
	char * const argv[],
	int option_char,
	int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);

	if (option_char == 0) {
		switch (long_index) {
		case 0:	/* --uefi_get_mn_count_multiple */
			uefi_get_mn_count_multiple = strtoul(optarg, NULL, 10);
			break;
		}
	}
	return FWTS_OK;
}

static fwts_option options[] = {
	{ "uefi-get-mn-count-multiple", "", 1, "Run uefirtmisc getnexthighmonotoniccount test multiple times." },
	{ NULL, NULL, 0, NULL }
};
static fwts_framework_minor_test uefirtmisc_tests[] = {
	{ uefirtmisc_test1, "Test for UEFI miscellaneous runtime service interfaces." },
	{ uefirtmisc_test2, "Stress test for UEFI miscellaneous runtime service interfaces." },
	{ uefirtmisc_test3, "Test GetNextHighMonotonicCount with invalid NULL parameter." },
	{ uefirtmisc_test4, "Test UEFI miscellaneous runtime services unsupported status." },
	{ NULL, NULL }
};

static fwts_framework_ops uefirtmisc_ops = {
	.description = "UEFI miscellaneous runtime service interface tests.",
	.init        = uefirtmisc_init,
	.deinit      = uefirtmisc_deinit,
	.minor_tests = uefirtmisc_tests,
	.options         = options,
	.options_handler = options_handler,
	.options_check   = options_check
};

FWTS_REGISTER("uefirtmisc", &uefirtmisc_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_XBBR)

#endif
