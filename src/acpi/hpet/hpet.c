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

#include <string.h>
#include <inttypes.h>

static fwts_list *klog;


#define HPET_REG_SIZE  (0x400)
#define MAX_CLK_PERIOD (100000000)

static uint64_t	hpet_base_p = 0;
static void     *hpet_base_v = 0;

static void hpet_parse_check_base(
	fwts_framework *fw,
	const char *table,
	fwts_list_link *item,
	bool *parsed)
{
	char *val;

	if ((val = strstr(fwts_text_list_text(item), "0x")) != NULL) {
		uint64_t address_base;
		char *idx = index(val, ',');
		if (idx)
			*idx = '\0';

		address_base = strtoul(val, NULL, 0x10);

		if (hpet_base_p != 0) {
			*parsed = true;
			if (hpet_base_p != address_base)
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"HPETBaseMismatch",
					"Mismatched HPET base between %s (%" PRIx64 ") "
					"and the kernel (0x%" PRIx64 ").",
					table,
					hpet_base_p,
					address_base);
			else
				fwts_passed(fw,
					"HPET base matches that between %s and "
					"the kernel (0x%" PRIx64 ").",
					table,
					hpet_base_p);
		}
	}
}

static void hpet_parse_device_hpet(
	fwts_framework *fw,
	const char *table,
	fwts_list_link *item,
	bool *parsed)
{
	for (; item != NULL; item = item->next) {
		const char *str = fwts_text_list_text(item);

		if ((strstr(str, "Name") != NULL) &&
		    (strstr(str, "ResourceTemplate") != NULL)) {
			fwts_list_link *tmp_item = item->next;
			for (; tmp_item != NULL; tmp_item = tmp_item->next) {
				const char *tmpstr = fwts_text_list_text(tmp_item);

				if (strstr(tmpstr, "Memory32Fixed") != NULL) {
					/* Next line contains base address */
					if (tmp_item->next != NULL) {
						hpet_parse_check_base(fw, table, tmp_item->next, parsed);
						return;
					}
				} else if (strstr(tmpstr, "DWordMemory") != NULL) {
					if (tmp_item->next != NULL &&		/* Granularity */
					    tmp_item->next->next != NULL) {	/* Base address */
						hpet_parse_check_base(fw, table, tmp_item->next->next, parsed);
						return;
					}
				}
			}
		}
	}
}

/*
 *  check_hpet_base_dsdt()
 *	used to parse the DSDT for HPET base info
 */
static void hpet_check_base_acpi_table(
	fwts_framework *fw,
	fwts_acpi_table_info *info,
	bool *parsed)
{
	fwts_list *output;
	fwts_list_link *item;

	if (fwts_iasl_disassemble(fw, info, true, &output) != FWTS_OK) {
		fwts_iasl_deinit();
		return;
	}
	if (output == NULL)
		return;

	fwts_list_foreach(item, output)
		if (strstr(fwts_text_list_text(item), "Device (HPET)") != NULL)
			hpet_parse_device_hpet(fw, info->name, item, parsed);

	fwts_text_list_free(output);
}


