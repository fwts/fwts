/*
 * Copyright (C) 2010-2025 Canonical
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fwts.h"

#if defined(FWTS_ARCH_INTEL) || defined(FWTS_ARCH_AARCH64)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

#define DMI_VERSION			(0x0380)
#define VERSION_MAJOR(v)		((v) >> 8)
#define VERSION_MINOR(v)		((v) & 0xff)

#define SMBIOS_END_OF_TABLE		(127)

#define DMI_NO_TABLE			"DMINoTable"
#define DMI_NO_TABLE_HEADER		"DMINoTableHeader"
#define DMI_BAD_TABLE_LENGTH		"DMIBadTableLength"
#define DMI_BAD_UUID			"DMIBadUUID"
#define DMI_STRUCT_COUNT		"DMIStructCount"
#define DMI_VALUE_OUT_OF_RANGE		"DMIValueOutOfRange"
#define DMI_STRING_INDEX_OUT_OF_RANGE	"DMIStringIndexOutOfRange"
#define DMI_ILLEGAL_MAPPED_ADDR_RANGE	"DMIIllegalMappedAddrRange"
#define DMI_MGMT_CTRL_HOST_TYPE		"DMIMgmtCtrlHostType"
#define DMI_INVALID_ENTRY_LENGTH	"DMIInvalidEntryLength"
#define DMI_INVALID_HARDWARE_ENTRY	"DMIInvalidHardwareEntry"
#define DMI_INVALID_MEMORY_ENTRY	"DMIInvalidMemoryEntry"
#define DMI_RESERVED_VALUE_USED		"DMIReservedValueUsed"
#define DMI_RESERVED_BIT_USED		"DMIReservedBitUsed"
#define DMI_RESERVED_OFFSET_NONZERO	"DMIReservedOffsetNonZero"
#define DMI_BAD_STRING			"DMIBadString"

#define GET_UINT8(x) (uint8_t)(*(const uint8_t *)(x))
#define GET_UINT16(x) (uint16_t)(*(const uint16_t *)(x))
#define GET_UINT32(x) (uint32_t)(*(const uint32_t *)(x))
#define GET_UINT64(x) (uint64_t)(*(const uint64_t *)(x))

#define CHASSIS_OTHER			0x00
#define CHASSIS_DESKTOP			0x01
#define CHASSIS_WORKSTATION		0x02
#define CHASSIS_MOBILE			0x04
#define CHASSIS_SERVER			0x08

#define dmi_ranges_uint8_check(fw, table, addr, field, hdr, offset, ranges) \
	 _dmi_ranges_uint8_check(fw, table, addr, field, hdr, offset, ranges, sizeof(ranges))

typedef struct {
	const char *label;
	const char *field;
	const char *value;
} fwts_dmi_pattern;

typedef struct {
	uint16_t   old;
	uint16_t   new;
} fwts_dmi_version;

typedef struct {
	uint8_t	type;
	uint8_t	offset;
} fwts_dmi_used_by_kernel;

typedef struct {
	uint8_t type;
	uint16_t max_version;
	uint16_t min_version;
	uint8_t length;
} fwts_dmi_type_length;

typedef struct {
	uint16_t min;
	uint16_t max;
} fwts_dmi_value_range;

static bool smbios_found = false;
static bool smbios30_found = false;

/*
 *  Table derived by scanning thousands of DMI table dumps from bug reports
 */
static const fwts_dmi_pattern dmi_patterns[] = {
	{ "DMISerialNumber",	"Serial Number",	"0000000" },
	{ "DMISerialNumber",	"Serial Number",	"00000000" },
	{ "DMISerialNumber",	"Serial Number",	"000000000" },
	{ "DMISerialNumber",	"Serial Number",	"0000000000" },
	{ "DMISerialNumber",	"Serial Number",	"0x00000000" },
	{ "DMISerialNumber",	"Serial Number",	"0x0000000000000000" },
	{ "DMISerialNumber",	"Serial Number",	"012345678" },
	{ "DMISerialNumber",	"Serial Number",	"0123456789" },
	{ "DMISerialNumber",	"Serial Number",	"01234567890" },
	{ "DMISerialNumber",	"Serial Number",	"012345678900" },
	{ "DMISerialNumber",	"Serial Number",	"0123456789000" },
	{ "DMISerialNumber",	"Serial Number",	"1234567" },
	{ "DMISerialNumber",	"Serial Number",	"12345678" },
	{ "DMISerialNumber",	"Serial Number",	"123456789" },
	{ "DMISerialNumber",	"Serial Number",	"1234567890" },
	{ "DMISerialNumber",	"Serial Number",	"System Serial Number" },
	{ "DMISerialNumber",	"Serial Number",	"MB-1234567890" },
	{ "DMISerialNumber",	"Serial Number",	"NB-1234567890" },
	{ "DMISerialNumber",	"Serial Number",	"NB-0123456789" },
	{ "DMISerialNumber",	"Serial Number",	"Base Board Serial Number" },
	{ "DMISerialNumber",	"Serial Number",	"Chassis Serial Number" },
	{ "DMISerialNumber",	"Serial Number",	"<cut out>" },
	{ "DMISerialNumber",	"Serial Number",	"Empty" },
	{ "DMISerialNumber",	"Serial Number",	"[Empty]" },
	{ "DMISerialNumber",	"Serial Number",	"NA" },
	{ "DMISerialNumber",	"Serial Number",	"N/A" },
	{ "DMISerialNumber",	"Serial Number",	"None" },
	{ "DMISerialNumber",	"Serial Number",	"None1" },
	{ "DMISerialNumber",	"Serial Number",	"Not Available" },
	{ "DMISerialNumber",	"Serial Number",	"Not Specified" },
	{ "DMISerialNumber",	"Serial Number",	"NotSupport" },
	{ "DMISerialNumber",	"Serial Number",	"Not Supported by CPU" },
	{ "DMISerialNumber",	"Serial Number",	"Not Supported" },
	{ "DMISerialNumber",	"Serial Number",	"OEM Chassis Serial Number" },
	{ "DMISerialNumber",	"Serial Number",	"OEM_Define1" },
	{ "DMISerialNumber",	"Serial Number",	"OEM_Define2" },
	{ "DMISerialNumber",	"Serial Number",	"OEM_Define3" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum0" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum00" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum01" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum02" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum03" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum1" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum2" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum3" },
	{ "DMISerialNumber",	"Serial Number",	"SerNum4" },
	{ "DMISerialNumber",	"Serial Number",	"TBD by ODM" },
	{ "DMISerialNumber",	"Serial Number",	"To Be Defined By O.E.M" },
	{ "DMISerialNumber",	"Serial Number",	"To be filled by O.E.M." },
	{ "DMISerialNumber",	"Serial Number",	"To Be Filled By O.E.M." },
	{ "DMISerialNumber",	"Serial Number",	"Unknow" },
	{ "DMISerialNumber",	"Serial Number",	"Unknown" },
	{ "DMISerialNumber",	"Serial Number",	"Unspecified System Serial Number" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXXXXXXXXX" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXXXXXXXX" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXXXXXXX" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXXXXXX" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXXXXX" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXXX" },
	{ "DMISerialNumber",	"Serial Number",	"XXXXX" },
	{ "DMISerialNumber",	NULL,			"Chassis Serial Number" },
	{ "DMIAssetTag",	"Asset Tag",		"0000000000" },
	{ "DMIAssetTag",	"Asset Tag",		"0x00000000" },
	{ "DMIAssetTag",	"Asset Tag",		"1234567890" },
	{ "DMIAssetTag",	"Asset Tag",		"123456789000" },
	{ "DMIAssetTag",	"Asset Tag",		"9876543210" },
	{ "DMIAssetTag",	"Asset Tag",		"A1_AssetTagNum0" },
	{ "DMIAssetTag",	"Asset Tag",		"A1_AssetTagNum1" },
	{ "DMIAssetTag",	"Asset Tag",		"A1_AssetTagNum2" },
	{ "DMIAssetTag",	"Asset Tag",		"A1_AssetTagNum3" },
	{ "DMIAssetTag",	"Asset Tag",		"ABCDEFGHIJKLM" },
	{ "DMIAssetTag",	"Asset Tag",		"Asset-1234567890" },
	{ "DMIAssetTag",	"Asset Tag",		"Asset Tag" },
	{ "DMIAssetTag",	"Asset Tag",		"Asset Tag:" },
	{ "DMIAssetTag",	"Asset Tag",		"AssetTagNum0" },
	{ "DMIAssetTag",	"Asset Tag",		"AssetTagNum1" },
	{ "DMIAssetTag",	"Asset Tag",		"AssetTagNum2" },
	{ "DMIAssetTag",	"Asset Tag",		"AssetTagNum3" },
	{ "DMIAssetTag",	"Asset Tag",		"AssetTagNum4" },
	{ "DMIAssetTag",	"Asset Tag",		"Asset tracking" },
	{ "DMIAssetTag",	"Asset Tag",		"ATN12345678901234567" },
	{ "DMIAssetTag",	"Asset Tag",		"Base Board Asset Tag#" },
	{ "DMIAssetTag",	"Asset Tag",		"Base Board Asset Tag" },
	{ "DMIAssetTag",	"Asset Tag",		"Chassis Asset Tag" },
	{ "DMIAssetTag",	"Asset Tag",		"<cut out>" },
	{ "DMIAssetTag",	"Asset Tag",		"Fill By OEM" },
	{ "DMIAssetTag",	"Asset Tag",		"N/A" },
	{ "DMIAssetTag",	"Asset Tag",		"No Asset Information" },
	{ "DMIAssetTag",	"Asset Tag",		"No Asset Tag" },
	{ "DMIAssetTag",	"Asset Tag",		"None" },
	{ "DMIAssetTag",	"Asset Tag",		"Not Available" },
	{ "DMIAssetTag",	"Asset Tag",		"Not Specified" },
	{ "DMIAssetTag",	"Asset Tag",		"OEM_Define0" },
	{ "DMIAssetTag",	"Asset Tag",		"OEM_Define1" },
	{ "DMIAssetTag",	"Asset Tag",		"OEM_Define2" },
	{ "DMIAssetTag",	"Asset Tag",		"OEM_Define3" },
	{ "DMIAssetTag",	"Asset Tag",		"OEM_Define4" },
	{ "DMIAssetTag",	"Asset Tag",		"TBD by ODM" },
	{ "DMIAssetTag",	"Asset Tag",		"To Be Defined By O.E.M" },
	{ "DMIAssetTag",	"Asset Tag",		"To be filled by O.E.M." },
	{ "DMIAssetTag",	"Asset Tag",		"To Be Filled By O.E.M." },
	{ "DMIAssetTag",	"Asset Tag",		"Unknown" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXXXXXXXX" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXXXXXXX" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXXXXXX" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXXXXX" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXXXX" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXXX" },
	{ "DMIAssetTag",	"Asset Tag",		"XXXXXX" },
	{ "DMIChassisVendor",	NULL,			"Chassis Manufacture" },
	{ "DMIChassisVersion",	NULL,			"Chassis Version" },
	{ "DMIProductVersion",	NULL,			"System Version" },
	{ "DMIBadDefault",	NULL,			"To Be Defined By O.E.M" },
	{ "DMIBadDefault",	NULL,			"To be filled by O.E.M." },
	{ "DMIBadDefault",	NULL,			"To Be Filled By O.E.M." },
	{ NULL,			NULL,			NULL }
};

static const char *uuid_patterns[] = {
	"0A0A0A0A-0A0A-0A0A-0A0A-0A0A0A0A0A0A",
	NULL,
};

/* Remapping table from buggy version numbers to correct values */
static const fwts_dmi_version dmi_versions[] = {
	{ 0x021f, 0x0203 },
	{ 0x0221, 0x0203 },
	{ 0x0233, 0x0206 },
	{ 0, 0 }
};

#define FIELD_ANY	0xff
#define TYPE_EOD	0xff

/*
 *  DMI decoded fields used by the kernel, i.e. fields
 *  we care that work,
 *	see drivers/firmware/dmi_scan.c, dmi_decode()
 */
static const fwts_dmi_used_by_kernel dmi_used_by_kernel_table[] = {
	/* Type 0 BIOS Information fields */
	{ 0, 4 },
	{ 0, 5 },
	{ 0, 8 },
	{ 0, 21 },
	{ 0, 23 },
	/* Type 1, System Information */
	{ 1, 4 },
	{ 1, 5 },
	{ 1, 6 },
	{ 1, 7 },
	{ 1, 8 },
	{ 1, 25 },
	{ 1, 26 },
	/* Type 2, Base Board Information */
	{ 2, 4 },
	{ 2, 5 },
	{ 2, 6 },
	{ 2, 7 },
	{ 2, 8 },
	/* Type 3, Chassis Information */
	{ 3, 4 },
	{ 3, 5 },
	{ 3, 6 },
	{ 3, 7 },
	{ 3, 8 },
	/* Type 9, System Slots */
	{ 9, FIELD_ANY },
	/* Type 10, Onboard Devices Information */
	{ 10, FIELD_ANY },
	/* Type 11, OEM Strings */
	{ 11, FIELD_ANY },
	/* Type 38, IPMI Device Information */
	{ 38, FIELD_ANY },
	/* Type 41, Onboard Devices Extended Information */
	{ 41, FIELD_ANY },
	/* End */
	{ TYPE_EOD, 0xff },
};

