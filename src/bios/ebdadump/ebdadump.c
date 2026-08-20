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
#include <stdint.h>
#include <inttypes.h>

#ifdef FWTS_ARCH_INTEL

#define BIOS_ROM_START		(0x000a0000)

static void ebdadump_data(fwts_framework *fw,
	uint8_t *data, int offset, int length)
{
	char buffer[128];
	int i;

	for (i = 0; i < length; i += 16) {
		fwts_dump_raw_data(buffer, sizeof buffer, data+i, offset+i, 16);
		fwts_log_info_verbatim(fw, "%s", buffer);
	}
}

static int ebdadump_test1(fwts_framework *fw)
{
	off_t  ebda_addr;
	uint8_t *mem;
	size_t len;

	if ((ebda_addr = fwts_ebda_get()) == FWTS_NO_EBDA) {
		fwts_log_error(fw, "Failed to local EBDA region.");
		return FWTS_ERROR;
	}

	len = BIOS_ROM_START - ebda_addr;

	if (ebda_addr > BIOS_ROM_START) {
		fwts_log_error(fw, "EBDA start address is greater than the "
			"BIOS ROM start address.");
		return FWTS_ERROR;
	}

        if ((mem = fwts_mmap(ebda_addr, len)) == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap BIOS ROM region.");
		return FWTS_ERROR;
	}

	fwts_log_info(fw, "EBDA region: %" PRIx32 "..%x (%zd bytes)",
		(uint32_t)ebda_addr,
		BIOS_ROM_START,
		len);

	if (fwts_safe_memread((void *)mem, len) != FWTS_OK)
		fwts_log_info(fw, "EBDA region at cannot be read");
	else
		ebdadump_data(fw, mem, ebda_addr, len);
        (void)fwts_munmap(mem, len);

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test ebdadump_tests[] = {
	{ ebdadump_test1, "Dump EBDA region." },
	{ NULL, NULL }
};

static fwts_framework_ops ebdadump_ops = {
	.description = "Dump EBDA region.",
	.minor_tests = ebdadump_tests
};

FWTS_REGISTER("ebdadump", &ebdadump_ops, FWTS_TEST_ANYTIME,
	FWTS_FLAG_UTILS | FWTS_FLAG_ROOT_PRIV)

#endif