static int hpet_check_init(fwts_framework *fw)
{
	if ((klog = fwts_klog_read()) == NULL) {
		fwts_log_error(fw, "Cannot read kernel log.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int hpet_check_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if (klog)
		fwts_text_list_free(klog);

	return FWTS_OK;
}

static int hpet_check_test1(fwts_framework *fw)
{
	fwts_list_link *item;

	if (klog == NULL)
		return FWTS_ERROR;

	fwts_log_info(fw,
		"This test checks the HPET PCI BAR for each timer block "
		"in the timer. The base address is passed by the firmware "
		"via an ACPI table. IRQ routing and initialization is also "
		"verified by the test.");

	fwts_list_foreach(item, klog) {
		char *text = fwts_text_list_text(item);
		/* Old format */
		if (strstr(text, "ACPI: HPET id:") != NULL) {
			char *str = strstr(text, "base: ");
			if (str) {
				hpet_base_p = strtoul(str+6,  NULL, 0x10);
				fwts_passed(fw,
					"Found HPET base 0x%" PRIx64 " in kernel log.",
					hpet_base_p);
				break;
			}
		}
		/* New format */
		/* [    0.277934] hpet0: at MMIO 0xfed00000, IRQs 2, 8, 0 */
		if ((strstr(text, "hpet") != NULL) &&
		    (strstr(text, "IRQs") != NULL)) {
			char *str = strstr(text, "at MMIO ");
			if (str) {
				hpet_base_p = strtoul(str+8,  NULL, 0x10);
				fwts_passed(fw,
					"Found HPET base 0x%" PRIx64 " in kernel log.",
					hpet_base_p);
				break;
			}
		}
	}

	if (hpet_base_p == 0) {
		fwts_log_info(fw, "No base address found for HPET in the kernel log.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 * Sanity check HPET table, see:
 *    http://www.intel.co.uk/content/www/uk/en/software-developers/software-developers-hpet-spec-1-0a.html
 */
static int hpet_check_test2(fwts_framework *fw)
{
	fwts_acpi_table_info *table;
	fwts_acpi_table_hpet *hpet;
	bool passed = true;
	char *page_prot;

	if (fwts_acpi_find_table(fw, "HPET", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table HPET.");
		return FWTS_ERROR;
	}

	if (table == NULL) {
		fwts_log_error(fw, "ACPI table HPET does not exist!");
		return FWTS_ERROR;
	}

	if (table->length < sizeof(fwts_acpi_table_hpet)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "HPETAcpiTableTooSmall",
			"HPET ACPI table is %zd bytes long which is smaller "
			"than the expected size of %zd bytes.",
			table->length, sizeof(fwts_acpi_table_hpet));
		return FWTS_ERROR;
	}

	hpet = (fwts_acpi_table_hpet *)table->data;

	if (hpet->base_address.address == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "HPETAcpiBaseAddressNull",
			"HPET base address in ACPI HPET table is null.");
		passed = false;
	}

	/*
	 * If we've already figured out the HPET base address then
	 * sanity check it against HPET. This should never happen.
	 */
	if ((hpet_base_p != 0) &&
	    (hpet_base_p != hpet->base_address.address)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"HPETAcpiBaseAddressDifferFromKernel",
			"HPET base address in ACPI HPET table "
			"is 0x%" PRIx64 " which is different from "
			"the kernel HPET base address of "
			"0x%" PRIx64 ".",
			hpet->base_address.address, hpet_base_p);
		passed = false;
	}

#if 0
	/*
	 *  The Intel spec states that the address space ID
	 *  must be memory or I/O space.
	 */
	if ((hpet->base_address.address_space_id != 0) &&
	    (hpet->base_address.address_space_id != 1)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"HPETAcpiBaseAddressSpaceId",
			"The HPET base address space ID was 0x%" PRIx8
			", was expecting 0 (System Memory) or "
			"1 (System I/O).",
			hpet->base_address.address_space_id);
		passed = false;
	}
#endif
	/*
	 *  The kernel expects the HPET address space ID
	 *  to be memory.
	 */
	if (hpet->base_address.address_space_id != 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"HPETAcpiBaseAddressSpaceIdNotMemory",
			"The HPET base address space ID was 0x%" PRIx8
			", however, the Kernel expects the HPET address "
			"space ID to be 0 (System Memory). The kernel "
			"will not parse this and the HPET configuration "
			"will be ignored.",
			hpet->base_address.address_space_id);
	}

	/*
	 *  Some broken firmware advertises the HPET at
	 *  0xfed0000000000000 instead of 0xfed00000. The kernel
	 *  detects this and fixes it, but even so, it is wrong
	 *  so lets check for this.
	 */
	if ((hpet->base_address.address >> 32) != 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"HPETAcpiBaseAddressBogus",
			"The HPET base address is bogus: 0x%" PRIx64 ".",
			hpet->base_address.address);
		fwts_advice(fw,
			"Bogus HPET base address can be worked around "
			"by using the kernel parameter 'hpet=force' if "
			"the base address is 0xfed0000000000000. "
			"This will make the kernel shift the address "
			"down 32 bits to 0xfed00000.");
		passed = false;
	}

	if (((hpet->event_timer_block_id >> 16) & 0xffff) == 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "HPETVendorIdZero",
			"HPET PCI Vendor ID is 0x0000, which is invalid.");
		fwts_advice(fw,
			"The HPET specification (http://www.intel.com/hardwaredesign/hpetspec_1.pdf) "
			"describes the HPET table in section 3.2.4 'The ACPI "
			"2.0 HPET Description Table (HPET)'. The top 16 bits "
			"of the Event Timer Block ID specify the Vendor ID "
			"and this should not be zero.  This won't affect the "
			"kernel behaviour, but should be fixed as it is an "
			"undefined ID value.");
	}

	/*
	 * We don't need to check for GAS address space widths etc
	 * since the kernel does not care and the spec doesn't
	 * stipulate these need to be sane
	 */

	/*
	 *   Dump out HPET
	 */
	fwts_log_info_verbatim(fw, "Hardware ID of Event Block:");
	fwts_log_info_verbatim(fw, "  PCI Vendor ID              : 0x%" PRIx32,
		(hpet->event_timer_block_id >> 16) & 0xffff);
	fwts_log_info_verbatim(fw, "  Legacy IRQ Routing Capable : %" PRIu32,
		(hpet->event_timer_block_id >> 15) & 1);
	fwts_log_info_verbatim(fw, "  COUNT_SIZE_CAP counter size: %" PRIu32,
		(hpet->event_timer_block_id >> 13) & 1);
	fwts_log_info_verbatim(fw, "  Number of comparators      : %" PRIu32,
		(hpet->event_timer_block_id >> 8) & 0x1f);
	fwts_log_info_verbatim(fw, "  Hardwre Revision ID        : 0x%" PRIx8,
		hpet->event_timer_block_id & 0xff);

	fwts_log_info_verbatim(fw, "Lower 32 bit base Address    : 0x%" PRIx64,
		hpet->base_address.address);
	fwts_log_info_verbatim(fw, "  Address Space ID           : 0x%" PRIx8,
		hpet->base_address.address_space_id);
	fwts_log_info_verbatim(fw, "  Register Bit Width         : 0x%" PRIx8,
		hpet->base_address.register_bit_width);
	fwts_log_info_verbatim(fw, "  Register Bit Offset        : 0x%" PRIx8,
		hpet->base_address.register_bit_offset);
	fwts_log_info_verbatim(fw, "  Address Width              : 0x%" PRIx8,
		hpet->base_address.access_width);
	fwts_log_info_verbatim(fw, "HPET sequence number         : 0x%" PRIx8,
		hpet->hpet_number);
	fwts_log_info_verbatim(fw, "Minimum clock tick           : 0x%" PRIx16,
		hpet->main_counter_minimum);

	switch (hpet->page_prot_and_oem_attribute & 0xf) {
	case 0:
		page_prot = "No guaranteed protection";
		break;
	case 1:
		page_prot = "4K page protected";
		break;
	case 2:
		page_prot = "64K page protected";
		break;
	default:
		page_prot = "Reserved";
		break;
	}
	fwts_log_info_verbatim(fw, "Page Protection              : 0x%" PRIx8 " (%s)",
		hpet->page_prot_and_oem_attribute & 0xf,
		page_prot);
	fwts_log_info_verbatim(fw, "OEM attributes               : 0x%" PRIx8,
		(hpet->page_prot_and_oem_attribute >> 4) & 0xf);

	if (passed)
		fwts_passed(fw, "HPET looks sane.");

	return FWTS_OK;
}

