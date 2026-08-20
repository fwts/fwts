/*
 * Copyright (C) 2006, Intel Corp
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
 * This test checks if the SVM-setup is done correctly by the BIOS
 */

#define _GNU_SOURCE

#include "fwts.h"
#include "virt.h"

#ifdef FWTS_ARCH_INTEL

#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define MSR_VM_CR		0xC0010114

extern fwts_cpuinfo_x86 *fwts_virt_cpuinfo;

static int cpu_has_svm(void)
{
	return (strstr(fwts_virt_cpuinfo->flags, "svm") != NULL);
}

static int can_lock_with_msr(void)
{
	return (fwts_virt_cpuinfo->x86 & 0x10);
}

static int vt_locked_by_bios(fwts_framework *fw)
{
	uint64_t msr;

	if (!can_lock_with_msr())
		return 0;

	if (fwts_cpu_readmsr(fw, 0, MSR_VM_CR, &msr))
		return -1;

	return ((msr & 0x10) == 0x10); /* SVM locked */
}

void virt_check_svm(fwts_framework *fw)
{
	fwts_log_info(fw, "Check SVM Virtualization extensions are set up correctly.");

	if (!cpu_has_svm())
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
