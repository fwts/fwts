/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This code was originally part of the Linux-ready Firmware Developer Kit
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
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <asm/mtrr.h>
#include <inttypes.h>

static fwts_list *klog;
static fwts_list *mtrr_list;
static fwts_cpuinfo_x86 *fwts_cpuinfo;

#define UNCACHED	0x0001
#define	WRITE_BACK	0x0002
#define	WRITE_COMBINING	0x0004
#define WRITE_THROUGH	0x0008
#define WRITE_PROTECT	0x0010
#define DEFAULT		0x0020
#define DISABLED	0x0040
#define UNKNOWN		0x0080

#define MTRR_DEF_TYPE_MSR	0x2FF
#define AMD_SYS_CFG_MSR		0xC0010010
#define AMD_TOM2_MSR		0xC001001D

#define MTRR_ENABLE		0x800
#define FIXED_MTRR_ENABLE	0x400

static	uint64_t mtrr_default;
static	uint64_t amd_tom2_addr;
static	bool amd_Tom2ForceMemTypeWB = false;

struct mtrr_entry {
	uint8_t  reg;
	uint64_t start;
	uint64_t end;
	uint64_t size;
	uint8_t  type;
};

static char *cache_to_string(int type)
{
	static char str[1024];

	memset(str, 0, 1024);

	if (type & UNCACHED || type == 0)
		strcat(str," Uncached");
	if (type & WRITE_BACK)
		strcat(str," Write-Back");
	if (type & WRITE_COMBINING)
		strcat(str," Write-Combining");
	if (type & WRITE_THROUGH)
		strcat(str," Write-Through");
	if (type & WRITE_PROTECT)
		strcat(str," Write-Protect");
	if (type & DEFAULT) {
		strcat(str," Default");
		if (mtrr_default & UNCACHED)
			strcat(str," (Uncached)");
		if (mtrr_default & WRITE_BACK)
			strcat(str," (Write-Back)");
		if (mtrr_default & WRITE_COMBINING)
			strcat(str," (Write-Combining)");
		if (mtrr_default & WRITE_THROUGH)
			strcat(str," (Write-Through)");
		if (mtrr_default & WRITE_PROTECT)
			strcat(str," (Write-Protect)");
		if (mtrr_default & UNKNOWN)
			strcat(str," (Unknown)");
	}
	if (type & UNKNOWN)
		strcat(str," Unknown");
	return str;
}

static int get_mtrrs(void)
{
	struct mtrr_entry *entry;
	FILE *fp;
	char line[4096];

	if ((mtrr_list = fwts_list_new()) == NULL)
		return FWTS_ERROR;

	if ((fp = fopen("/proc/mtrr", "r")) == NULL) {
		fwts_list_free(mtrr_list, free);
		return FWTS_ERROR;
	}

	while (!feof(fp)) {
		char *ptr1, *ptr2;

		if (fgets(line, sizeof(line), fp) == NULL)
			break;

		if ((entry = calloc(1, sizeof(struct mtrr_entry))) == NULL) {
			fwts_list_free(mtrr_list, free);
			(void)fclose(fp);
			return FWTS_ERROR;
		}

		/*
		 * Put all text to lower case since the output
		 * from /proc/mtrr is variable upper/lower case
		 * across kernel versions so forcing to lower
		 * saves comparing for upper/lower case variants.
		 */
		for (ptr1 = line; *ptr1; ptr1++)
			*ptr1 = tolower(*ptr1);

		/* Parse the following:
		 *   reg01: base=0x080000000 ( 2048MB), size= 1024MB, count=1: write-back
		 */

		/* Get register, in decimal  */
		if (strncmp(line, "reg", 3)) {
			free(entry);
			continue;
		}
		entry->reg = strtoul(line + 3, NULL, 10);

		/* Get base, in hex */
		if ((ptr1 = strstr(line, "base=0x")) == NULL) {
			free(entry);
			continue;
		}
		entry->start = strtoull(ptr1 + 5, NULL, 16);

		/* Get size, in decimal */
		if ((ptr1 = strstr(line, "size=")) == NULL) {
			free(entry);
			continue;
		}

		entry->size = strtoull(ptr1 + 5, &ptr2, 10);
		if (ptr2 && (*ptr2 == 'm'))
			entry->size *= 1024 * 1024;
		if (ptr2 && (*ptr2 == 'k'))
			entry->size *= 1024;

		entry->end = entry->start + entry->size - 1;

		if (strstr(line, "write-back"))
			entry->type = WRITE_BACK;
		else if (strstr(line, "uncachable"))
			entry->type = UNCACHED;
		else if (strstr(line, "write-through"))
			entry->type = WRITE_THROUGH;
		else if (strstr(line, "write-combining"))
			entry->type = WRITE_COMBINING;
		else if (strstr(line, "write-protect"))
			entry->type = WRITE_PROTECT;
		else entry->type = UNKNOWN;

		fwts_list_append(mtrr_list, entry);
	}
	(void)fclose(fp);

	return FWTS_OK;
}