#define MAX_VERSION	0xFFFF

static const fwts_dmi_type_length type_info[] = {
	/* type, max_version, min_version, length */
	{ 1, 0x201, 0x200, 0x8 },
	{ 1, 0x204, 0x201, 0x19 },
	{ 1, MAX_VERSION, 0x204, 0x1b },
	{ 4, 0x201, 0x200, 0x1a },
	{ 4, 0x203, 0x201, 0x20 },
	{ 4, 0x205, 0x203, 0x23 },
	{ 4, 0x206, 0x205, 0x28 },
	{ 4, 0x300, 0x206, 0x2a },
	{ 4, 0x306, 0x300, 0x30 },
	{ 4, 0x308, 0x306, 0x32 },
	{ 4, MAX_VERSION, 0x308, 0x33 },
	{ 7, 0x201, 0x200, 0x0f },
	{ 7, 0x301, 0x201, 0x13 },
	{ 7, MAX_VERSION, 0x301, 0x1b },
	{ 8, MAX_VERSION, 0x200, 0x9 },
	{ 11, MAX_VERSION, 0x200, 0x5 },
	{ 12, MAX_VERSION, 0x200, 0x5 },
	{ 13, MAX_VERSION, 0x200, 0x16 },
	{ 15, 0x201, 0x200, 0x14 },
	{ 16, 0x207, 0x201, 0xf },
	{ 16, MAX_VERSION, 0x207, 0x17 },
	{ 17, 0x203, 0x201, 0x15 },
	{ 17, 0x206, 0x203, 0x1b },
	{ 17, 0x207, 0x206, 0x1c },
	{ 17, 0x208, 0x207, 0x22 },
	{ 17, 0x302, 0x208, 0x28 },
	{ 17, 0x303, 0x302, 0x54 },
	{ 17, 0x307, 0x303, 0x5c },
	{ 17, MAX_VERSION, 0x307, 0x64 },
	{ 18, MAX_VERSION, 0x201, 0x17 },
	{ 19, 0x207, 0x201, 0xf },
	{ 19, MAX_VERSION, 0x207, 0x1f },
	{ 20, 0x207, 0x201, 0x13 },
	{ 20, MAX_VERSION, 0x207, 0x23 },
	{ 21, MAX_VERSION, 0x201, 0x7 },
	{ 22, MAX_VERSION, 0x201, 0x1a },
	{ 23, MAX_VERSION, 0x201, 0xd },
	{ 24, MAX_VERSION, 0x201, 0x5 },
	{ 25, MAX_VERSION, 0x201, 0x9 },
	{ 30, MAX_VERSION, 0x201, 0x6 },
	{ 33, MAX_VERSION, 0x201, 0x1f },
	{ 34, MAX_VERSION, 0x201, 0xb },
	{ 35, MAX_VERSION, 0x201, 0xb },
	{ 36, MAX_VERSION, 0x201, 0x10 },
	{ 41, MAX_VERSION, 0x201, 0xb },
	{ 43, MAX_VERSION, 0x201, 0x1f },
	{ 46, MAX_VERSION, 0x201, 0x9 },
	{ 0, 0, 0, 0 } /* terminator */
};

static int dmi_load_file(
	const char *filename,
	void *buf,
	const size_t size)
{
	int fd;
	ssize_t ret;

	(void)memset(buf, 0, size);

	if ((fd = open(filename, O_RDONLY)) < 0)
		return FWTS_ERROR;
	ret = read(fd, buf, size);
	(void)close(fd);

	if (ret != (ssize_t)size)
		return FWTS_ERROR;
	return FWTS_OK;
}

static int dmi_load_file_variable_size(
	const char *filename,
	void *buf,
	size_t *size)
{
	int fd;
	char *p;
	ssize_t count;
	size_t sz, total;

	sz = *size;
	(void)memset(buf, 0, sz);

	if ((fd = open(filename, O_RDONLY)) < 0)
		return FWTS_ERROR;

	for (p = buf, total = count = 0; ; p += count) {
		if (total >= sz) {
			close(fd);
			return FWTS_ERROR;
		}
		count = read(fd, p, sz - total);
		if (count == -1) {
			close(fd);
			return FWTS_ERROR;
		}
		if (count == 0)
			break;

		total += (size_t)count;
	}

	(void)close(fd);
	*size = total;
	return FWTS_OK;
}

static void* dmi_table_smbios(fwts_framework *fw, fwts_smbios_entry *entry)
{
	off_t addr = (off_t)entry->struct_table_address;
	size_t length = (size_t)entry->struct_table_length;
	void *table;
	void *mem;
	char anchor[8];

	/* 32 bit entry sanity check on length */
	if ((length == 0) || (length > 0xffff)) {
		fwts_log_info(fw, "SMBIOS table size of %zu bytes looks "
			"suspicious",  length);
		return NULL;
	}

	if (dmi_load_file("/sys/firmware/dmi/tables/smbios_entry_point", anchor, 4) == FWTS_OK
			&& strncmp(anchor, "_SM_", 4) == 0) {
		table = malloc(length);
		if (!table)
			return NULL;
		if (dmi_load_file("/sys/firmware/dmi/tables/DMI", table, length) == FWTS_OK) {
			fwts_log_info(fw, "SMBIOS table loaded from /sys/firmware/dmi/tables/DMI");
			return table;
		}
		free(table);
	}

#ifdef FWTS_ARCH_AARCH64
	if (!fwts_kernel_config_exist() ||
			fwts_kernel_config_set("CONFIG_STRICT_DEVMEM")) {
		fwts_warning(fw, "Skipping scanning SMBIOS table in memory for arm64 systems");
		return NULL;
	}
#endif

	mem = fwts_mmap(addr, length);
	if (mem != FWTS_MAP_FAILED) {
		/* Can we safely copy the table? */
		if (fwts_safe_memread((void *)mem, length) != FWTS_OK) {
			fwts_log_info(fw, "SMBIOS table at %p cannot be read", (void *)addr);
			(void)fwts_munmap(mem, length);
			return NULL;
		}
		table = malloc(length);
		if (table)
			memcpy(table, mem, length);
		(void)fwts_munmap(mem, length);
		return table;
	}


	fwts_log_error(fw, "Cannot mmap SMBIOS table from %8.8" PRIx32 "..%8.8" PRIx32 ".",
			entry->struct_table_address, entry->struct_table_address + entry->struct_table_length);
	return NULL;
}

static void* dmi_table_smbios30(fwts_framework *fw, fwts_smbios30_entry *entry)
{
	off_t addr = (off_t)entry->struct_table_address;
	size_t length = (size_t)entry->struct_table_max_size;
	void *table;
	void *mem;
	char anchor[8];

	/* 64 bit entry sanity check on length */
	if ((length == 0) || (length > 0xffffff)) {
		fwts_log_info(fw, "SMBIOS table size of %zu bytes looks "
			"suspicious",  length);
		return NULL;
	}

	if (dmi_load_file("/sys/firmware/dmi/tables/smbios_entry_point", anchor, 5) == FWTS_OK
			&& strncmp(anchor, "_SM3_", 5) == 0) {
		table = malloc(length);
		if (!table)
			return NULL;
		if (dmi_load_file_variable_size("/sys/firmware/dmi/tables/DMI", table, &length) == FWTS_OK) {
			fwts_log_info(fw, "SMBIOS30 table loaded from /sys/firmware/dmi/tables/DMI");
			return table;
		}
		free(table);
	}

#ifdef FWTS_ARCH_AARCH64
	if (!fwts_kernel_config_exist() ||
			fwts_kernel_config_set("CONFIG_STRICT_DEVMEM")) {
		fwts_warning(fw, "Skipping scanning SMBIOS3 table in memory for arm64 systems");
		return NULL;
	}
#endif

	mem = fwts_mmap(addr, length);
	if (mem != FWTS_MAP_FAILED) {
		/* Can we safely copy the table? */
		if (fwts_safe_memread((void *)mem, length) != FWTS_OK) {
			fwts_log_info(fw, "SMBIOS table at %p cannot be read", (void *)addr);
			(void)fwts_munmap(mem, length);
			return NULL;
		}
		table = malloc(length);
		if (table)
			memcpy(table, mem, length);
		(void)fwts_munmap(mem, length);
		return table;
	}

	fwts_log_error(fw, "Cannot mmap SMBIOS 3.0 table from %16.16" PRIx64 "..%16.16" PRIx64 ".",
			entry->struct_table_address, entry->struct_table_address + entry->struct_table_max_size);
	return NULL;
}

static void dmi_table_free(void *table)
{
	if (table)
		free(table);
}

static void dmi_dump_entry(
	fwts_framework *fw,
	const fwts_smbios_entry *entry,
	const fwts_smbios_type type)
{
	if (type == FWTS_SMBIOS) {
		fwts_log_info_verbatim(fw, "SMBIOS Entry Point Structure:");
		fwts_log_info_verbatim(fw, "  Anchor String          : %4.4s", entry->signature);
		fwts_log_info_simp_int(fw, "  Checksum               : ", entry->checksum);
		fwts_log_info_simp_int(fw, "  Entry Point Length     : ", entry->length);
		fwts_log_info_simp_int(fw, "  Major Version          : ", entry->major_version);
		fwts_log_info_simp_int(fw, "  Minor Version          : ", entry->minor_version);
		fwts_log_info_simp_int(fw, "  Maximum Struct Size    : ", entry->max_struct_size);
		fwts_log_info_simp_int(fw, "  Entry Point Revision   : ", entry->revision);
		fwts_log_info_verbatim(fw, "  Formatted Area         : 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x",
			entry->formatted_area[0], entry->formatted_area[1],
			entry->formatted_area[2], entry->formatted_area[3],
			entry->formatted_area[4]);
	}
	if (type == FWTS_SMBIOS_DMI_LEGACY)
		fwts_log_info_verbatim(fw, "Legacy DMI Entry Point Structure:");

	/* Common to SMBIOS and SMBIOS_DMI_LEGACY */
	fwts_log_info_verbatim(fw, "  Intermediate Anchor    : %5.5s", (const char *)entry->anchor_string);
	fwts_log_info_simp_int(fw, "  Intermediate Checksum  : ", entry->intermediate_checksum);
	fwts_log_info_simp_int(fw, "  Structure Table Length : ", entry->struct_table_length);
	fwts_log_info_simp_int(fw, "  Structure Table Address: ", entry->struct_table_address);
	fwts_log_info_simp_int(fw, "  # of SMBIOS Structures : ", entry->number_smbios_structures);
	fwts_log_info_verbatim(fw, "  SMBIOS BCD Revision    : %2.2x", entry->smbios_bcd_revision);
	if (entry->smbios_bcd_revision == 0)
		fwts_log_info_verbatim(fw, "    BCD Revision 00 indicates compliance with specification stated in Major/Minor Version.");
}

static void dmi_dump_entry30(
	fwts_framework *fw,
	const fwts_smbios30_entry *entry)
{
	fwts_log_info_verbatim(fw, "SMBIOS30 Entry Point Structure:");
	fwts_log_info_verbatim(fw, "  Anchor String          : %5.5s", entry->signature);
	fwts_log_info_simp_int(fw, "  Checksum               : ", entry->checksum);
	fwts_log_info_simp_int(fw, "  Entry Point Length     : ", entry->length);
	fwts_log_info_simp_int(fw, "  Major Version          : ", entry->major_version);
	fwts_log_info_simp_int(fw, "  Minor Version          : ", entry->minor_version);
	fwts_log_info_simp_int(fw, "  Docrev                 : ", entry->docrev);
	fwts_log_info_simp_int(fw, "  Entry Point Revision   : ", entry->revision);
	fwts_log_info_simp_int(fw, "  Reserved               : ", entry->reserved);
	fwts_log_info_simp_int(fw, "  Table maximum size     : ", entry->struct_table_max_size);
	fwts_log_info_simp_int(fw, "  Table address          : ", entry->struct_table_address);

}