static int hpet_check_test3(fwts_framework *fw)
{
	int i;
	bool parsed = false;

	if (hpet_base_p == 0) {
		fwts_log_info(fw, "Test skipped because HPET address was not found.");
		return FWTS_SKIP;
	}

	if (fwts_iasl_init(fw) != FWTS_OK) {
		fwts_warning(fw, "Failure to initialise iasl, aborting.");
		fwts_iasl_deinit();
		return FWTS_ERROR;
	}

	for (i = 0; i < ACPI_MAX_TABLES; i++) {
		fwts_acpi_table_info *info;

		if (fwts_acpi_get_table(fw, i, &info) != FWTS_OK)
			break;
		if (info && info->has_aml)
			hpet_check_base_acpi_table(fw, info, &parsed);
	}
	fwts_iasl_deinit();

	if (!parsed)
		fwts_warning(fw, "Test skipped because HPET Device address was not found in DSDT/SSDT.");

	return FWTS_OK;
}


static int hpet_check_test4(fwts_framework *fw)
{
	uint64_t hpet_id;
	uint32_t vendor_id;
	uint32_t clk_period;

	if (hpet_base_p == 0) {
		fwts_log_info(fw, "Test skipped because HPET address was not found.");
		return FWTS_SKIP;
	}

	hpet_base_v = fwts_mmap(hpet_base_p, HPET_REG_SIZE);
	if (hpet_base_v == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap to /dev/mem.");
		return FWTS_ERROR;
	}

	if (fwts_safe_memread64(hpet_base_v, HPET_REG_SIZE / 8) != FWTS_OK) {
		fwts_log_info(fw, "Test skipped because HPET region cannot be read.");
		(void)fwts_munmap(hpet_base_v, HPET_REG_SIZE);
		return FWTS_SKIP;
	}

	usleep(10);

	hpet_id = *(uint64_t*) hpet_base_v;
	vendor_id = (hpet_id & 0xffff0000) >> 16;

	if (vendor_id == 0xffff)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "HPETVendorId",
			"Invalid Vendor ID: %04" PRIx32 " - this should be configured.",
			vendor_id);
	else
		fwts_passed(fw, "Vendor ID looks sane: 0x%04" PRIx32 ".", vendor_id);

	clk_period = hpet_id >> 32;
	if ((clk_period > MAX_CLK_PERIOD) || (clk_period == 0))
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "HPETClockPeriod",
			"Invalid clock period %" PRIu32 ", must be non-zero and "
			"less than 10^8.", clk_period);
	else
		fwts_passed(fw, "Valid clock period %" PRIu32 ".", clk_period);

	(void)fwts_munmap(hpet_base_v, HPET_REG_SIZE);

	return FWTS_OK;
}


static fwts_framework_minor_test hpet_check_tests[] = {
	{ hpet_check_test1, "Test HPET base in kernel log." },
	{ hpet_check_test2, "Test HPET base in HPET table."},
	{ hpet_check_test3, "Test HPET base in DSDT and/or SSDT."},
	{ hpet_check_test4, "Test HPET configuration." },
	{ NULL, NULL }
};

static fwts_framework_ops hpet_check_ops = {
	.description = "HPET IA-PC High Precision Event Timer Table tests.",
	.init        = hpet_check_init,
	.deinit      = hpet_check_deinit,
	.minor_tests = hpet_check_tests
};

FWTS_REGISTER("hpet", &hpet_check_ops, FWTS_TEST_ANYTIME,
	FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_ACPI)

#endif
