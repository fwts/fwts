/*
 * Copyright (C) 2015-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "fwts_acpi_object_eval.h"

#define SBBR_DBG2_PORT_SERIAL          0x8000
#define SBBR_DBG2_FULLY_16550_COMPAT   0x0000
#define SBBR_DBG2_16550_SUBSET_COMPAT  0x0001
#define SBBR_DBG2_ARM_PL011_UART       0x0003
#define SBBR_DBG2_ARM_SBSA_UART        0x000E
#define SBBR_DBG2_16550_COMPAT_GAS     0x0012

static fwts_acpi_table_info *table;
acpi_table_init(DBG2, &table)

static int dbg2_test2(fwts_framework *fw)
{
	uint32_t i;

	if (!(fw->flags & FWTS_FLAG_SBBR))
		return FWTS_SKIP;

	fwts_acpi_table_dbg2 *dbg2 = (fwts_acpi_table_dbg2 *)table->data;
	fwts_acpi_table_dbg2_info *info;

	info = (fwts_acpi_table_dbg2_info *)(table->data + dbg2->info_offset);

	for (i = 0; i < dbg2->info_count; i++) {
		if ((uint8_t*)&info->length >= ((uint8_t*)table + table->length))
			break;
		if (((uint8_t*)info + info->length) >= ((uint8_t*)table + table->length))
			break;
		if (info->port_type == SBBR_DBG2_PORT_SERIAL &&
		   (info->port_subtype == SBBR_DBG2_FULLY_16550_COMPAT ||
		    info->port_subtype == SBBR_DBG2_16550_SUBSET_COMPAT ||
		    info->port_subtype == SBBR_DBG2_ARM_PL011_UART ||
		    info->port_subtype == SBBR_DBG2_ARM_SBSA_UART ||
		    info->port_subtype == SBBR_DBG2_16550_COMPAT_GAS)) {
			fwts_passed(fw,
				"DBG2 provides a standard serial debug "
				"port and describes ARM BSA compliant UART");
			return FWTS_OK;
		}

		/* ..and onto the next info structure .. */
		info = (fwts_acpi_table_dbg2_info *)((uint8_t *)info +
			info->length);
	}

	fwts_failed(fw, LOG_LEVEL_CRITICAL, "dbg2_sbsa_uart:",
			"DBG2 provides a non standard debug port");
	return FWTS_OK;
}

/*
 *  dbg2_check_offset()
 *	check if offset does not fall off end of table
 */
static void dbg2_check_offset(
	fwts_framework *fw,
	const size_t table_length,
	const size_t offset,
	const char *msg,
	bool *passed)
{
	if (table_length < offset) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"DBG2TooShort",
			"DBG2 table too short, expecting %zu bytes, "
			"instead got %zu bytes for a DBG2 table and %s",
			offset, table->length, msg);
		*passed = false;
	}
}

/*
 *  dbg2_check_namespace_string()
 *	check namespace string is '\0' termimated
 */
static void dbg2_check_namespace_string(
	fwts_framework *fw,
	const char *str,
	size_t length,
	bool *passed)
{
	size_t i;

	for (i = 0; i < length; i++) {
		if (*(str + i) == '\0')
			return;
	}
	fwts_failed(fw, LOG_LEVEL_HIGH,
		"DBG2NameSpaceStringNull",
		"DBG2 Name Space String was not null terminated");
	*passed = false;
}


/*
 *  DBG2 Table
 *   see https://msdn.microsoft.com/en-us/library/windows/hardware/dn639131%28v=vs.85%29.aspx
 */