static int dmi_sane(fwts_framework *fw, fwts_smbios_entry *entry)
{
	uint8_t	*table, *ptr;
	uint8_t dmi_entry_type = 0;
	uint16_t i = 0;
	uint16_t table_length = entry->struct_table_length;
	int ret = FWTS_OK;

	ptr = table = dmi_table_smbios(fw, entry);
	if (table == NULL)
		return FWTS_ERROR;

	for (i = 0; i < entry->number_smbios_structures; i++) {
		uint8_t dmi_entry_length;

		if (ptr > table + table_length) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"SMBIOSTableLengthTooSmall",
				"The size indicated by the SMBIOS table length is "
				"smaller than the DMI data.");
			ret = FWTS_ERROR;
			break;
		}

		dmi_entry_type = ptr[0];
		dmi_entry_length = ptr[1];

		if (dmi_entry_length < 4) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"SMBIOSIllegalTableEntry",
				"The size of a DMI entry %" PRIu16 " is illegal, "
				"DMI data is either wrong or the SMBIOS Table "
				"Pointer is pointing to the wrong memory region.", i);
			ret = FWTS_ERROR;
			break;
		}
		ptr += dmi_entry_length;

		/* Scan for end of DMI entry, must be 2 zero bytes */
		while (((ptr - table + 1) < table_length) &&
		       ((ptr[0] != 0) || (ptr[1] != 0)))
				ptr++;
		/* Skip over the two zero bytes */
		ptr += 2;
	}

	/* We found DMI end of table, are number of entries correct? */
	if ((dmi_entry_type == 127) && (i != entry->number_smbios_structures)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SMBIOSNumberOfStructures",
			"The end of DMI table marker structure was found "
			"but only %d DMI structures were found. The SMBIOS "
			"entry table reported that there were %" PRIu16
			" DMI structures in the DMI table.",
			i, entry->number_smbios_structures);
		/* And table length can't be correct either */
		ret = FWTS_ERROR;
	}

	dmi_table_free(table);

	return ret;
}

static int smbios_entry_check(fwts_framework *fw)
{
	void *addr = 0;

	fwts_smbios_entry entry;
	fwts_smbios_type  type;
	uint16_t	  version;
	uint8_t checksum;
	static char warning[] =
		"This field is not checked by the kernel, and so will not affect the system, "
		"however it should be fixed to conform to the latest version of the "
		"System Management BIOS (SMBIOS) Reference Specification. ";

	fwts_log_info(fw,
		"This test tries to find and sanity check the SMBIOS "
		"data structures.");

	if ((addr = fwts_smbios_find_entry(fw, &entry, &type, &version)) == NULL)
		return FWTS_ERROR;

	fwts_passed(fw, "Found SMBIOS Table Entry Point at %p", addr);
	dmi_dump_entry(fw, &entry, type);
	fwts_log_nl(fw);

	if (type == FWTS_SMBIOS) {
		checksum = fwts_checksum((uint8_t *)&entry, sizeof(fwts_smbios_entry));
		if (checksum != 0)
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"SMBIOSBadChecksum",
				"SMBIOS Table Entry Point Checksum is 0x%2.2x, should be 0x%2.2x",
					entry.checksum, (uint8_t)(entry.checksum - checksum));
		else
			fwts_passed(fw, "SMBIOS Table Entry Point Checksum is valid.");

		if (entry.length != 0x1f) {
			fwts_failed(fw, LOG_LEVEL_LOW,
				"SMBIOSBadEntryLength",
				"SMBIOS Table Entry Point Length is 0x%2.2x, should be 0x1f", entry.length);
			fwts_advice(fw, "%s Note that version 2.1 of the specification incorrectly stated that the "
				"Entry Point Length should be 0x1e when it should be 0x1f.", warning);
		} else
			fwts_passed(fw, "SMBIOS Table Entry Point Length is valid.");
	}

	if (memcmp(entry.anchor_string, "_DMI_", 5) != 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SMBIOSBadIntermediateAnchor",
			"SMBIOS Table Entry Intermediate Anchor String was '%5.5s' and should be '_DMI_'.",
			entry.anchor_string);
		fwts_advice(fw, "%s", warning);
	} else
		fwts_passed(fw, "SMBIOS Table Entry Intermediate Anchor String _DMI_ is valid.");

	/* Intermediate checksum for legacy DMI */
	checksum = fwts_checksum(((uint8_t *)&entry)+16, 15);
	if (checksum != 0)
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SMBIOSBadChecksum",
			"SMBIOS Table Entry Point Intermediate Checksum is 0x%2.2x, should be 0x%2.2x",
				entry.intermediate_checksum,
				(uint8_t)(entry.intermediate_checksum - checksum));
	else
		fwts_passed(fw, "SMBIOS Table Entry Point Intermediate Checksum is valid.");

	if ((entry.struct_table_length > 0) && (entry.struct_table_address == 0)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SMBIOSBadTableAddress",
			"SMBIOS Table Entry Structure Table Address is NULL and should be defined.");
		fwts_advice(fw,
			"The address of the SMBIOS Structure Tables must be defined if the "
			"length of these tables is defined.");
	} else {
		/*
		 * Now does the DMI table look sane? If not,
		 * the SMBIOS Structure Table could be bad
		 */
		if (dmi_sane(fw, &entry) == FWTS_OK)
			fwts_passed(fw, "SMBIOS Table Entry Structure Table Address and Length looks valid.");
	}

	return FWTS_OK;

}

static int dmi_smbios30_sane(fwts_framework *fw, fwts_smbios30_entry *entry)
{
	uint8_t	*table, *ptr;
	uint16_t i = 0;
	uint32_t table_length = entry->struct_table_max_size;
	int ret = FWTS_OK;

	ptr = table = dmi_table_smbios30(fw, entry);
	if (table == NULL)
		return FWTS_ERROR;

	for (;;) {
		uint8_t struct_length;
		uint8_t struct_type;

		if (ptr > table + table_length) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"SMBIOS30TableLengthTooSmall",
				"The maximum size indicated by the SMBIOS 3.0 table length is "
				"smaller than the dmi data or the DMI end of table not found.");
			ret = FWTS_ERROR;
			break;
		}

		struct_type = ptr[0];
		struct_length = ptr[1];

		if (struct_length < 4) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"SMBIOSIllegalTableEntry",
				"The size of a DMI entry %" PRIu16 " is illegal, "
				"DMI data is either wrong or the SMBIOS Table "
				"Pointer is pointing to the wrong memory region.", i);
			ret = FWTS_ERROR;
			break;
		}
		ptr += struct_length;

		/* Scan for end of DMI entry, must be 2 zero bytes */
		while (((ptr - table + 1) < (ssize_t)table_length) &&
		       ((ptr[0] != 0) || (ptr[1] != 0)))
				ptr++;
		/* Skip over the two zero bytes */
		ptr += 2;

		/* We found DMI end of table and inside the maximum length? */
		if (struct_type == 127) {
			if (ptr <= table + table_length)
				break;
			else {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"SMBIOS30TableLengthTooSmall",
					"The end of DMI table marker structure was found "
					"but outside the structure table maximum size");
				ret = FWTS_ERROR;
				break;
			}
		}
	}

	dmi_table_free(table);

	return ret;
}

static int smbios30_entry_check(fwts_framework *fw)
{
	void *addr = 0;

	fwts_smbios30_entry entry;
	uint16_t version;
	uint8_t checksum;

	if ((addr = fwts_smbios30_find_entry(fw, &entry, &version)) == NULL)
		return FWTS_ERROR;

	fwts_passed(fw, "Found SMBIOS30 Table Entry Point at %p", addr);
	dmi_dump_entry30(fw, &entry);
	fwts_log_nl(fw);

	checksum = fwts_checksum((uint8_t *)&entry, sizeof(fwts_smbios30_entry));
	if (checksum != 0)
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SMBIOS30BadChecksum",
			"SMBIOS30 Table Entry Point Checksum is 0x%2.2" PRIx8
			", should be 0x%2.2" PRIx8,
			entry.checksum, (uint8_t)(entry.checksum - checksum));
	else
		fwts_passed(fw, "SMBIOS30 Table Entry Point Checksum is valid.");

	if (entry.length != 0x18) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SMBIOS30BadEntryLength",
			"SMBIOS30 Table Entry Point Length is 0x%2.2"  PRIx8
			", should be 0x18", entry.length);
	} else
		fwts_passed(fw, "SMBIOS30 Table Entry Point Length is valid.");

	if (entry.reserved)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SMBIOSBadReserved",
			"SMBIOS30 Table Entry Point Reserved is 0x%2.2" PRIx8
			", should be 0", entry.reserved);

	if ((entry.revision == 1) && (entry.struct_table_address == 0)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SMBIOS30BadTableAddress",
			"SMBIOS Table Entry Structure Table Address is NULL and should be defined.");
	} else {
		/*
		 * Now does the SMBIOS 3.0.0 table look sane? If not,
		 * the SMBIOS Structure Table could be bad
		 */
		if (dmi_smbios30_sane(fw, &entry) == FWTS_OK)
			fwts_passed(fw, "SMBIOS 3.0 Table Entry Structure Table Address and Length looks valid.");
	}

	return FWTS_OK;

}

static int dmicheck_test1(fwts_framework *fw)
{

	if (smbios_entry_check(fw) != FWTS_ERROR) {
		smbios_found = true;
	}

	if (smbios30_entry_check(fw) != FWTS_ERROR) {
		smbios30_found = true;
	}

	if (!smbios30_found) {
		if (!(fw->flags & FWTS_FLAG_SBBR)) {
			if (smbios_found)
				return FWTS_OK;
		}
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"SMBIOSNoEntryPoint",
			"Could not find any SMBIOS Table Entry Points.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static bool dmi_used_by_kernel(const uint8_t type, const uint8_t offset)
{
	int i;

	for (i = 0; dmi_used_by_kernel_table[i].type != TYPE_EOD; i++) {
		if (dmi_used_by_kernel_table[i].type == type)
			if ((dmi_used_by_kernel_table[i].offset == FIELD_ANY) ||
			    (dmi_used_by_kernel_table[i].offset == offset))
				return true;
	}
	return false;
}

static uint16_t dmi_remap_version(fwts_framework *fw, const uint16_t old)
{
	int i;

	for (i = 0; dmi_versions[i].old != 0; i++) {
		if (old == dmi_versions[i].old) {
			const uint16_t new = dmi_versions[i].new;

			fwts_warning(fw,
				"Detected a buggy DMI version number "
				"%" PRIu16 ".%" PRIu16 "remapping to "
				"%" PRIu16 ".%" PRIu16,
				VERSION_MAJOR(old), VERSION_MINOR(old),
				VERSION_MAJOR(new), VERSION_MINOR(new));
			return new;
		}
	}

	/* All OK, return original */
	return old;
}

static void dmi_out_of_range_advice(
	fwts_framework *fw,
	const uint8_t type,
	const uint8_t offset)
{
	if (dmi_used_by_kernel(type, offset))
		fwts_advice(fw,
			"A value that is out of range is incorrect and not conforming to "
			"the SMBIOS specification.  The Linux kernel extracts and uses "
			"this particular data item, so it is recommended that this SMBIOS "
			"configuration is corrected even if the impact on the system "
			"is most probably not critical.");
	else
		fwts_advice(fw,
			"A value that is out of range is incorrect and not conforming to "
			"the SMBIOS specification.  This field is not currently used by "
			"the Linux kernel, so this firmware bug shouldn't cause any "
			"problems.");
}

static void dmi_reserved_bits_check(
	fwts_framework *fw,
	const char *table,
	const uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	const size_t size,
	const uint8_t offset,
	const uint8_t min,
	const uint8_t max)
{
	uint64_t mask = 0;
	uint64_t val;
	uint8_t i;

	for (i = min; i <= max; i++) {
		mask |= (1ULL << i);
	}

	switch (size) {
	case sizeof(uint8_t):
		val = hdr->data[offset];
		if (val & mask) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_RESERVED_BIT_USED,
				"Value 0x%2.2" PRIx8 " was used but bits %" PRIu8
				"..%" PRIu8 " should be reserved while accessing entry "
				"'%s' @ 0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
				(uint8_t)val, min, max, table, addr, field, offset);
		}
		break;
	case sizeof(uint16_t):
		val = GET_UINT16((hdr->data) + offset);
		if (val & mask) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_RESERVED_BIT_USED,
				"Value 0x%4.4" PRIx16 " was used but bits %" PRIu8
				"..%" PRIu8 " should be reserved while accessing entry "
				"'%s' @ 0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
				(uint16_t)val, min, max, table, addr, field, offset);
		}
		break;
	case sizeof(uint32_t):
		val = GET_UINT32((hdr->data) + offset);
		if (val & mask) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_RESERVED_BIT_USED,
				"Value 0x%8.8" PRIx32 " was used but bits %" PRIu8
				"..%" PRIu8 " should be reserved while accessing entry "
				"'%s' @ 0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
				(uint32_t)val, min, max, table, addr, field, offset);
		}
		break;
	case sizeof(uint64_t):
		val = GET_UINT64((hdr->data) + offset);
		if (val & mask) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_RESERVED_BIT_USED,
				"Value 0x%16.16" PRIx64 " was used but bits %" PRIu8
				"..%" PRIu8 " should be reserved while accessing entry "
				"'%s' @ 0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
				val, min, max, table, addr, field, offset);
		}
		break;
	}
}

