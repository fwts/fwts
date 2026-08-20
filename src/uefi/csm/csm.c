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
#ifdef FWTS_ARCH_INTEL

#include <stdbool.h>
#include <inttypes.h>

/* Real Mode IDT */
#define INT_VEC_START		(0x00000000)
#define INT_VEC_END		(0x000003ff)
#define INT_VEC_SIZE		(INT_VEC_END - INT_VEC_START)

/* Legacy BIOS Option ROM region */
#define BIOS_ROM_REGION_START	(0x000c0000)
#define BIOS_ROM_REGION_END	(0x000fffff)
#define BIOS_ROM_REGION_SIZE	(BIOS_ROM_REGION_END - BIOS_ROM_REGION_START)

#define EFI_SUPPORT		(0x0001)
#define VGA_SUPPORT		(0x0002)

static int csm_test1(fwts_framework *fw)
{
	uint8_t  *optROM;
	uint32_t *intVec;
	uint32_t int10hVec;
	uint32_t i;
	int flag = 0;

	fwts_log_info(fw, "Checking for UEFI Compatibility Support Module (CSM)");

	if (fw->firmware_type == FWTS_FIRMWARE_UEFI)
		flag |= EFI_SUPPORT;

	/* Get Int 10h vector from segment/offset realmode address */
	if ((intVec = fwts_mmap(INT_VEC_START, INT_VEC_SIZE)) == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap interrupt vector region.");
		return FWTS_ERROR;
	}
	if (fwts_safe_memread(intVec, INT_VEC_SIZE) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read interrupt vector region.");
		fwts_munmap(intVec, INT_VEC_SIZE);
		return FWTS_ERROR;
	}
	int10hVec = (intVec[0x10] & 0xffff) | ((intVec[0x10] & 0xffff0000)>> 12);
	fwts_munmap(intVec, INT_VEC_SIZE);

	if ((optROM = fwts_mmap(BIOS_ROM_REGION_START, BIOS_ROM_REGION_SIZE)) == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap BIOS ROM region.");
		return FWTS_ERROR;
	}

	for (i = 0; i < BIOS_ROM_REGION_SIZE; i += 512) {
		/* Skip any option ROM headers that we can't read */
		if (fwts_safe_memread(optROM + i, 6) != FWTS_OK)
			continue;
		if ((*(optROM + i) == 0x55) && (*(optROM + i + 1) == 0xaa)) {
			uint32_t length = *(optROM + i + 2) << 9;
			uint32_t ROMstart = BIOS_ROM_REGION_START + i;
			uint32_t ROMend = BIOS_ROM_REGION_START + i + length;

			if ((ROMstart <= int10hVec) && (int10hVec <= ROMend)) {
				fwts_log_info(fw, "Int 10h jumps to 0x%" PRIx32 " in option ROM at: "
					"0x%x..0x%0x",
					int10hVec, ROMstart, ROMend);
				flag |= VGA_SUPPORT;
				break;
			}
		}
	}
	(void)fwts_munmap(optROM, BIOS_ROM_REGION_SIZE);

	switch (flag) {
	case 0:
		/* Unlikely */
		fwts_log_info(fw, "No CSM: Legacy BIOS firmware does not have a video option ROM.");
		break;
	case VGA_SUPPORT:
		fwts_log_info(fw, "No CSM: Legacy BIOS firmware has video option ROM with Int 10h support.");
		break;
	case EFI_SUPPORT:
		fwts_log_info(fw, "No CSM: UEFI firmware seems to have no CSM support.");
		break;
	case (EFI_SUPPORT | VGA_SUPPORT):
		fwts_log_info(fw, "CSM: UEFI firmware seems to have CSM support with Int 10h support.");
		break;
	default:
		/* Impossible */
		break;
	}

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test csm_tests[] = {
	{ csm_test1, "UEFI Compatibility Support Module test." },
	{ NULL, NULL }
};

static fwts_framework_ops csm_ops = {
	.description = "UEFI Compatibility Support Module test.",
	.minor_tests = csm_tests
};

FWTS_REGISTER("csm", &csm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