static int get_default_mtrr(fwts_framework *fw)
{
	uint64_t amd_sys_conf;

	/* Get the default memory type of memory between 4GB and second top of
	 * memory (TOM2) - i.e. is it write back (WB)
	 */
	if (strstr(fwts_cpuinfo->vendor_id, "AMD") || strstr(fwts_cpuinfo->vendor_id, "Hygon")) {
		if (fwts_cpu_readmsr(fw, 0, AMD_SYS_CFG_MSR, &amd_sys_conf) == FWTS_OK)
			if (amd_sys_conf & 0x200000)
				amd_Tom2ForceMemTypeWB = true;

		if (fwts_cpu_readmsr(fw, 0, AMD_TOM2_MSR, &amd_tom2_addr) != FWTS_OK)
			amd_tom2_addr = 0x100000000;	/* this should above 4GB */
	}

	if (fwts_cpu_readmsr(fw, 0, MTRR_DEF_TYPE_MSR, &mtrr_default) == FWTS_OK) {
		switch (mtrr_default & 0xFF) {
			case 0:
				mtrr_default = UNCACHED;
				break;
			case 1:
				mtrr_default = WRITE_COMBINING;
				break;
			case 4:
				mtrr_default = WRITE_THROUGH;
				break;
			case 5:
				mtrr_default = WRITE_PROTECT;
				break;
			case 6:
				mtrr_default = WRITE_BACK;
				break;
			default:
				mtrr_default = UNKNOWN;
				break;
		}
		return FWTS_OK;
	}
	return FWTS_ERROR;
}

static int cache_types(const uint64_t start, uint64_t end)
{
	fwts_list_link *item;
	struct mtrr_entry *entry;
	int type = 0;

	fwts_list_foreach(item, mtrr_list) {
		entry = fwts_list_data(struct mtrr_entry*, item);

		if (entry->end >= start && entry->start <= end)
			type |= entry->type;
	}

	/* On AMD platforms, Tom2ForceMemTypeWB overwrites MTRRdefType */
	if (type == 0 && amd_Tom2ForceMemTypeWB && start >= 0x100000000 && start <= amd_tom2_addr)
			return WRITE_BACK;

	/*
	 * now to see if there is any part of the range that isn't
	 * covered by an mtrr, since it's UNCACHED if so
	 */
restart:
	fwts_list_foreach(item, mtrr_list) {
		entry = fwts_list_data(struct mtrr_entry*, item);

		if (entry->end >= end && entry->start < end) {
			end = (entry->start == 0) ? 0 : (entry->start - 1);
			if (end < start)
				end = start;
			else
				goto restart;
		}
	}

	/* if there is no full coverage it's also uncached */
	if (start != end)
		type |= DEFAULT;

	if ((type & DEFAULT) && mtrr_default != UNCACHED) {
		type &= ~DEFAULT;
		type |= mtrr_default;
	}

	return type;
}