static void dmi_reserved_uint8_check(
	fwts_framework *fw,
	const char *table,
	const uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	const uint8_t offset)
{
	if (hdr->data[offset] != 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_RESERVED_OFFSET_NONZERO,
			"Reserved offset value is 0x%2.2" PRIx8 " (nonzero) "
			"while accessing entry '%s' @ 0x%8.8" PRIx32
			", field '%s', offset 0x%2.2" PRIx8,
			hdr->data[offset], table, addr, field, offset);
	}
}

static void _dmi_ranges_uint8_check(
	fwts_framework *fw,
	const char *table,
	const uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	const uint8_t offset,
	const fwts_dmi_value_range *ranges,
	const uint32_t range_size)
{
	uint16_t i;
	uint8_t val = hdr->data[offset];

	for (i = 0; i < range_size / sizeof(fwts_dmi_value_range); i++) {
		const fwts_dmi_value_range *range = ranges + i;

		if ((val >= range->min) && (val <= range->max))
			return;
	}

	fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
		"Out of range value 0x%2.2" PRIx8 " "
		"while accessing entry '%s' @ 0x%8.8" PRIx32
		", field '%s', offset 0x%2.2" PRIx8,
		val, table, addr, field, offset);
		dmi_out_of_range_advice(fw, hdr->type, offset);
}

static void dmi_min_max_uint32_check(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset,
	uint32_t min,
	uint32_t max)
{
	uint32_t val = GET_UINT32((hdr->data) + offset);
	if ((val < min) || (val > max)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			DMI_VALUE_OUT_OF_RANGE,
			"Out of range value 0x%4.4" PRIx32
			" (range allowed 0x%4.4" PRIx32 "..0x%4.4" PRIx32 ") "
			"while accessing entry '%s' @ 0x%8.8" PRIx32
			", field '%s', offset 0x%2.2" PRIx8,
			val, min, max, table, addr, field, offset);
		dmi_out_of_range_advice(fw, hdr->type, offset);
	}
}

static void dmi_min_max_uint16_check(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset,
	uint16_t min,
	uint16_t max)
{
	uint16_t val = GET_UINT16((hdr->data) + offset);
	if ((val < min) || (val > max)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			DMI_VALUE_OUT_OF_RANGE,
			"Out of range value 0x%4.4" PRIx16
			" (range allowed 0x%4.4" PRIx16 "..0x%4.4" PRIx16 ") "
			"while accessing entry '%s' @ 0x%8.8" PRIx32
			", field '%s', offset 0x%2.2" PRIx8,
			val, min, max, table, addr, field, offset);
		dmi_out_of_range_advice(fw, hdr->type, offset);
	}
}

static void dmi_min_max_uint8_check(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset,
	uint8_t min,
	uint8_t max)
{
	uint8_t val = hdr->data[offset];
	if ((val < min) || (val > max)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			DMI_VALUE_OUT_OF_RANGE,
			"Out of range value 0x%2.2" PRIx8
			" (range allowed 0x%2.2" PRIx8 "..0x%2.2" PRIx8 ") "
			"while accessing entry '%s' @ 0x%8.8" PRIx32
			", field '%s', offset 0x%2.2" PRIx8,
			val, min, max, table, addr, field, offset);
		dmi_out_of_range_advice(fw, hdr->type, offset);
	}
}

static void dmi_min_max_mask_uint8_check(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset,
	uint8_t min,
	uint8_t max,
	uint8_t shift,
	uint8_t mask)
{
	uint8_t val = (hdr->data[offset] >> shift) & mask;

	if ((val < min) || (val > max)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
			"Out of range value 0x%2.2" PRIx8
			" (range allowed 0x%2.2" PRIx8 "..0x%2.2" PRIx8 ") "
			"while accessing entry '%s' @ 0x%8.8" PRIx32
			", field '%s', offset 0x%2.2" PRIx8,
			val, min, max, table, addr, field, offset);
		dmi_out_of_range_advice(fw, hdr->type, offset);
	}
}

static void dmi_str_check_index(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset,
	uint8_t index)
{
	char	*data = (char *)hdr->data;
	uint8_t	i = index;
	bool used_by_kernel = dmi_used_by_kernel(hdr->type, offset);

	if (i > 0) {
		int	j;
		int	failed = -1;

		data += hdr->length;
		while (i > 1 && *data) {
			data += strlen(data) + 1;
			i--;
		}

		/* Sanity checks */
		if (*data == '\0') {
			int level = used_by_kernel ? LOG_LEVEL_HIGH : LOG_LEVEL_LOW;

			/* This entry is clearly broken so flag it as a corrupt entry */
			fwts_failed(fw, level, DMI_STRING_INDEX_OUT_OF_RANGE,
				"Out of range string index 0x%2.2" PRIx8
				" while accessing entry '%s' "
				"@ 0x%8.8" PRIx32 ", field '%s', offset 0x%2.2" PRIx8,
				index, table, addr, field, offset);
			if (used_by_kernel)
				fwts_advice(fw,
					"DMI strings are stored in a manner that uses a special "
					"index to fetch the Nth string from the data. For this "
					"particular DMI string the index given is not in range "
					"which means this particular entry is broken. The Linux "
					"kernel uses this string - hence this string should be "
					"fixed to ensure sane data is passed to the kernel. "
					"Note that this probably won't cause any critical system "
					"errors.");
			else
				fwts_advice(fw,
					"DMI strings are stored in a manner that uses a special "
					"index to fetch the Nth string from the data. For this "
					"particular DMI string the index given is not in range "
					"which means this particular entry is broken. The Linux "
					"kernel does not use this string, so this error will not "
					"cause any system errors.");
			return;
		}

		/* Scan for known BIOS defaults that vendors forget to set */
		for (j = 0; dmi_patterns[j].label != NULL; j++) {
			if (fw->flags & FWTS_FLAG_FIRMWARE_VENDOR)
				break;

			if (dmi_patterns[j].field &&
				(strcmp(dmi_patterns[j].field, field) == 0) &&
				(strcmp(dmi_patterns[j].value, data) == 0)) {
				failed = j;
				break;
			} else if (strcmp(dmi_patterns[j].value, data) == 0) {
				failed = j;
				break;
			}
		}
		if (failed != -1) {
			int level = used_by_kernel ? LOG_LEVEL_MEDIUM : LOG_LEVEL_LOW;

			fwts_failed(fw, level, dmi_patterns[j].label,
				"String index 0x%2.2" PRIx8
				" in table entry '%s' @ 0x%8.8" PRIx32
				", field '%s', offset 0x%2.2" PRIx8
				" has a default value '%s' and probably has "
				"not been updated by the BIOS vendor.",
				index, table, addr, field, offset, data);

			if (used_by_kernel) {
				fwts_advice(fw,
					"The DMI table contains data which is clearly been "
					"left in a default setting and not been configured "
					"for this machine. "
					"Somebody has probably forgotten to define this "
					"field and it basically means this field is effectively "
					"useless. Note that the kernel uses this field so "
					"it probably should be corrected to ensure the kernel "
					"is using sane values.");
			} else {
				/* This string is broken, but we don't care about it too much */
				fwts_advice(fw,
					"The DMI table contains data which is clearly been "
					"left in a default setting and not been configured "
					"for this machine. "
					"Somebody has probably forgotten to define this "
					"field and it basically means this field is effectively "
					"useless, however the kernel does not use this data "
					"so the issue is fairly low.");
			}
		}
	}
}

static void dmi_str_check(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset)
{
	dmi_str_check_index(fw, table, addr, field, hdr, offset, hdr->data[offset]);
}

static void dmi_uuid_check(fwts_framework *fw,
	const char *table,
	uint32_t addr,
	const char *field,
	const fwts_dmi_header *hdr,
	uint8_t offset)
{
	char guid_str[37];
	int i;

	fwts_guid_buf_to_str(hdr->data + offset, guid_str, sizeof(guid_str));

	for (i = 0; uuid_patterns[i] != NULL; i++) {
		if (strcmp(guid_str, uuid_patterns[i]) == 0) {
			fwts_failed(fw, LOG_LEVEL_LOW, DMI_BAD_UUID,
				"UUID in table entry '%s' @ 0x%8.8" PRIx32
				" field '%s', offset 0x%2.2" PRIx8
				" has a default value '%s' and probably has "
				"not been updated by the BIOS vendor.",
				table, addr, field, offset, guid_str);
			fwts_advice(fw,
				"The DMI table contains a UUID which is clearly been "
				"left in a default setting and not been configured "
				"for this machine.  While this is not critical it does "
				"mean that somebody has probably forgotten to define this "
				"field and it basically means this field is effectively "
				"useless.");
		}
	}
}

static void dmicheck_type_length(
	fwts_framework *fw,
	const uint16_t smbios_version,
	const fwts_dmi_header *hdr)
{
	const fwts_dmi_type_length *tbl = type_info;
	const uint8_t *data = hdr->data;
	uint8_t length = hdr->length;
	bool passed = false;

	/* Special cases */
	switch (hdr->type) {
		case 15:
			if (smbios_version >= 0x201 && length >= 17) {
				length = 0x17 + data[0x15] * data[0x16];
				if (length == hdr->length)
					passed = true;
			}
			break;
		case 17:
			if (smbios_version >= 0x302 && length >= 0x34)
				passed = true;
			break;
		default:
			passed = true;
			break;
	}

	/* general cases */
	while (tbl->length != 0) {
		if (hdr->type != tbl->type) {
				tbl++;
				continue;
		}
		if (smbios_version >= tbl->min_version && smbios_version < tbl->max_version) {
			if (length == tbl->length) {
				passed = true;
			} else
				passed = false;
			break;
		}
		tbl++;
	}

	if (!passed)
		fwts_failed(fw, LOG_LEVEL_HIGH, DMI_BAD_TABLE_LENGTH,
			"Type %" PRId8 " expects length of 0x%" PRIx8
			", has incorrect length of 0x%" PRIx8,
			hdr->type, tbl->length, length);
}

