/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2007, AMD Inc
 * Copyright (C) 2010-2026 Canonical
 *
 * This file was originally part of the Linux-ready Firmware Developer Kit
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation;version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */


/*
 * This test checks if the VT-setup is done correctly by the BIOS
 */

#define _GNU_SOURCE

#include "fwts.h"
#include "virt.h"

#ifdef FWTS_ARCH_INTEL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <errno.h>

#define MSR_FEATURE_CONTROL	0x03a

extern fwts_cpuinfo_x86 *fwts_virt_cpuinfo;

static int cpu_has_vmx(void)
{
	return (strstr(fwts_virt_cpuinfo->flags, "vmx") != NULL);
}

static int vt_locked_by_bios(fwts_framework *fw)
{
	uint64_t msr;

	if (fwts_cpu_readmsr(fw, 0, MSR_FEATURE_CONTROL, &msr))
		return -1;

	return (msr & 5) == 1; /* VT capable but locked by bios*/
}

void virt_check_vmx(fwts_framework *fw)
{
	fwts_log_info(fw, "Check VT/VMX Virtualization extensions are set up correctly.");

	if (!cpu_has_vmx())
		fwts_skipped(fw, "Processor does not support Virtualization extensions, won't test BIOS configuration, skipping test.");
	else  {
		int ret = vt_locked_by_bios(fw);
		switch (ret) {
		case 0:
			fwts_passed(fw, "Virtualization extensions supported and enabled by BIOS.");
			break;
		case 1:
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"VirtDisabledByBIOS",
				"Virtualization extensions supported but disabled by BIOS.");
			break;
		default:
			fwts_log_error(fw, "Virtualization extensions check failed - cannot read msr.");
			break;
		}
	}
}

#endif