static int check_prefetchable(
	fwts_framework *fw,
	const char *device,
	const uint64_t address,
	bool *pref)
{
	char path[PATH_MAX];
	uint8_t config[64];
	int fd, i, n, bars;
	uint32_t *bar;

	*pref = false;
	snprintf(path, sizeof(path), FWTS_PCI_DEV_PATH "/%s/config", device);
	if ((fd = open(path, O_RDONLY)) < 0) {
		fwts_log_error(fw, "Cannot read PCI config for device %s\n", device);
		return FWTS_ERROR;
	}

	n = read(fd, config, sizeof(config));
	(void)close(fd);

	/* config space too small? ignore for now */
	if (n < 64)
		return FWTS_OK;

	/* Type, mask off top bit so we can cater for multi-function devices */
	switch (config[FWTS_PCI_CONFIG_HEADER_TYPE] & 0x7f) {
	case FWTS_PCI_CONFIG_HEADER_TYPE_NON_BRIDGE:
		bars = 6;
		break;
	case FWTS_PCI_CONFIG_HEADER_TYPE_PCI_BRIDGE:
		bars = 2;
		break;
	default:
		/* No BARs, ignore */
		return FWTS_OK;
	}

	/*
	 *  Check BAR addresses, do they match and are they prefetchable
	 *  See http://wiki.osdev.org/PCI
	 */
	bar = (uint32_t*)&config[0x10];
	for (i = 0; i < bars; i++) {
		if ((bar[i] & 1) == 0) {
			uint64_t bar_addr;
			bool is_prefetchable =
				(bar[i] & FWTS_PCI_CONFIG_BAR_PREFETCHABLE);

			switch ((bar[i] >> 1) & 0x03) {
			case FWTS_PCI_CONFIG_BAR_TYPE_ADDR_16BIT:
				bar_addr = bar[i] & 0xfff0;
				break;
			case FWTS_PCI_CONFIG_BAR_TYPE_ADDR_32BIT:
				bar_addr = bar[i] & 0xfffffff0;
				break;
			case FWTS_PCI_CONFIG_BAR_TYPE_ADDR_64BIT:
				bar_addr = (bar[i] & 0xfffffff0) |
					   (((uint64_t)bar[i+1]) << 32);
				i++;
				break;
			default:
				continue;
			}

			if (bar_addr == address) {
				*pref = is_prefetchable;
				break;
			}
		}
	}

	return FWTS_OK;
}

static int guess_cache_type(
	fwts_framework *fw,
	const char *string,
	int *must,
	int *mustnot,
	const uint64_t address)
{
	*must = 0;
	*mustnot = 0;

	if (strstr(string, "System RAM")) {
		*must = WRITE_BACK;
		*mustnot = ~WRITE_BACK;
		return FWTS_OK;
	}
	/* if it's PCI mmio -> uncached typically except for video */
	if (strstr(string, "0000:")) {
		bool pref;

		if (check_prefetchable(fw, string, address, &pref) != FWTS_OK)
			return FWTS_ERROR;

		if (pref) {
			*must = 0;
			*mustnot = WRITE_BACK;
		} else {
			*must = UNCACHED;
			*mustnot = (~UNCACHED) & (~DEFAULT);
		}
	}

	return FWTS_OK;
}

static void multi_types_check(fwts_framework *fw, int *type)
{
	int n_types = 0, i;

	/* checking number of types set, UNCACHED, WRITE_BACK, WRITE_COMBINING, WRITE_THROUGH, WRITE_PROTECT */
	for (i = 0; i < 5; i++) {
		if (*type & (1 << i))
			n_types++;
	}

	if (n_types >= 2) {
		if (*type & UNCACHED) {
			fwts_log_info(fw, "Detected more than one type has been set. "
					"From Intel Software Developer Manual 11.11.4.1 MTRR Precedences, "
					"If two or more variable memory ranges match and one of the memory "
					"types is UC, the UC memory type used. FWTS will check with type "
					"UNCACHED here.");
			*type = UNCACHED;
			return;
		} else if ((*type & WRITE_THROUGH) && (*type & WRITE_BACK)) {
			fwts_log_info(fw, "Detected more than one type has been set. "
					"From Intel Software Developer Manual 11.11.4.1 MTRR Precedences, "
					"If two or more variable memory ranges match and the memory types "
					"are WT and WB, the WT memory type is used. FWTS will check with type "
					"WRITE_THROUGH here.");
			*type = WRITE_THROUGH;
			return;
		} else {
			fwts_warning(fw, "Detected more than one type has been set. The overlaps not defined, "
					"processor behavior is undefined.");
			return;
		}
	}
	return;
}