static void dmicheck_entry(fwts_framework *fw,
	uint32_t addr,
	const fwts_dmi_header *hdr,
	const uint16_t smbios_version)
{
	uint8_t *ptr;
	uint8_t count;
	uint8_t val;
	uint8_t *data = hdr->data;
	char	tmp[64];
	char	*table;
	int	i;
	int	len;
	uint32_t failed_count = fw->minor_tests.failed;
	uint32_t battery_count;
	int	ret;

	dmicheck_type_length(fw, smbios_version, hdr);

	switch (hdr->type) {
		case 0: /* 7.1 */
			table = "BIOS Information (Type 0)";
			if (hdr->length < 0x12)
				break;
			dmi_str_check(fw, table, addr, "Vendor", hdr, 0x4);
			dmi_str_check(fw, table, addr, "BIOS Version", hdr, 0x5);
			dmi_str_check(fw, table, addr, "Release Date", hdr, 0x8);
			if (hdr->length < 0x18)
				break;
			dmi_reserved_bits_check(fw, table, addr, "BIOS Characteristics Extension Byte 2", hdr, sizeof(uint8_t), 0x13, 5, 7);

			/* new fields in spec 3.11 */
			if (hdr->length < 0x1a)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Extended BIOS ROM Size", hdr, sizeof(uint16_t), 0x18, 15, 15);
			break;

		case 1: /* 7.2 */
			table = "System Information (Type 1)";
			if (hdr->length < 0x08)
				break;
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0x4);
			dmi_str_check(fw, table, addr, "Product Name", hdr, 0x5);
			dmi_str_check(fw, table, addr, "Version", hdr, 0x6);
			dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x7);
			if (hdr->length < 0x19)
				break;
			dmi_uuid_check(fw, table, addr, "UUID", hdr, 0x8);
			dmi_min_max_uint8_check(fw, table, addr, "Wakeup Type", hdr, 0x18, 0x0, 0x08);
			if (hdr->length < 0x1b)
				break;
			dmi_str_check(fw, table, addr, "SKU Number", hdr, 0x19);
			dmi_str_check(fw, table, addr, "Family", hdr, 0x1a);
			break;

		case 2: /* 7.3 */
			table = "Base Board Information (Type 2)";
			if (hdr->length < 0x08)
				break;
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0x4);
			dmi_str_check(fw, table, addr, "Product", hdr, 0x5);
			dmi_str_check(fw, table, addr, "Version", hdr, 0x6);
			dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x7);
			if (hdr->length < 0x09)
				break;
			dmi_str_check(fw, table, addr, "Asset Tag", hdr, 0x8);
			if (hdr->length < 0x0f)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Feature Flags", hdr, sizeof(uint8_t), 0x9, 5, 7);
			dmi_str_check(fw, table, addr, "Location In Chassis", hdr, 0xa);
			dmi_min_max_uint8_check(fw, table, addr, "Type", hdr, 0xd, 0x1, 0xd);
			break;

		case 3: /* 7.4 */
			table = "Chassis Information (Type 3)";
			if (hdr->length < 0x09)
				break;
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0x4);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Chassis Type", hdr, 0x5, 0x1, FWTS_SMBIOS_CHASSIS_MAX - 1, 0x0, 0x7f);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Chassis Lock", hdr, 0x5, 0x0, 0x1, 0x7, 0x1);
			dmi_str_check(fw, table, addr, "Version", hdr, 0x6);
			dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x7);
			dmi_str_check(fw, table, addr, "Asset Tag", hdr, 0x8);
			dmi_min_max_uint8_check(fw, table, addr, "Boot-up State", hdr, 0x9, 0x1, 0x6);
			dmi_min_max_uint8_check(fw, table, addr, "Power Supply State", hdr, 0xa, 0x1, 0x6);
			dmi_min_max_uint8_check(fw, table, addr, "Thermal State", hdr, 0xb, 0x1, 0x6);
			dmi_min_max_uint8_check(fw, table, addr, "Security Status", hdr, 0xc, 0x1, 0x5);
			if (hdr->length < 0x15 + data[0x13] * data[0x14])
				break;
			ptr = data + 0x15;
			len = data[0x14];
			if (len >= 0x3) {
				for (i = 0; i < data[0x13]; i++) {
					val = ptr[i * len] & 0x7f;
					if (ptr[i * len] & 0x80) {
						if (val > 0x42)
							fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
								"Out of range value 0x%2.2" PRIx8
								" (range allowed 0x00..0x42) "
								"while accessing entry '%s' @ "
								"0x%8.8" PRIx32 ", field "
								"'SMBIOS Structure Type %d', "
								"offset 0x%2.2x",
								val, table, addr, i, 0x15 + (i*len));
					} else {
						if ((val < 0x1) || (val > 0xd)) {
							fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
								"Out of range value 0x%2.2" PRIx8
								" (range allowed 0x01..0x0d) "
								"while accessing entry '%s' @ "
								"0x%8.8" PRIx32 ", field "
								"'Base Board Type %d', offset 0x%2.2x",
								val, table, addr, i, 0x15 + (i*len));
						}
					}
				}
			}
			if (hdr->length < 0x16 + data[0x13] * data[0x14])
				break;
			dmi_str_check(fw, table, addr, "SKU Number", hdr, 0x15 + data[0x13] * data[0x14]);
			break;

		case 4: /* 7.5 */
			table = "Processor Information (Type 4)";
			if (hdr->length < 0x1a)
				break;
			dmi_str_check(fw, table, addr, "Socket Designation", hdr, 0x4);
			dmi_min_max_uint8_check(fw, table, addr, "Processor Type", hdr, 0x5, 0x1, 0x6);
			dmi_min_max_uint8_check(fw, table, addr, "Processor Family", hdr, 0x6, 0x1, 0xfe);
			dmi_str_check(fw, table, addr, "Processor Manufacturer", hdr, 0x7);
			/* Check for Processor ID */
			#ifdef FWTS_ARCH_AARCH64
			if (hdr->length >= 0x28)
			{
				/* Get "Processor Characteristics" and check 9th bit if Arm64 SoC ID is supported */
				uint32_t processor_ID_first_DWORD  = GET_UINT32(data + 0x8);  /* Getting 0x8, 0x9 ,0xA, 0xB */
				uint32_t processor_ID_second_DWORD = GET_UINT32(data + 0xC);  /* Getting 0xC, 0xD ,0xE, 0xF */

				if(((GET_UINT16(data + 0x26) >> 8) & 0x02) == 0x02)
				{
					/* Arm64 SoC ID is Supported. The second DWORD should be non-zero */
					if (processor_ID_second_DWORD == 0)
					{
						fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
							"According to the configured \"Processor Characteristics\", Arm64 SoC ID is supported "
							"however, the SoC revision value is 0x%8.8" PRIx32 " . Non-zero expected.",
							processor_ID_second_DWORD );
						
						fwts_advice(fw,
							"Arm64 SoC ID support is configured to true in \"Processor Characteristics\" "
							"however, the SiP SoC revision value is 0. "
							"It is possible that the system is not correctly configured for "
							"Arm64 SoC ID and still has the old configuration of "
							"MIDR_EL1 register. If Arm64 SoC ID is indeed not supported " 
							"then reset the bit in \"Processor Characteristics\".");
					}	
				}
				else
				{
					/* Arm64 SoC ID is NOT Supported
					 *  First DWORD should have contents of MIDR_EL1
					 *  Second DWORD is reserved and must be 0
					 */
					if (processor_ID_second_DWORD != 0 )
					{
						fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
							"According to the configured \"Processor Characteristics\" Arm64 SoC ID is NOT supported "
							"however, the DWORD in offset is 0Ch-0Fh is non-zero: 0x%8.8" PRIx32 " ",
							processor_ID_second_DWORD );
					
						fwts_advice(fw,
							"Arm64 SoC ID support is configured to false in \"Processor Characteristics\" "
							"however, the DWORD in offset is 0Ch-0Fh is non-zero. Configure the " 
							"\"Processor Characteristics\" "
							"correctly if Arm64 SoC ID is indeed supported in the system.");
					}
					if ( processor_ID_first_DWORD == 0 )
					{
						fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
							"According to the configured \"Processor Characteristics\" Arm64 SoC ID is NOT supported "
							"however, the DWORD in offset is 08h-0bh is zero: 0x%8.8" PRIx16 " This should contain MIDR_EL1 value",
							processor_ID_first_DWORD );
					
						fwts_advice(fw,
							"Arm64 SoC ID support is configured to false in \"Processor Characteristics\" "
							"however, the DWORD in offset is 08h-0bh is zero. This must contain MIDR_EL1");
					}
				}
			}
			#endif
			dmi_str_check(fw, table, addr, "Processor Version", hdr, 0x10);
			if (((GET_UINT8(data + 0x18) & 0x07) == 0x5) || ((GET_UINT8(data + 0x18) & 0x07) == 0x6))
				fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
					"Out of range value 0x%2.2" PRIx8 " "
					"bits 0..2 set to illegal value 0x5 or 0x6 "
					"while accessing entry '%s' @ 0x%8.8" PRIx32
					", field '%s', offset 0x%2.2x",
					GET_UINT8(data + 0x18),
					table, addr, "Status", 0x18);
			dmi_reserved_bits_check(fw, table, addr, "Status", hdr, sizeof(uint8_t), 0x18, 3, 5);
			dmi_reserved_bits_check(fw, table, addr, "Status", hdr, sizeof(uint8_t), 0x18, 7, 7);
			fwts_dmi_value_range t4_ranges[] = {{0x1, 0x57}, {0xff, 0xff}};
			dmi_ranges_uint8_check(fw, table, addr, "Processor Upgrade", hdr, 0x19, t4_ranges);
			if (hdr->length < 0x23)
				break;
			dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x20);
			dmi_str_check(fw, table, addr, "Asset Tag", hdr, 0x21);
			dmi_str_check(fw, table, addr, "Part Number", hdr, 0x22);
			if (hdr->length < 0x28)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Processor Characteristics", hdr, sizeof(uint16_t), 0x26, 0, 0);
			dmi_reserved_bits_check(fw, table, addr, "Processor Characteristics", hdr, sizeof(uint16_t), 0x26, 10, 15);
			if (hdr->length < 0x2a)
				break;
			dmi_min_max_uint16_check(fw, table, addr, "Processor Family 2", hdr, 0x28, 0x1, 0xfffd);
			if (hdr->length < 0x30)
				break;
			dmi_min_max_uint16_check(fw, table, addr, "Core Count 2", hdr, 0x2a, 0, 0xfffe);
			dmi_min_max_uint16_check(fw, table, addr, "Core Enabled 2", hdr, 0x2c, 0, 0xfffe);
			dmi_min_max_uint16_check(fw, table, addr, "Thread Count 2", hdr, 0x2e, 0, 0xfffe);
			if (hdr->length < 0x32)
				break;
			dmi_min_max_uint16_check(fw, table, addr, "Thread Enabled", hdr, 0x30, 0, 0xfffe);
			if (hdr->length < 0x33)
				break;
			dmi_str_check(fw, table, addr, "Socket Type", hdr, 0x32);
			if (GET_UINT8(data + 0x19) == 0xFF && ((char)data[0x32] == '\0'))
				fwts_failed(fw, LOG_LEVEL_HIGH,
					DMI_BAD_STRING,
					"Must be non-null if 'Processor Upgrade' at offset 19h is FFh "
					"while accessing entry '%s' @ 0x%8.8" PRIx32
					"on field '%s', offset 0x%2.2" PRIx8,
					table, addr, "Socket Type", 0x32);
			break;

		case 5: /* 7.6 (Type 5 is obsolete) */
			table = "Memory Controller Information (Type 5)";
			if (hdr->length < 0x0f)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Error Detecting Method", hdr, 0x4, 0x1, 0x8);
			dmi_min_max_uint8_check(fw, table, addr, "Supported Interleave", hdr, 0x6, 0x1, 0x7);
			dmi_min_max_uint8_check(fw, table, addr, "Current Interleave", hdr, 0x7, 0x1, 0x7);
			break;

		case 6: /* 7.7 (Type 6 is obsolete) */
			table = "Memory Module Information (Type 6)";
			if (hdr->length < 0x0c)
				break;
			dmi_str_check(fw, table, addr, "Socket Designation", hdr, 0x4);
			break;

		case 7: /* 7.8 */
			table = "Cache Information (Type 7)";
			if (hdr->length < 0x0f)
				break;
			dmi_str_check(fw, table, addr, "Socket Designation", hdr, 0x4);
			if (((GET_UINT16(data + 0x05) >> 5) & 0x0003) == 0x2)
				fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
					"Out of range value 0x%4.4" PRIx16 " "
					"bits 5..6 set to illegal value 0x2, only allowed"
					"0x0, 0x1, 0x3 while accessing entry '%s' @ "
					"0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
					GET_UINT16(data + 0x05),
					table, addr, "Cache Location", 0x5);

			dmi_reserved_bits_check(fw, table, addr, "Cache Location", hdr, sizeof(uint16_t), 0x05, 10, 15);
			dmi_reserved_bits_check(fw, table, addr, "Supported SRAM Type", hdr, sizeof(uint16_t), 0x0b, 7, 15);
			dmi_reserved_bits_check(fw, table, addr, "Current SRAM Type", hdr, sizeof(uint16_t), 0x0d, 7, 15);
			if (hdr->length < 0x13)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Error Correction Type", hdr, 0x10, 0x1, 0x6);
			dmi_min_max_uint8_check(fw, table, addr, "System Cache Type", hdr, 0x11, 0x1, 0x5);
			dmi_min_max_uint8_check(fw, table, addr, "Associativity", hdr, 0x12, 0x1, 0xe);
			break;

		case 8: /* 7.9 */
			table = "Port Connector Information (Type 8)";
			if (hdr->length < 0x09)
				break;
			dmi_str_check(fw, table, addr, "Internal Reference Designator", hdr, 0x4);
			fwts_dmi_value_range t8_ranges1[] = {{0, 0x23}, {0xa0, 0xa4}, {0xff, 0xff}};
			dmi_ranges_uint8_check(fw, table, addr, "Internal Connector Type", hdr, 0x5, t8_ranges1);
			dmi_str_check(fw, table, addr, "External Reference Designator", hdr, 0x6);
			dmi_ranges_uint8_check(fw, table, addr, "External Connector Type", hdr, 0x7, t8_ranges1);
			fwts_dmi_value_range t8_ranges2[] = {{0, 0x23}, {0xa0, 0xa1}, {0xff, 0xff}};
			dmi_ranges_uint8_check(fw, table, addr, "Port Type", hdr, 0x8, t8_ranges2);
			break;

		case 9: /* 7.10 */
			table = "System Slot Information (Type 9)";
			if (hdr->length < 0x0c)
				break;
			dmi_str_check(fw, table, addr, "Slot Designation", hdr, 0x4);
			fwts_dmi_value_range t9_ranges[] = {{1, 0x28}, {0x30, 0x30}, {0xa0, 0xc6}};
			dmi_ranges_uint8_check(fw, table, addr, "Slot Type", hdr, 0x5, t9_ranges);
			dmi_min_max_uint8_check(fw, table, addr, "Slot Data Bus Width", hdr, 0x6, 0x1, 0xe);
			dmi_min_max_uint8_check(fw, table, addr, "Current Usage", hdr, 0x7, 0x1, 0x5);
			dmi_min_max_uint8_check(fw, table, addr, "Slot Length", hdr, 0x8, 0x1, 0x6);
			if (hdr->length < 0x0d)
				break;

			if (hdr->length < 0x11)
				break;
			if (!((data[0x5] == 0x06) ||
			      ((data[0x5] >= 0x0e) && (data[0x5] <= 0x28)) ||
			      ((data[0x5] >= 0xa0) && (data[0x5] <= 0xc6)))) {
				if (GET_UINT16(data + 0xd) != 0xffff)
					fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_VALUE_OUT_OF_RANGE,
						"Invalid value 0x%4.4" PRIx16 " was used and 0xffff "
						"should be used for non-PCI(e) while accessing entry '%s' @ "
						"0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
						GET_UINT16(data + 0x0d), table, addr, "Segment Group Number", 0xd);
				if (data[0xf] != 0xff)
					fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_VALUE_OUT_OF_RANGE,
						"Invalid value 0x%2.2" PRIx8 " was used and 0xff "
						"should be used for non-PCI(e) while accessing entry '%s' @ "
						"0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
						data[0xf], table, addr, "Bus Number", 0xf);
				if (data[0x10] != 0xff)
					fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_VALUE_OUT_OF_RANGE,
						"Invalid value 0x%2.2" PRIx8 " was used and 0xff "
						"should be used for non-PCI(e) while accessing entry '%s' @ "
						"0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
						data[0x10], table, addr, "Device/Function Number", 0x10);
			}

			if (hdr->length < (0x18 + 5 * data[0x12]))
				break;

			dmi_min_max_uint8_check(fw, table, addr, "Slot Height", hdr, (0x17 + 5 * data[0x12]), 0, 0x4);

			break;

		case 10: /* 7.11 (Type 10 is obsolete) */
			table = "On Board Devices (Type 10)";
			count = (hdr->length - 4) / 2;
			for (i = 0; i < count; i++) {
				snprintf(tmp, sizeof(tmp), "Type (Device #%d)", i);
				dmi_min_max_mask_uint8_check(fw, table, addr, tmp, hdr, 4 + (2 * i), 0x1, 0xa, 0x0, 0x7f);
				snprintf(tmp, sizeof(tmp), "Description (Device #%d)", i);
				dmi_str_check(fw, table, addr, tmp, hdr, 5 + (2 * i));
			}
			break;

		case 11: /* 7.12 */
			table = "OEM Strings (Type 11)";
			if (hdr->length < 0x5)
				break;
			for (i = 1; i <= hdr->data[4]; i++) {
				snprintf(tmp, sizeof(tmp), "String %d", i);
				dmi_str_check_index(fw, table, addr, tmp, hdr, 0x4, i);
			}
			break;

		case 12: /* 7.13 */
			table = "System Configuration Options (Type 12)";
			if (hdr->length < 0x5)
				break;
			for (i = 1; i <= hdr->data[4]; i++) {
				snprintf(tmp, sizeof(tmp), "Option %d", i);
				dmi_str_check_index(fw, table, addr, tmp, hdr, 0x4, i);
			}
			break;

		case 13: /* 7.14 */
			table = "BIOS Language Information (Type 13)";
			if (hdr->length < 0x6)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Flags", hdr, sizeof(uint8_t), 0x5, 1, 7);
			if (hdr->length < 0x15)
				break;
			for (i = 0x6; i < 15; i++)
				dmi_reserved_uint8_check(fw, table, addr, "Reserved", hdr, i);
			if (hdr->length < 0x16)
				break;
			for (i = 1; i <= hdr->data[4]; i++) {
				snprintf(tmp, sizeof(tmp), "BIOS Language String %d", i);
				dmi_str_check_index(fw, table, addr, tmp, hdr, 0x4, i);
			}
			dmi_str_check(fw, table, addr, "Currently Installed Language", hdr, 0x15);
			break;

		case 14: /* 7.15 */
			table = "Group Associations (Type 14)";
			if (hdr->length < 0x05)
				break;
			dmi_str_check(fw, table, addr, "Name", hdr, 0x4);
			break;

		case 15: /* 7.16 */
			table = "System Event Log (Type 15)";
			if (hdr->length < 0x14)
				break;
			fwts_dmi_value_range t15_ranges1[] = {{0, 0x4}, {0x80, 0xff}};
			dmi_ranges_uint8_check(fw, table, addr, "Access Method", hdr, 0xa, t15_ranges1);
			dmi_reserved_bits_check(fw, table, addr, "Log Status", hdr, sizeof(uint8_t), 0xb, 2, 7);
			if (hdr->length < 0x17)
				break;
			fwts_dmi_value_range t15_ranges2[] = {{0, 0x1}, {0x80, 0xff}};
			dmi_ranges_uint8_check(fw, table, addr, "Log Header Format", hdr, 0x14, t15_ranges2);
			if (hdr->length < 0x17 + data[0x15] * data[0x16])
				break;
			if (data[0x16] >= 0x02) {
				uint8_t *tmpptr = data + 0x17;
				int k;
				for (k = 0; k < data[0x15]; k++) {
					int j = data[0x16] * k;
					val = tmpptr[j];
					if (!(((val >= 0x01) && (val <= 0x0e)) ||
					      ((val >= 0x10) && (val <= 0x17)) ||
					      (val >= 0x80))) {
						fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
							"Out of range value 0x%2.2" PRIx8 " "
							"(range allowed 0x01..0x0e, 0x10..0x17, "
							"0x80..0xff) while accessing entry '%s' @ "
							"0x%8.8" PRIx32 ", field '%s', item %d",
							val, table, addr, "Log Descriptor Type", k);
					}
					val = tmpptr[j + 1];
					if ((val > 0x06) && (val < 0x80)) {
						fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
							"Out of range value 0x%2.2" PRIx8 " "
							"(range allowed 0x00..0x06, 0x80..0xff) "
							"while accessing entry '%s' @ "
							"0x%8.8" PRIx32 ", field '%s', item %d",
							val, table, addr, "Log Descriptor Format", k);
					}
				}
			}
			break;

		case 16: /* 7.17 */
			table = "Physical Memory Array (Type 16)";
			if (hdr->length < 0x0f)
				break;
			fwts_dmi_value_range t16_ranges[] = {{0x1, 0xa}, {0xa0, 0xa4}};
			dmi_ranges_uint8_check(fw, table, addr, "Location", hdr, 0x4, t16_ranges);
			dmi_min_max_uint8_check(fw, table, addr, "Use", hdr, 0x5, 0x1, 0x7);
			dmi_min_max_uint8_check(fw, table, addr, "Error Correction Type", hdr, 0x6, 0x1, 0x7);
			dmi_min_max_uint32_check(fw, table, addr, "Maximum Capacity", hdr, 0x7, 0, 0x80000000);
			if (hdr->length < 0x17)
				break;
			if (GET_UINT64(data + 0xf) != 0 && GET_UINT32(data + 0x7) != 0x80000000)
				fwts_failed(fw, LOG_LEVEL_HIGH, DMI_INVALID_MEMORY_ENTRY,
					"An illegal value 0x%16.16" PRIx64 " for entry '%s' "
					"@ 0x%8.8" PRIx32, GET_UINT64(data + 0xf), table, addr);
			break;

		case 17: /* 7.18 */
			table = "Memory Device (Type 17)";

			/* skip if memory module is not installed (size = 0) */
			if (GET_UINT16(data + 0xc) == 0)
				break;
			if (hdr->length < 0x15)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Form Factor", hdr, 0xe, 0x1, 0x11);
			dmi_str_check(fw, table, addr, "Locator", hdr, 0x10);
			dmi_str_check(fw, table, addr, "Bank Locator", hdr, 0x11);
			fwts_dmi_value_range t17_ranges[] = {{0x1, 0x14}, {0x18, 0x24}};
			dmi_ranges_uint8_check(fw, table, addr, "Memory Type", hdr, 0x12, t17_ranges);
			dmi_reserved_bits_check(fw, table, addr, "Type Detail", hdr, sizeof(uint16_t), 0x13, 0, 0);
			if (hdr->length < 0x1b)
				break;
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0x17);
			dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x18);
			dmi_str_check(fw, table, addr, "Asset Tag", hdr, 0x19);
			dmi_str_check(fw, table, addr, "Part Number", hdr, 0x1a);
			if (hdr->length < 0x1c)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Attributes", hdr, sizeof(uint8_t), 0x1b, 4, 7);
			if (hdr->length < 0x22)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Extended Size", hdr, sizeof(uint32_t), 0x1c, 31, 31);
			if (hdr->length < 0x4c)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Memory Technology", hdr, 0x28, 0x1, 0x8);
			dmi_reserved_bits_check(fw, table, addr, "Memory Operating Mode Cap", hdr, sizeof(uint16_t), 0x29, 0, 0);
			dmi_reserved_bits_check(fw, table, addr, "Memory Operating Mode Cap", hdr, sizeof(uint16_t), 0x29, 6, 15);
			switch(hdr->data[0x28]) {
			case 0x04:
			case 0x05:
			case 0x06:
				dmi_str_check(fw, table, addr, "Firmware Version", hdr, 0x2b);
				break;
			default:
				break;
			}
			if (hdr->length < 0x54)
				break;
			dmi_reserved_bits_check(fw, table, addr, "Extended Speed", hdr, sizeof(uint32_t), 0x54, 31, 31);
			dmi_reserved_bits_check(fw, table, addr, "Extended Configured Memory Speed", hdr, sizeof(uint32_t), 0x58, 31, 31);

			break;

		case 18: /* 7.19 */
			table = "32-bit Memory Error Information (Type 18)";
			if (hdr->length < 0x17)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Error Type", hdr, 0x4, 0x1, 0xe);
			dmi_min_max_uint8_check(fw, table, addr, "Error Granularity", hdr, 0x5, 0x1, 0x4);
			dmi_min_max_uint8_check(fw, table, addr, "Error Operation", hdr, 0x6, 0x1, 0x5);
			break;

		case 19: /* 7.20 */
			table = "Memory Array Mapped Address (Type 19)";
			if (hdr->length < 0x0F)
				break;
			if (hdr->length >= 0x1F && GET_UINT32(data + 0x04) == 0xFFFFFFFF) {
				uint64_t start, end;
				start = GET_UINT64(data + 0x0F);
				end = GET_UINT64(data + 0x17);
				if (start == end)
					fwts_failed(fw, LOG_LEVEL_HIGH, DMI_ILLEGAL_MAPPED_ADDR_RANGE,
						"Extended Start and End addresses are identical "
						"while accessing entry '%s' @ 0x%8.8" PRIx32 ", "
						"fields 'Extended Starting Address' and 'Extended Ending Address'",
						table, addr);
			} else {
				if (GET_UINT32(data + 0x08) - GET_UINT32(data + 0x04) + 1 == 0)
					fwts_failed(fw, LOG_LEVEL_HIGH, DMI_ILLEGAL_MAPPED_ADDR_RANGE,
						"Illegal zero mapped address range "
						"for entry '%s' @ 0x%8.8" PRIx32, table, addr);
			}
			break;

		case 20: /* 7.21 */
			table = "Memory Device Mapped Address (Type 20)";
			if (hdr->length < 0x13)
				break;
			if (hdr->length >= 0x23 && GET_UINT32(data + 0x04) == 0xFFFFFFFF) {
				uint64_t start, end;
				start = GET_UINT64(data + 0x13);
				end = GET_UINT64(data + 0x1B);
				if (start == end)
					fwts_failed(fw, LOG_LEVEL_HIGH, DMI_ILLEGAL_MAPPED_ADDR_RANGE,
						"Extended Start and End addresses are identical "
						"while accessing entry '%s' @ 0x%8.8" PRIx32 ", "
						"fields 'Extended Starting Address' and 'Extended Ending Address'",
						table, addr);
			} else {
				if (GET_UINT32(data + 0x08) - GET_UINT32(data + 0x04) + 1 == 0)
					fwts_failed(fw, LOG_LEVEL_HIGH, DMI_ILLEGAL_MAPPED_ADDR_RANGE,
						"Illegal zero mapped address range "
						"for entry '%s' @ 0x%8.8" PRIx32, table, addr);
			}
			if (data[0x10] == 0)
				fwts_failed(fw, LOG_LEVEL_HIGH, DMI_ILLEGAL_MAPPED_ADDR_RANGE,
					"Illegal row position %2.2" PRIx8 ", "
					"while accessing entry '%s' @ 0x%8.8" PRIx32
					", field '%s', offset 0x%2.2x",
					data[0x10], table, addr, "Partial Row Position", 0x10);
			break;

		case 21: /* 7.22 */
			table = "Built-in Pointing Device (Type 21)";
			if (hdr->length < 0x07)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Type", hdr, 0x4, 0x1, 0x9);
			fwts_dmi_value_range t21_ranges[] = {{0x1, 0x8}, {0xa0, 0xa4}};
			dmi_ranges_uint8_check(fw, table, addr, "Interface", hdr, 0x5, t21_ranges);
			break;

		case 22: /* 7.23 */
			table = "Portable Battery (Type 22)";
			if (hdr->length < 0x10)
				break;
			dmi_str_check(fw, table, addr, "Location", hdr, 0x4);
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0x5);
			if (data[0x06] || hdr->length < 0x1A)
				dmi_str_check(fw, table, addr, "Manufacturer Date", hdr, 0x6);
			if (data[0x07] || hdr->length < 0x1A)
				dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x7);
			dmi_str_check(fw, table, addr, "Device Name", hdr, 0x8);
			dmi_min_max_uint8_check(fw, table, addr, "Device Chemistry", hdr, 0x9, 1, 8);

			dmi_str_check(fw, table, addr, "SBDS Version Number", hdr, 0xe);
			if (data[0xf] != 0xff)
				dmi_min_max_uint8_check(fw, table, addr, "Maximum Error in Battery Data", hdr, 0xf, 0, 100);

			if (hdr->length < 0x1A)
				break;
			if (data[0x09] == 0x02)
				dmi_str_check(fw, table, addr, "SBDS Device Chemistry", hdr, 0x14);

			ret = fwts_battery_get_count(fw, &battery_count);
			if (ret != FWTS_OK || battery_count < 1) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_INVALID_HARDWARE_ENTRY,
					"Invalid Hardware Configuration "
					"(no battery found) ");
			}
			break;
		case 23: /* 7.24 */
			table = "System Reset (Type 23)";
			if (hdr->length < 0x0D)
				break;
			if (!(data[0x04] & (1 << 5)))
				break;
			dmi_min_max_mask_uint8_check(fw, table, addr, "Capabilities (bits 1..2)", hdr, 0x4, 0x1, 0x3, 1, 0x3);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Capabilities (bits 3..4)", hdr, 0x4, 0x1, 0x3, 3, 0x3);
			dmi_reserved_bits_check(fw, table, addr, "Capabilities", hdr, sizeof(uint8_t), 0x4, 6, 7);
			break;

		case 24: /* 7.25 */
			table = "Hardware Security (Type 24)";
			/* if (hdr->length < 0x05)
				break; */
			break;

		case 25: /* 7.26 */
			table = "System Power Controls (Type 25)";
			/* if (hdr->length < 0x9)
				break; */
			break;

		case 26: /* 7.27 */
			table = "Voltage Probe (Type 26)";
			if (hdr->length < 0x16)
				break;
			dmi_str_check(fw, table, addr, "Description", hdr, 0x4);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Location (bits 0..4)", hdr, 0x5, 0x1, 0xb, 0, 0x1f);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Status (bits 5..7)", hdr, 0x5, 0x1, 0x6, 5, 0x7);
			dmi_min_max_uint16_check(fw, table, addr, "Maximum Value", hdr, 0x6, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Minimum Value", hdr, 0x8, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Resolution", hdr, 0xa, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Tolerance", hdr, 0xc, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Accuracy", hdr, 0xe, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Nominal Value", hdr, 0x14, 0, 0x8000);

			break;

		case 27: /* 7.28 */
			table = "Cooling Device (Type 27)";
			if (hdr->length < 0xe)
				break;
			val = data[0x06] & 0x1f;
			if (!(((val >= 0x01) && (val <= 0x09)) ||
			      ((val >= 0x10) && (val <= 0x11))))
				fwts_failed(fw, LOG_LEVEL_HIGH, DMI_VALUE_OUT_OF_RANGE,
					"Out of range value 0x%2.2" PRIx8 " "
					"(range allowed 0x01..0x09, 0x10..0x11) "
					"while accessing entry '%s' @ "
					"0x%8.8" PRIx32 ", field '%s', "
					"offset 0x%2.2x, mask 0x%2.2x",
					data[0x6], table, addr, "Device Type", 0x6, 0x1f);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Status (bits 5..7)", hdr, 0x6, 0x1, 0x6, 5, 0x7);
			if (hdr->length < 0x0f)
				break;
			dmi_min_max_uint16_check(fw, table, addr, "Nominal Speed", hdr, 0xc, 0, 0x8000);
			dmi_str_check(fw, table, addr, "Description", hdr, 0xe);
			break;

		case 28: /* 7.29 */
			table = "Temperature Probe (Type 28)";
			if (hdr->length < 0x16)
				break;
			dmi_str_check(fw, table, addr, "Description", hdr, 0x4);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Location (bits 0..4)", hdr, 0x5, 0x1, 0xf, 0, 0x1f);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Status (bits 5..7)", hdr, 0x5, 0x1, 0x6, 5, 0x7);
			dmi_min_max_uint16_check(fw, table, addr, "Maximum Value", hdr, 0x6, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Minimum Value", hdr, 0x8, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Resolution", hdr, 0xa, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Tolerance", hdr, 0xc, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Accuracy", hdr, 0xe, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Nominal Value", hdr, 0x14, 0, 0x8000);
			break;

		case 29: /* 7.30 */
			table = "Electrical Current Probe (Type 29)";
			if (hdr->length < 0x16)
				break;
			dmi_str_check(fw, table, addr, "Description", hdr, 0x4);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Location (bits 0..4)", hdr, 0x5, 0x1, 0xb, 0, 0x1f);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Status (bits 5..7)", hdr, 0x5, 0x1, 0x6, 5, 0x7);
			dmi_min_max_uint16_check(fw, table, addr, "Maximum Value", hdr, 0x6, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Minimum Value", hdr, 0x8, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Resolution", hdr, 0xa, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Tolerance", hdr, 0xc, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Accuracy", hdr, 0xe, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Nominal Value", hdr, 0x14, 0, 0x8000);
			break;

		case 30: /* 7.31 */
			table = "Out-of-band Remote Access (Type 30)";
			if (hdr->length < 0x06)
				break;
			dmi_str_check(fw, table, addr, "Manufacturer Name", hdr, 0x4);
			dmi_reserved_bits_check(fw, table, addr, "Connections", hdr, sizeof(uint8_t), 0x5, 2, 7);
			break;

		case 31: /* 7.32 */
			table = "Boot Integrity Services Entry Point (Type 31)";
			/*
			if (hdr->length < 0x1c)
				break;
			*/
			break;

		case 32: /* 7.33 */
			table = "System Boot Information (Type 32)";
			if (hdr->length < 0xb)
				break;
			for (i = 4; i <= 9; i++)
				dmi_reserved_uint8_check(fw, table, addr, "Reserved", hdr, i);
			fwts_dmi_value_range t32_ranges[] = {{0, 0x8}, {0x80, 0xff}};
			dmi_ranges_uint8_check(fw, table, addr, "Boot Status", hdr, 0xa, t32_ranges);
			break;

		case 33: /* 7.34 */
			table = "64-bit Memory Error Information (Type 33)";
			if (hdr->length < 0x1f)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Error Type", hdr, 0x4, 0x1, 0xe);
			dmi_min_max_uint8_check(fw, table, addr, "Error Granularity", hdr, 0x5, 0x1, 0x4);
			dmi_min_max_uint8_check(fw, table, addr, "Error Operation", hdr, 0x6, 0x1, 0x5);
			break;

		case 34: /* 7.35 */
			table = "Management Device (Type 34)";
			if (hdr->length < 0x0b)
				break;
			dmi_str_check(fw, table, addr, "Description", hdr, 0x4);
			dmi_min_max_uint8_check(fw, table, addr, "Type", hdr, 0x5, 0x1, 0xd);
			dmi_min_max_uint8_check(fw, table, addr, "Address Type", hdr, 0xa, 0x1, 0x5);
			break;

		case 35: /* 7.36 */
			table = "Management Device Component (Type 35)";
			if (hdr->length < 0x0b)
				break;
			dmi_str_check(fw, table, addr, "Description", hdr, 0x4);
			break;

		case 36: /* 7.37 */
			table = "Management Device Threshold Data (Type 36)";
			if (hdr->length < 0x10)
				break;
			dmi_min_max_uint16_check(fw, table, addr, "Lower Threshold - Non-critical", hdr, 0x4, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Upper Threshold - Non-critical", hdr, 0x6, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Lower Threshold - Critical", hdr, 0x8, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Upper Threshold - Critical", hdr, 0xa, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Lower Threshold - Non-recoverable", hdr, 0xc, 0, 0x8000);
			dmi_min_max_uint16_check(fw, table, addr, "Upper Threshold - Non-recoverable", hdr, 0xe, 0, 0x8000);
			break;

		case 37: /* 7.38 */
			table = "Memory Channel (Type 37)";
			if (hdr->length < 0x07)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Type", hdr, 0x4, 0x1, 0x4);
			break;

		case 38: /* 7.39 */
			table = "IPMI Device Information (Type 38)";
			if (hdr->length < 0x12)
				break;
			dmi_min_max_uint8_check(fw, table, addr, "Interface Type", hdr, 0x4, 0x0, 0x4);

			dmi_reserved_bits_check(fw, table, addr, "Base Addr Modifier/Interrupt Info", hdr, sizeof(uint8_t), 0x10, 2, 2);
			dmi_reserved_bits_check(fw, table, addr, "Base Addr Modifier/Interrupt Info", hdr, sizeof(uint8_t), 0x10, 5, 5);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Base Addr Modifier/Interrupt Info)", hdr, 0x10, 0x0, 0x2, 6, 0x3);
			break;

		case 39: /* 7.40 */
			table = "System Power Supply (Type 39)";
			if (hdr->length < 0x10)
				break;
			dmi_str_check(fw, table, addr, "Location", hdr, 0x5);
			dmi_str_check(fw, table, addr, "Device Name", hdr, 0x6);
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0x7);
			dmi_str_check(fw, table, addr, "Serial Number", hdr, 0x8);
			dmi_str_check(fw, table, addr, "Asset Tag", hdr, 0x9);
			dmi_str_check(fw, table, addr, "Model Part Number", hdr, 0xa);
			dmi_str_check(fw, table, addr, "Revision Level", hdr, 0xb);
			dmi_min_max_uint16_check(fw, table, addr, "Max Power Capacity", hdr, 0xc, 0, 0x8000);
			dmi_reserved_bits_check(fw, table, addr, "Power Supply Characteristics", hdr, sizeof(uint16_t), 0xe, 14, 15);
			break;

		case 40: /* 7.41 */
			table = "Additional Information (Type 40)";
			break;

		case 41: /* 7.42 */
			table = "Onboard Device (Type 41)";
			if (hdr->length < 0xb)
				break;
			dmi_str_check(fw, table, addr, "Reference Designation", hdr, 0x4);
			dmi_min_max_mask_uint8_check(fw, table, addr, "Device Type", hdr, 0x5, 0x1, 0x10, 0, 0x7f);
			break;

		case 42: /* 7.43 */
			table = "Management Controller Host Interface (Type 42)";
			if (hdr->length < 0x05)
				break;
			if (!((data[0x04] <= 0x3F) ||
			      (data[0x04] == 0x40) ||
			      (data[0x04] == 0xF0)))
				fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_MGMT_CTRL_HOST_TYPE,
					"Out of range value 0x%2.2" PRIx8 " "
					"(range allowed 0x00..0x3f, 0x40, 0xf0) "
					"while accessing entry '%s' @ "
					"0x%8.8" PRIx32 ", field '%s', offset 0x%2.2x",
					data[0x4], table, addr, "Reference Designation", 0x4);
			break;

		case 43: /* 7.44 */
			table = "TPM Device (Type 43)";
			if (hdr->length < 0x1b)
				break;
			dmi_str_check(fw, table, addr, "Description", hdr, 0x12);
			dmi_reserved_bits_check(fw, table, addr, "Characteristics", hdr, sizeof(uint64_t), 0x13, 0, 1);
			dmi_reserved_bits_check(fw, table, addr, "Characteristics", hdr, sizeof(uint64_t), 0x13, 6, 63);
			break;

		case 44: /* 7.45 */
			table = "Processor Additional Information (Type 44)";
			if (hdr->length < 0x6)
				break;

			dmi_min_max_uint8_check(fw, table, addr, "IProcessor Architecture Types", hdr, 0x7, 0x0, 0xa);
			break;

		case 45: /* 7.46 */
			table = "Firmware Inventory Information (Type 45)";
			if (hdr->length < 0x18)
				break;

			dmi_str_check(fw, table, addr, "Firmware Component Name", hdr, 0x4);
			dmi_str_check(fw, table, addr, "Firmware Version", hdr, 0x5);
			dmi_str_check(fw, table, addr, "Firmware ID", hdr, 0x7);
			dmi_str_check(fw, table, addr, "Release Date", hdr, 0x9);
			dmi_str_check(fw, table, addr, "Manufacturer", hdr, 0xa);
			dmi_str_check(fw, table, addr, "Lowest Supported Firmware Version", hdr, 0xb);
			dmi_reserved_bits_check(fw, table, addr, "Characteristics", hdr, sizeof(uint16_t), 0x14, 2, 15);
			dmi_min_max_uint8_check(fw, table, addr, "State", hdr, 0x16, 0x1, 0x8);
			break;

		case 46: /* 7.47 */
			table = "String Property (Type 46)";
			if (hdr->length < 0x7)
				break;
			break;

		case 126: /* 7.48 */
			table = "Inactive (Type 126)";
			break;
		case SMBIOS_END_OF_TABLE: /* 7.49 */
			table = "End of Table (Type 127)";
			break;
		default:
			snprintf(tmp, sizeof(tmp), "Unknown (Type %" PRId8 ")", hdr->type);
			table = tmp;
			break;
	}
	if (fw->minor_tests.failed == failed_count)
		fwts_passed(fw, "Entry @ 0x%8.8" PRIx32 " '%s'", addr, table);
	else if (hdr->type <= 42)
		fwts_advice(fw,
			"It may be worth checking against section 7.%" PRId8 " of the "
			"System Management BIOS (SMBIOS) Reference Specification "
			"(see http://www.dmtf.org/standards/smbios).", hdr->type+1);
}