static int dbg2_test1(fwts_framework *fw)
{
	bool passed = true;
	fwts_acpi_table_dbg2 *dbg2 = (fwts_acpi_table_dbg2 *)table->data;
	fwts_acpi_table_dbg2_info *info;
	uint32_t i;
	size_t total_size;

	/* Enough length for the initial dbg2 header? */
	if (!fwts_acpi_table_length(fw, "DBG2", table->length, sizeof(fwts_acpi_table_dbg2))) {
		passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "DBG2 Table:");
	fwts_log_info_simp_int(fw, "  Info Offset:              ", dbg2->info_offset);
	fwts_log_info_simp_int(fw, "  Info Count:               ", dbg2->info_count);
	fwts_log_nl(fw);

	total_size = dbg2->info_offset +
		(dbg2->info_count * sizeof(fwts_acpi_table_dbg2_info));
	if (table->length < total_size) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"DBG2TooShort",
			"DBG2 table too short, expecting %zu bytes, "
			"instead got %zu bytes for a DBG2 table "
			"containing %" PRIu32 " debug device "
			"information structures",
			sizeof(fwts_acpi_table_dbg2), table->length,
			dbg2->info_count);
		goto done;
	}

	info = (fwts_acpi_table_dbg2_info *)(table->data + dbg2->info_offset);

	/*
	 *  Sanity check structures
	 */
	for (i = 0; i < dbg2->info_count; i++) {
		uint32_t offset = (uint8_t *)info - (uint8_t *)table->data;
		bool ok, length_ok;
		char *port = NULL, *subport = NULL;

		switch (info->port_type) {
		case 0x8000:
			port = "Serial";
			switch (info->port_subtype) {
			case 0x0000:
				subport = "Fully 16550-compatible";
				break;
			case 0x0001:
				subport = "16550 subset compatible";
				break;
			case 0x0002:
				subport = "MAX311xE SPI UART";
				break;
			case 0x0003:
				subport = "ARMPL011 UART";
				break;
			case 0x0004:
				subport = "MSM8x60 (e.g. 8960)";
				break;
			case 0x0005:
				subport = "Nvidia 16550";
				break;
			case 0x0006:
				subport = "TI OMAP";
				break;
			case 0x0008:
				subport = "APM88xxxx";
				break;
			case 0x0009:
				subport = "MSM8974";
				break;
			case 0x000a:
				subport = "SAM5250";
				break;
			case 0x000b:
				subport = "Intel USIF";
				break;
			case 0x000c:
				subport = "i.MX 6";
				break;
			case 0x000d:
				subport = "ARM SBSA Generic UART (deprecated)";
				break;
			case 0x000e:
				subport = "ARM SBSA Generic UART";
				break;
			case 0x000f:
				subport = "ARM DCC";
				break;
			case 0x0010:
				subport = "BCM2835";
				break;
			case 0x0011:
				subport = "SDM845 with clock rate of 1.8432 MHz";
				break;
			case 0x0012:
				subport = "16550-compatible with parameters in GAS";
				break;
			case 0x0013:
				subport = "SDM845 with clock rate of 7.372 MHz";
				break;
			case 0x0014:
				subport = "Intel LPSS";
				break;
			case 0x0015:
				subport = "RISC-V SBI console (any supported SBI mechanism)";
				break;
			default:
				break;
			}
			break;
		case 0x8001:
			port = "1394";
			switch (info->port_subtype) {
			case 0x0000:
				subport = "IEEE1394";
				break;
			default:
				break;
			}
			break;
		case 0x8002:
			port = "USB";
			switch (info->port_subtype) {
			case 0x0000:
				subport = "XHCI controller";
				break;
			case 0x0001:
				subport = "EHCI controller";
				break;
			default:
				break;
			}
			break;
		case 0x8003:
			port = "Net";
			subport = "PCI";
			break;
		default:
			break;
		}

		fwts_log_info_verbatim(fw, "DBG2 Info Structure %" PRIu32 ":", i);
		fwts_log_info_simp_int(fw, "  Revision:                 ", info->revision);
		fwts_log_info_simp_int(fw, "  Length:                   ", info->length);
		fwts_log_info_simp_int(fw, "  Number of Registers       ", info->number_of_regs);
		fwts_log_info_simp_int(fw, "  Namespace String Length:  ", info->namespace_length);
		fwts_log_info_simp_int(fw, "  Namespace String Offset:  ", info->namespace_offset);
		fwts_log_info_simp_int(fw, "  OEM Data Length:          ", info->oem_data_length);
		fwts_log_info_simp_int(fw, "  OEM Data Offset:          ", info->oem_data_offset);
		fwts_log_info_verbatim(fw, "  Port Type:                0x%4.4" PRIx16 " (%s)", info->port_type,
			port ? port : "(Reserved)");
		fwts_log_info_verbatim(fw, "  Port Subtype:             0x%4.4" PRIx16 " (%s)", info->port_subtype,
			subport ? subport : "(Reserved)");
		fwts_log_info_simp_int(fw, "  Reserved:                 ", info->reserved);
		fwts_log_info_simp_int(fw, "  Base Address Offset:      ", info->base_address_offset);
		fwts_log_info_simp_int(fw, "  Address Size Offset:      ", info->address_size_offset);
		fwts_log_nl(fw);

		fwts_acpi_fixed_value(fw, LOG_LEVEL_HIGH, "DBG2", "Info Structure Revision", info->revision, 0, &passed);

		if (port == NULL) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"DBG2PortTypeReserved",
				"DBG2 Info Structure Port Type is 0x%4.4" PRIx16
				" which is a reserved type.",
				info->port_type);
		}
		if (subport == NULL) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"DBG2PortSubTypeReserved",
				"DBG2 Info Structure Port Subtype is 0x%4.4" PRIx16
				" which is a reserved type.",
				info->port_subtype);
		}

		fwts_acpi_reserved_zero("DBG2", "Info Structure Reserved", info->reserved, &passed);

		length_ok = true;
		dbg2_check_offset(fw, table->length, offset + info->length,
			"DBG2 Info Structure Namespace Length", &length_ok);
		passed &= length_ok;

		ok = true;
		dbg2_check_offset(fw, table->length, offset + info->namespace_offset,
			"DBG2 Info Structure Namespace String Offset", &ok);
		dbg2_check_offset(fw, table->length, offset + info->namespace_offset + info->namespace_length,
			"DBG2 Info Structure Namespace String End", &ok);
		if (ok) {
			char *str = (char *)table->data + offset + info->namespace_offset;
			dbg2_check_namespace_string(fw, str, info->namespace_length, &passed);
			fwts_log_info_verbatim(fw, "  Namespace String:         '%s'", str);
			if (strcmp(str, ".") != 0) {
				bool found = fwts_acpi_obj_find(fw, str);
				if (!found) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
							"DBG2DeviceNotFound",
							"DBG2 Device '%s' not found in ACPI object name space.",
							str);
				}
			}
		}

		dbg2_check_offset(fw, table->length, offset + info->oem_data_offset,
			"DBG2 Info Structure OEM Data Offset", &passed);
		dbg2_check_offset(fw, table->length, offset + info->oem_data_offset + info->oem_data_length,
			"DBG2 Info Structure OEM Data End", &passed);

		ok = true;
		total_size = info->number_of_regs * sizeof(fwts_acpi_gas);
		dbg2_check_offset(fw, table->length, offset + info->base_address_offset,
			"DBG2 Info Structure Base Address Offset", &ok);
		dbg2_check_offset(fw, table->length, offset + info->base_address_offset + total_size,
			"DBG2 Info Structure Base Address End", &ok);
		total_size = info->number_of_regs * sizeof(uint32_t);
		dbg2_check_offset(fw, table->length, offset + info->address_size_offset,
			"DBG2 Info Structure Address Size Offset", &ok);
		dbg2_check_offset(fw, table->length, offset + info->address_size_offset + total_size,
			"DBG2 Info Structure Address Size End", &ok);
		passed &= ok;

		/* Sanity check the GAS array */
		if (ok) {
			uint8_t j;
			fwts_acpi_gas *gas = (fwts_acpi_gas *)(table->data + offset + info->base_address_offset);
			uint32_t *addrsize = (uint32_t *)(table->data + offset + info->address_size_offset);

			for (j = 0; j < info->number_of_regs; j++, gas++, addrsize++) {
				fwts_log_info_simp_int(fw, "    Address Space ID:       ", gas->address_space_id);
				fwts_log_info_simp_int(fw, "    Register Bit Width      ", gas->register_bit_width);
				fwts_log_info_simp_int(fw, "    Register Bit Offset     ", gas->register_bit_offset);
				fwts_log_info_simp_int(fw, "    Access Size             ", gas->access_width);
				fwts_log_info_simp_int(fw, "    Address                 ", gas->address);
				fwts_log_nl(fw);

				if (*addrsize == 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"DBG2InvalidAddressSize",
						"DBG2 Address Size is 0");
				}

				if (gas->register_bit_width == 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"DBG2InvalidGasWidth",
						"DBG2 Generic Address Structure has "
						"zero register bit width which is probably incorrect");
				}
				if (gas->address == 0) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"DBG2InvalidGasAddress",
						"DBG2 Generic Address Structure has "
						"a NULL Address which is incorrect");
				}
			}
		}
		if (!length_ok)
			break;

		/* ..and onto the next info structure .. */
		info = (fwts_acpi_table_dbg2_info *)
			((uint8_t *)info + info->length);
	}

done:
	if (passed)
		fwts_passed(fw, "No issues found in DBG2 table.");

	return FWTS_OK;
}

static fwts_framework_minor_test dbg2_tests[] = {
	{ dbg2_test1, "DBG2 (Debug Port Table 2) test." },
	{ dbg2_test2, "DBG2 ARM BSA compliant UART test," },
	{ NULL, NULL }
};

static fwts_framework_ops dbg2_ops = {
	.description = "DBG2 (Debug Port Table 2) test.",
	.init        = DBG2_init,
	.minor_tests = dbg2_tests
};

FWTS_REGISTER("dbg2", &dbg2_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI | FWTS_FLAG_SBBR)

#endif