static int validate_iomem(fwts_framework *fw)
{
	FILE *file;
	char buffer[4096];
	int pcidepth = 0;
	int failed = 0;

	memset(buffer, 0, 4096);

	if ((file = fopen("/proc/iomem", "r")) == NULL)
		return FWTS_ERROR;

	if (get_default_mtrr(fw) != FWTS_OK)
		mtrr_default = UNKNOWN;

	while (!feof(file)) {
		uint64_t start;
		uint64_t end;
		int type, type_must, type_mustnot;
		char *c, *c2;
		int i;
		bool skiperror = false;

		if (fgets(buffer, 4095, file) == NULL)
			break;

		fwts_chop_newline(buffer);

		/*
		 * For pci bridges, we note the increased depth and
		 * otherwise skip the entry
 		 */
		if (strstr(buffer, ": PCI Bus ")) {
			pcidepth++;
			continue;
		}

		/* then: check the pci depth */
		for (i = 0; i < pcidepth * 2; i++) {
			if (buffer[i] != ' ') {
				pcidepth = i / 2;
				break;
			}
		}
		c = &buffer[pcidepth*2];
		/* sub entry to a main entry -> skip */
		if (*c == ' ')
			continue;

		start = strtoull(c, NULL, 16);
		c2 = strchr(c, '-');
		if (!c2)
			continue;
		c2++;
		end = strtoull(c2, NULL, 16);

		c2 = strstr(c, " : ");
		if (!c2)
			continue;
		c2 += 3;

		/* exception: 640K - 1Mb range we ignore */
		if (start >= 640*1024 && end <= 1024*1024)
			continue;

		type = cache_types(start, end);

		multi_types_check(fw, &type);

		if (guess_cache_type(fw, c2, &type_must, &type_mustnot, start) != FWTS_OK) {
			/*  This has failed, give up at this point */
			fwts_skipped(fw,
				"Could not guess cache type.");
			(void)fclose(file);
			return FWTS_ERROR;
		}

		if ((type & type_mustnot) != 0) {
			failed++;
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"MTRRIncorrectAttr",
				"Memory range 0x%" PRIx64 " to 0x%" PRIx64 " (%s) "
				"has incorrect attribute%s.",
				start, end,
				c2, cache_to_string(type & type_mustnot));
			if (type_must == UNCACHED)
				skiperror = true;
		}

		if (type & DEFAULT) {
			type |= UNCACHED;
			type &= ~DEFAULT;
		}
		if (((type & type_must) != type_must) && !skiperror) {
			failed++;
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"MTRRLackingAttr",
				"Memory range 0x%" PRIx64 " to 0x%" PRIx64 " (%s) "
				"is lacking attribute%s.",
				start, end,
				c2,
				cache_to_string( (type & type_must) ^ type_must));
		}

	}
	(void)fclose(file);

	if (!failed)
		fwts_passed(fw, "Memory ranges seem to have correct attributes.");

	return FWTS_OK;
}

static void do_mtrr_resource(fwts_framework *fw)
{
	fwts_list_link *item;

	fwts_log_info_verbatim(fw,"MTRR overview");
	fwts_log_info_verbatim(fw,"-------------");

	fwts_list_foreach(item, mtrr_list) {
		struct mtrr_entry *entry = fwts_list_data(struct mtrr_entry *, item);

		if (entry->type & DISABLED)
			fwts_log_info_verbatim(fw, "Reg %hhu: disabled", entry->reg);
		else
			fwts_log_info_verbatim(fw,
				"Reg %hhu: 0x%16.16" PRIx64 " - 0x%16.16" PRIx64 " (%6" PRId64 " %cB)  %s",
				entry->reg,
				entry->start,
				entry->end,
				(entry->size >= (1024*1024) ? entry->size / (1024*1024) : (entry->size / 1024)),
				entry->size >= (1024*1024) ? 'M' : 'K', cache_to_string(entry->type));
	}
	fwts_log_nl(fw);
}

static int mtrr_init(fwts_framework *fw)
{
	if (get_mtrrs() != FWTS_OK) {
		fwts_log_error(fw, "Failed to read /proc/mtrr.");
		return FWTS_ERROR;
	}

	if (access("/proc/mtrr", R_OK))
		return FWTS_ERROR;

	if ((klog = fwts_klog_read()) == NULL) {
		fwts_log_error(fw, "Failed to read kernel log.");
		return FWTS_ERROR;
	}

	if ((fwts_cpuinfo = fwts_cpu_get_info(-1)) == NULL) {
		fwts_log_error(fw, "Cannot get CPU info");
		return FWTS_ERROR;
	}

	do_mtrr_resource(fw);

	return FWTS_OK;
}

static int mtrr_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_klog_free(klog);
	fwts_list_free(mtrr_list, free);
	if (fwts_cpuinfo)
		fwts_cpu_free_info(fwts_cpuinfo);

	return FWTS_OK;
}