static int dmi_version_check(fwts_framework *fw, uint16_t version)
{
	if (version > DMI_VERSION) {
		fwts_skipped(fw,
			"SMBIOS version %" PRIu16 ".%" PRIu16
			" is not supported by the dmicheck "
			"test. This test only supports SMBIOS version "
			"%" PRIu16 ".%" PRIu16 " and lower.",
			VERSION_MAJOR(version), VERSION_MINOR(version),
			VERSION_MAJOR(DMI_VERSION), VERSION_MINOR(DMI_VERSION));
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static void dmi_scan_tables(fwts_framework *fw,
	fwts_smbios_entry *entry,
	uint8_t  *table)
{
	const uint16_t smbios_version = (entry->major_version << 8) +
					entry->minor_version;
	uint8_t *entry_data = table;
	uint16_t table_length;
	uint16_t struct_count;
	int i = 0;

	table_length = entry->struct_table_length;
	struct_count = entry->number_smbios_structures;

	while ((i < struct_count) &&
	       (entry_data <= (table + table_length - 4))) {
		uint32_t addr = entry->struct_table_address + (entry_data - table);
		fwts_dmi_header hdr;
		uint8_t *next_entry;

		hdr.type   = entry_data[0];
		hdr.length = entry_data[1];
		hdr.handle = GET_UINT16(entry_data + 2);
		hdr.data   = entry_data;

		/* Sanity check */
		if (hdr.length < 4) {
			fwts_failed(fw, LOG_LEVEL_HIGH, DMI_INVALID_ENTRY_LENGTH,
				"Invalid header length of entry #%d, "
				"length was 0x%2.2" PRIx8 ".",
				i, hdr.length);
			fwts_advice(fw,
				"DMI entry header lengths must be 4 or more bytes long "
				"so this error indicates that the DMI table is unreliable "
				"and DMI table checking has been aborted at entry #%d.", i);
			break;
		}

		/* Real Physical Address */
		next_entry = entry_data + hdr.length;

		/* Look for structure terminator, ends in two zero bytes */
		while (((next_entry - table + 1) < table_length) &&
		       ((next_entry[0] != 0) || (next_entry[1] != 0))) {
			next_entry++;
		}

		/* Skip over terminating two zero bytes, see section 6.1 of spec */
		next_entry += 2;

		if ((next_entry - table) <= table_length)
			dmicheck_entry(fw, addr, &hdr, smbios_version);

		i++;
		entry_data = next_entry;
	}

	if (table_length != (entry_data - table))
		fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_BAD_TABLE_LENGTH,
			"DMI table length was %" PRId16 " bytes (as specified by "
			"the SMBIOS header) but the DMI entries used %td bytes.",
			table_length, entry_data - table);

	if (struct_count != i)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, DMI_STRUCT_COUNT,
			"DMI table was DMI %d entries in size (as specified by "
			"the SMBIOS header) but only %d DMI entries were found.",
				struct_count, i);
}

