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
#include <stdio.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

/*
 * Some CMOS information from
 * http://www-ivs.cs.uni-magdeburg.de/~zbrog/asm/cmos.html
 */

static int cmosdump_test1(fwts_framework *fw)
{
	int i;
	unsigned long tmp;

	static const char *const cmos_shutdown_status[] = {
		"Power on or soft reset",
		"Memory size pass",
		"Memory test pass",
		"Memory test fail",

		"INT 19h reboot",
		"Flush keyboard and jmp via 40h:67h",
		"Protected mode tests pass",
		"Protected mode tests fail",

		"Used by POST during protected-mode RAM test",
		"Int 15h (block move)",
		"Jmp via 40h:67h",
		"Used by 80386",
	};

	static const char *const floppy_disk[] = {
		"None",
		"360KB 5.25\" Drive",
		"1.2MB 5.25\" Drive",
		"720KB 3.5\" Drive",
		"1.44MB 3.5\" Drive",
		"2.88MB 3.5\" Drive",
		"Unknown",
		"Unknown"
	};

	static const char *const hard_disk[] = {
		"None",
		"Type 1",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Unknown",
		"Type 14",
		"Type 16-47"
	};

	static const char *const primary_display[] = {
		"BIOS selected",
		"CGA 40 column",
		"CGA 80 column",
		"Monochrome"
	};

	static const char *const divider[8] = {
		"4.194 MHz",
		"1.049 MHz",
		"32.768 KHz (default)",
		"unknown",
		"test mode",
		"test mode",
		"reset / disable",
		"reset / disable",
	};

	static const char *const rate_selection[16] = {
		"none",
		"3.90625 milliseconds",
		"7.8215 milliseconds",
		"122.070 microseconds",
		"244.141 microseconds",
		"488.281 microseconds",
		"976.562 microseconds (default)",
		"1.953125 milliseconds",
		"3.90625 milliseconds",
		"7.8215 milliseconds",
		"15.625 milliseconds",
		"31.25 milliseconds",
		"62.5 milliseconds",
		"125 milliseconds",
		"250 milliseconds",
		"500 milliseconds"
	};

	unsigned char data[0x80];

	/* Read CMOS Data */
	for (i = 0; i < (int)sizeof(data); i++) {
		if (fwts_cmos_read(i, &data[i]) != FWTS_OK) {
			fwts_log_error(fw, "Cannot get read/write permission on I/O ports.");
			return FWTS_ERROR;
		}
	}

	fwts_log_info_verbatim(fw, "CMOS Memory Dump:");
	for (i = 0; i < (int)sizeof(data); i += 16) {
		char buffer[128];
		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, 16);
		fwts_log_info_verbatim(fw, "%s", buffer);
	}
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "RTC Current Time: (CMOS 0x00..0x09)");
	fwts_log_info_verbatim(fw, "  RTC seconds:            %2.2x", data[0]);
	fwts_log_info_verbatim(fw, "  RTC minutes:            %2.2x", data[2]);
	fwts_log_info_verbatim(fw, "  RTC hours:              %2.2x", data[4]);
	fwts_log_info_verbatim(fw, "  RTC day of week:        %2.2x", data[6]);
	fwts_log_info_verbatim(fw, "  RTC date day:           %2.2x", data[7]);
	fwts_log_info_verbatim(fw, "  RTC date month:         %2.2x", data[8]);
	fwts_log_info_verbatim(fw, "  RTC date year:          %2.2x", data[9]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "RTC Alarm:");
	fwts_log_info_verbatim(fw, "  RTC seconds:            %2.2x", data[1]);
	fwts_log_info_verbatim(fw, "  RTC minutes:            %2.2x", data[3]);
	fwts_log_info_verbatim(fw, "  RTC hours:              %2.2x", data[5]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Status Register A: (CMOS 0x0a): 0x%2.2x",
		data[10]);
	fwts_log_info_verbatim(fw, "  Rate freq:              %1.1x (%s)",
		data[10] & 0xf, rate_selection[data[10] & 0xf]);
	fwts_log_info_verbatim(fw, "  Timer freq divider:     %1.1x (%s)",
		(data[10] >> 4) & 0x7, divider[(data[10] >> 4) & 0x7]);
	fwts_log_info_verbatim(fw, "  Update in progress:     %1.1x",
		(data[10] >> 7) & 1);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Status Register B: (CMOS 0x0b): 0x%2.2x",
		data[11]);
	fwts_log_info_verbatim(fw, "  Daylight savings:       %1.1x (%s)",
		data[11] & 1,
		(data[11] & 1) ? "Enabled" : "Disabled");
	fwts_log_info_verbatim(fw, "  24 Hour Clock:          %1.1x (%s)",
		(data[11] >> 1) & 1,
		((data[11] >> 1) & 1) ? "24 Hour" : "12 Hour");
	fwts_log_info_verbatim(fw, "  Data Mode (DM):         %1.1x (%s)",
		(data[11] >> 2) & 1,
		((data[11] >> 2) & 1) ? "Binary" : "BCD");
	fwts_log_info_verbatim(fw, "  Square Wave:            %1.1x (%s)",
		(data[11] >> 3) & 1,
		((data[11] >> 3) & 1) ? "Enabled" : "Disabled");
	fwts_log_info_verbatim(fw, "  Update ended IRQ:       %1.1x (%s)",
		(data[11] >> 4) & 1,
		((data[11] >> 4) & 1) ? "Enabled" : "Disabled");
	fwts_log_info_verbatim(fw, "  Alarm IRQ:              %1.1x (%s)",
		(data[11] >> 5) & 1,
		((data[11] >> 5) & 1) ? "Enabled" : "Disabled");
	fwts_log_info_verbatim(fw, "  Periodic IRQ:           %1.1x (%s)",
		(data[11] >> 6) & 1,
		((data[11] >> 6) & 1) ? "Enabled" : "Disabled");
	fwts_log_info_verbatim(fw, "  Clock update cycle:     %1.1x (%s)",
		(data[11] >> 7) & 1,
		((data[11] >> 7) & 1) ? "Abort update in progress" : "Update normally");
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Status Register C: (CMOS 0x0c): 0x%2.2x",
		data[12]);
	fwts_log_info_verbatim(fw, "  UF flag:                0x%1.1x",
		(data[12] >> 4) & 1);
	fwts_log_info_verbatim(fw, "  AF flag:                0x%1.1x",
		(data[12] >> 5) & 1);
	fwts_log_info_verbatim(fw, "  PF flag:                0x%1.1x",
		(data[12] >> 6) & 1);
	fwts_log_info_verbatim(fw, "  IRQF flag:              0x%1.1x",
		(data[12] >> 7) & 1);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Status Register D: (CMOS 0x0d): 0x%2.2x",
		data[13]);
	fwts_log_info_verbatim(fw, "  Valid CMOS RAM flag:    0x%1.1x (%s)",
		(data[13] >> 7) & 1,
		((data[13] >> 7) & 1) ? "Battery Good": "Battery Dead");
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Diagnostic Status: (CMOS 0x0e): 0x%2.2x",
		data[14]);
	fwts_log_info_verbatim(fw, "  CMOS time status:       0x%1.1x (%s)",
		(data[14] >> 2) & 1,
		((data[14] >> 2) & 1) ? "Invalid": "Valid");
	fwts_log_info_verbatim(fw, "  Fixed disk init:        0x%1.1x (%s)",
		(data[14] >> 3) & 1,
		((data[14] >> 3) & 1) ? "Bad": "Good");
	fwts_log_info_verbatim(fw, "  Memory size check:      0x%1.1x (%s)",
		(data[14] >> 4) & 1,
		((data[14] >> 4) & 1) ? "Bad": "Good");
	fwts_log_info_verbatim(fw, "  Config info status:     0x%1.1x (%s)",
		(data[14] >> 5) & 1,
		((data[14] >> 5) & 1) ? "Invalid": "Valid");
	fwts_log_info_verbatim(fw, "  CMOS checksum status:   0x%1.1x (%s)",
		(data[14] >> 6) & 1,
		((data[14] >> 6) & 1) ? "Bad": "Good");
	fwts_log_info_verbatim(fw, "  CMOS power loss:        0x%1.1x (%s)",
		(data[14] >> 7) & 1,
		((data[14] >> 7) & 1) ? "Lost power": "Not lost power");
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "CMOS Shutdown Status: (CMOS 0x0f): 0x%2.2x (%s)",
		data[15],
		data[15] < 0xb ? cmos_shutdown_status[data[15]] : "Perform power-on reset");
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Floppy Disk Type: (CMOS 0x10): 0x%2.2x",
		data[16]);
	fwts_log_info_verbatim(fw, "  Drive 0: %s",
		floppy_disk[((data[16] >> 4) & 0xf)]);
	fwts_log_info_verbatim(fw, "  Drive 1: %s",
		floppy_disk[((data[16] >> 0) & 0xf)]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Hard Disk Type: (CMOS 0x12, Obsolete): 0x%2.2x", data[18]);
	fwts_log_info_verbatim(fw, "  Drive 0: %s",
		hard_disk[((data[18] >> 4) & 0xf)]);
	fwts_log_info_verbatim(fw, "  Drive 1: %s",
		hard_disk[((data[18] >> 0) & 0xf)]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Installed H/W: (CMOS 0x14): 0x%2.2x",
		data[20]);
	fwts_log_info_verbatim(fw, "  Maths Coprocessor:      0x%1.1x (%s)",
		(data[20] >> 1) & 1,
		((data[20] >> 1) & 1) ? "Installed": "Not Installed");
	fwts_log_info_verbatim(fw, "  Keyboard:               0x%1.1x (%s)",
		(data[20] >> 2) & 1,
		((data[20] >> 2) & 1) ? "Installed": "Not Installed");
	fwts_log_info_verbatim(fw, "  Display Adaptor:        0x%1.1x (%s)",
		(data[20] >> 3) & 1,
		((data[20] >> 3) & 1) ? "Installed": "Not Installed");
	fwts_log_info_verbatim(fw, "  Primary Display:        0x%1.1x (%s)",
		(data[20] >> 4) & 3,
		primary_display[(data[20] >> 4) & 3]);
	if (data[20] & 1) {
		int drives = (data[20] >> 6) & 3;
		fwts_log_info_verbatim(fw, "  Floppy Drives:          0x%2.2x (%d drive%s)",
			drives, drives + 1, drives > 0 ? "s" : "");
	} else {
		fwts_log_info_verbatim(fw, "  Floppy Drives:          None.");
	}
	fwts_log_nl(fw);

	tmp = ((data[22] << 8) | (data[21]));
	fwts_log_info_verbatim(fw, "Base Mem: (CMOS 0x16):");
	fwts_log_info_verbatim(fw, "  0x%2.2x%2.2x (%luK)",
		data[22], data[21], tmp);
	fwts_log_nl(fw);

	tmp = ((data[24] << 8) | (data[25]));
	fwts_log_info_verbatim(fw, "Extended Mem: (CMOS 0x18):");
	fwts_log_info_verbatim(fw, "  0x%2.2x%2.2x (%luK) %s",
		data[24], data[23], tmp,
		tmp > (16 * 1024) ? "[untrustworthy]" : "");
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Hard Disk Extended Types (CMOS 0x19, 0x1a):");
	fwts_log_info_verbatim(fw, "  Hard Disk 0:            0x%2.2x",
		data[25]);
	fwts_log_info_verbatim(fw, "  Hard Disk 1:            0x%2.2x",
		data[26]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "CMOS Checksum:(CMOS 0x2e):0x%2.2x%2.2x",
		data[47], data[46]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Extended Mem: (CMOS 0x30):0x%2.2x%2.2x",
		data[49], data[48]);
	fwts_log_nl(fw);

	fwts_log_info_verbatim(fw, "Century Date: (CMOS 0x32):%2.2x", data[50]);
	fwts_log_nl(fw);
	fwts_log_info_verbatim(fw, "POST Information Flag (CMOS 0x33):");
	fwts_log_info_verbatim(fw, "  POST cache test:        0x%1.1x %s",
		(data[51] >> 0) & 1, ((data[51] >> 0) & 1) ? "Failed" : "Passed");
	fwts_log_info_verbatim(fw, "  BIOS size:              0x%1.1x %s",
		(data[51] >> 7) & 1, ((data[51] >> 7) & 1) ? "128KB" : "64KB");
	fwts_log_nl(fw);

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test cmosdump_tests[] = {
	{ cmosdump_test1, "Dump CMOS Memory." },
	{ NULL, NULL }
};

static fwts_framework_ops cmosdump_ops = {
	.description = "Dump CMOS Memory.",
	.minor_tests = cmosdump_tests
};

FWTS_REGISTER("cmosdump", &cmosdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS | FWTS_FLAG_ROOT_PRIV)

#endif