static int mtrr_test0(fwts_framework *fw)
{
	uint64_t mtrr_def_msr;

	if (fwts_cpu_readmsr(fw, 0, MTRR_DEF_TYPE_MSR, &mtrr_def_msr) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"MTRRMSRDefaultNotAvailable",
			"MTRR_DEF_TYPE_MSR cannot be read from CPU.");
		return FWTS_ERROR;
	}

	if (mtrr_def_msr & MTRR_ENABLE)
		fwts_passed(fw, "MTRRs enabled flag is set in MTRR_DEF_TYPE MSR correctly.");
	else
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"MTRRDisabled",
			"MTRRs enabled flag is clear in MTRR_DEF_TYPE MSR and "
			"all MTRRs are disabled ");

	if (mtrr_def_msr & FIXED_MTRR_ENABLE)
		fwts_passed(fw, "fixed MTRRs enabled flag is set in MTRR_DEF_TYPE MSR correctly.");
	else
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"MTRRFixedRangeDisabled",
			"Fixed MTRRs enabled flag is clear in MTRR_DEF_TYPE MSR and "
			"all Fixed-range MTRRs are disabled ");

	return FWTS_OK;
}

static int mtrr_test1(fwts_framework *fw)
{
	return validate_iomem(fw);
}

static int mtrr_test2(fwts_framework *fw)
{
	if (klog != NULL) {
		bool failed = false;

		if (fwts_klog_regex_find(fw, klog, "mtrr: your CPUs had inconsistent fixed MTRR settings") > 0) {
			fwts_log_info(fw, "Detected CPUs with inconsistent fixed MTRR settings which the kernel fixed.");
			failed = true;
		}
		if (fwts_klog_regex_find(fw, klog, "mtrr: your CPUs had inconsistent variable MTRR settings") > 0) {
			fwts_log_info(fw, "Detected CPUs with inconsistent variable MTRR settings which the kernel fixed.");
			failed = true;
		}
		if (fwts_klog_regex_find(fw, klog, "mtrr: your CPUs had inconsistent MTRRdefType") > 0) {
			fwts_log_info(fw, "Detected CPUs with inconsistent variable MTRR settings which the kernel fixed.");
			failed = true;
		}

		if (failed)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"MTRRCPUsMisconfigured",
				"It is probable that the BIOS does not set up "
				"all the CPUs correctly and the kernel has now "
				"corrected this misconfiguration.");
		else
			fwts_passed(fw, "All processors have the a consistent MTRR setup.");
	} else
		fwts_log_error(fw, "No boot dmesg found.");

	return FWTS_OK;
}

static int mtrr_test3(fwts_framework *fw)
{
	if (fwts_cpuinfo->vendor_id == NULL) {
		fwts_log_error(fw, "Cannot get CPU vendor_id");
		return FWTS_ERROR;
	}
	if (strstr(fwts_cpuinfo->vendor_id, "AMD") || strstr(fwts_cpuinfo->vendor_id, "Hygon")) {
		if (klog != NULL) {
			if (fwts_klog_regex_find(fw, klog, "SYSCFG[MtrrFixDramModEn] not cleared by BIOS, clearing this bit") > 0)
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"MTRRFixDramModEnBit",
					"The BIOS is expected to clear MtrrFixDramModEn bit, see for example "
 					"\"BIOS and Kernel Developer's Guide for the AMD Athlon 64 and AMD "
 					"Opteron Processors\" (26094 Rev. 3.30 February 2006), section "
 					"\"13.2.1.2 SYSCFG Register\": \"The MtrrFixDramModEn bit should be set "
 					"to 1 during BIOS initialization of the fixed MTRRs, then cleared to "
 					"0 for operation.\"");
			else
				fwts_passed(fw, "No MtrrFixDramModEn error detected.");
		}
	} else
		fwts_skipped(fw, "CPU is not an AMD, cannot test.");

	return FWTS_OK;
}

static fwts_framework_minor_test mtrr_tests[] = {
	{ mtrr_test0, "Validate MTRR default enabled." },
	{ mtrr_test1, "Validate the kernel MTRR IOMEM setup." },
	{ mtrr_test2, "Validate the MTRR setup across all processors." },
	{ mtrr_test3, "Test for AMD MtrrFixDramModEn being cleared by the BIOS." },
	{ NULL, NULL }
};

static fwts_framework_ops mtrr_ops = {
	.description = "MTRR tests.",
	.init        = mtrr_init,
	.deinit      = mtrr_deinit,
	.minor_tests = mtrr_tests
};

FWTS_REGISTER("mtrr", &mtrr_ops, FWTS_TEST_EARLY, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