/*
 * ARM SBBR SMBIOS Structure Test
 */

#define RECOMMENDED_STRUCTURE_DEFAULT_MSG "This structure is recommended."

/* Test Entry Structure */
typedef struct {
	const char *name;
	const uint8_t type;
	const uint8_t mandatory;
	const char *advice;
	uint8_t found;
} sbbr_test_entry;

/* Test Definition Array */
static sbbr_test_entry sbbr_test[] = {
	{ "BIOS Information", 0, 1, 0, 0 },
	{ "System Information", 1, 1, 0, 0 },
	{ "Baseboard Information", 2, 0, RECOMMENDED_STRUCTURE_DEFAULT_MSG, 0 },
	{ "System Enclosure or Chassis", 3, 1, 0, 0 },
	{ "Processor Information", 4, 1, 0, 0 },
	{ "Cache Information", 7, 1, 0, 0 },
	{ "Port Connector Information", 8, 0, "Recommended for platforms with physical ports.", 0 },
	{ "System Slots", 9, 0, "Required for platforms with expansion slots.", 0 },
	{ "OEM Strings", 11, 0, RECOMMENDED_STRUCTURE_DEFAULT_MSG, 0 },
	{ "BIOS Language Information", 13, 0, RECOMMENDED_STRUCTURE_DEFAULT_MSG, 0 },
	{ "System Event Log", 15, 0, RECOMMENDED_STRUCTURE_DEFAULT_MSG, 0 },
	{ "Physical Memory Array", 16, 1, 0, 0 },
	{ "Memory Device", 17, 1, 0, 0 },
	{ "Memory Array Mapped Address", 19, 1, 0, 0 },
	{ "System Boot Information", 32, 1, 0, 0 },
	{ "IPMI Device Information", 38, 0, "Required for platforms with IPMI BMC Interface.", 0 },
	{ "Onboard Devices Extended Information", 41, 0, RECOMMENDED_STRUCTURE_DEFAULT_MSG, 0 },
	{ "Redfish Host Interface", 42, 0, "Required for platforms supporting Redfish Host Interface.", 0 },
	{ 0, 0, 0, 0, 0 }
};

static void sbbr_test_entry_check(fwts_dmi_header *hdr)
{
	uint32_t i;
	for (i = 0; sbbr_test[i].name != NULL; i++) {
		if (hdr->type == sbbr_test[i].type) {
			sbbr_test[i].found = 1;
			break;
		}
	}
}

static void dmi_scan_smbios30_table(fwts_framework *fw,
	fwts_smbios30_entry *entry,
	uint8_t  *table)
{
	const uint16_t smbios_version = (entry->major_version << 8) +
					entry->minor_version;
	uint8_t *entry_data = table;
	uint16_t table_max_length;
	int i = 0;

	table_max_length = entry->struct_table_max_size;

	for (i = 0; entry_data <= (table + table_max_length - 4); i++) {
		uint64_t addr = entry->struct_table_address + (entry_data - table);
		fwts_dmi_header hdr;
		uint8_t *next_entry;

		hdr.type   = entry_data[0];
		hdr.length = entry_data[1];
		hdr.handle = GET_UINT16(entry_data + 2);
		hdr.data   = entry_data;

		/* We found DMI end of table */
		if (hdr.type == SMBIOS_END_OF_TABLE)
			break;

		/* Sanity check */
		if (hdr.length < 4) {
			fwts_failed(fw, LOG_LEVEL_HIGH, DMI_INVALID_ENTRY_LENGTH,
				"Invalid header length of entry #%d, "
				"length was 0x%2.2" PRIx8 ".",
				i, hdr.length);
			fwts_advice(fw,
				"DMI entry header lengths must be 4 or more bytes long "
				"so this error indicates that the DMI table is unreliable "
				"and DMI table checking has been aborted at entry #%d.", i);
			break;
		}

		/* Real Physical Address */
		next_entry = entry_data + hdr.length;

		/* Look for structure terminator, ends in two zero bytes */
		while (((next_entry - table + 1) < table_max_length) &&
		       ((next_entry[0] != 0) || (next_entry[1] != 0))) {
			next_entry++;
		}

		/* Skip over terminating two zero bytes, see section 6.1 of spec */
		next_entry += 2;

		if ((next_entry - table) <= table_max_length) {
			dmicheck_entry(fw, addr, &hdr, smbios_version);
			if (fw->flags & FWTS_FLAG_SBBR)
				sbbr_test_entry_check(&hdr);
		}
		else {
			fwts_failed(fw, LOG_LEVEL_HIGH, DMI_BAD_TABLE_LENGTH,
				"DMI table maximum size was %" PRId32 " bytes (as specified by "
				"the SMBIOS 3.0 header) but the DMI entries over the maximum "
				"length without finding the End-of-Table(Type 127).",
				table_max_length);
			break;
		}

		entry_data = next_entry;
	}

}

static int dmicheck_test2(fwts_framework *fw)
{
	void *addr;
	fwts_smbios_entry entry;
	fwts_smbios_type  type;
	uint16_t version = 0;
	uint8_t  *table;

	if (fw->flags & FWTS_FLAG_SBBR)
		return FWTS_SKIP;

	if (!smbios_found) {
		fwts_skipped(fw, "Cannot find SMBIOS or DMI table entry, skip the test.");
		return FWTS_SKIP;
	}

	addr = fwts_smbios_find_entry(fw, &entry, &type, &version);
	if (addr == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, DMI_NO_TABLE_HEADER,
			"Cannot find SMBIOS or DMI table entry.");
		return FWTS_ERROR;
	}
	if (type == FWTS_SMBIOS_UNKNOWN) {
		fwts_failed(fw, LOG_LEVEL_HIGH, DMI_NO_TABLE,
			"Cannot find a valid SMBIOS or DMI table.");
		return FWTS_ERROR;
	}

	/* Fix broken version numbers found in the wild */
	version = dmi_remap_version(fw, version);
	if (dmi_version_check(fw, version) != FWTS_OK)
		return FWTS_SKIP;

	table = dmi_table_smbios(fw, &entry);
	if (table == NULL)
		return FWTS_ERROR;

	dmi_scan_tables(fw, &entry, table);

	dmi_table_free(table);

	return FWTS_OK;
}

static int dmicheck_test3(fwts_framework *fw)
{
	void *addr;
	fwts_smbios30_entry entry30;
	uint16_t version = 0;
	uint8_t  *table;

	if (!smbios30_found) {
		fwts_skipped(fw, "Cannot find SMBIOS30 table entry, skip the test.");
		return FWTS_SKIP;
	}

	addr = fwts_smbios30_find_entry(fw, &entry30, &version);
	if (addr == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, DMI_NO_TABLE_HEADER,
			"Cannot find SMBIOS 3.0 table entry.");
		return FWTS_ERROR;
	}

	if (dmi_version_check(fw, version) != FWTS_OK)
		return FWTS_SKIP;

	table = dmi_table_smbios30(fw, &entry30);
	if (table == NULL)
		return FWTS_ERROR;

	dmi_scan_smbios30_table(fw, &entry30, table);

	dmi_table_free(table);

	return FWTS_OK;
}

/* SBBR SMBIOS structure test function. */
static int dmicheck_test4(fwts_framework *fw)
{
	uint32_t i;

	if (!(fw->flags & FWTS_FLAG_SBBR))
		return FWTS_SKIP;

	if (!smbios30_found) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SbbrSmbiosNoStruct", "Cannot find SMBIOS30 table entry.");
		return FWTS_ERROR;
	}

	/* Check whether all SMBIOS structures needed by SBBR have been found. */
	for (i = 0; sbbr_test[i].name != NULL; i++) {
		if (!sbbr_test[i].found) {
			if (sbbr_test[i].mandatory)
				fwts_failed(fw, LOG_LEVEL_HIGH, "SbbrSmbiosNoStruct", "Cannot find SMBIOS "
						"structure: %s (Type %d).", sbbr_test[i].name, sbbr_test[i].type);
			else
				fwts_skipped(fw, "SMBIOS structure %s (Type %d) not found. %s",
						sbbr_test[i].name, sbbr_test[i].type,
						sbbr_test[i].advice?sbbr_test[i].advice:"");
		} else
			fwts_passed(fw, "SMBIOS structure %s (Type %d) found.",
					sbbr_test[i].name, sbbr_test[i].type);
	}
	return FWTS_OK;
}

static fwts_framework_minor_test dmicheck_tests[] = {
	{ dmicheck_test1, "Find and test SMBIOS Table Entry Points." },
	{ dmicheck_test2, "Test DMI/SMBIOS tables for errors." },
	{ dmicheck_test3, "Test DMI/SMBIOS3 tables for errors." },
	{ dmicheck_test4, "Test ARM SBBR SMBIOS structure requirements."},
	{ NULL, NULL }
};

static fwts_framework_ops dmicheck_ops = {
	.description = "DMI/SMBIOS table tests.",
	.minor_tests = dmicheck_tests
};

FWTS_REGISTER("dmicheck", &dmicheck_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_SBBR)

#endif
