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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "fwts_acpi_object_eval.h"
#include "crsdump.h"

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

typedef struct {
	const char *label;				/* Field label */
	size_t offset;					/* Offset into _CRS buffer */
	size_t bitlength;				/* Size of field in bits */
	uint64_t bitmask;				/* Bit mask, 0 = use all bits */
	uint8_t shift;					/* Value shift */
	const char **annotation;			/* Annotations */
	const char *(*callback)(const uint64_t val);	/* val -> string mapping callback for CRS_UINTX */
} crsdump_info;

#define CRS_UINT(label, offset, bitlength)		{ label, offset, bitlength, 0, 0, NULL, NULL }
#define CRS_UINT24(label, offset, bitlength)		{ label, offset, bitlength, 0, 8, NULL, NULL }
#define CRS_UINTX(label, offset, bitlength, callback)	{ label, offset, bitlength, 0, 0, NULL, callback }

#define CRS_BITS(label, offset, bitmask)		{ label, offset, 8, bitmask, 0, NULL, NULL }
#define CRS_BITX(label, offset, bitmask, annotation) 	{ label, offset, 8, bitmask, 0, annotation, NULL }
#define CRS_END						{ NULL, 0, 0, 0, 0, 0, NULL }

static void crsdump_show_header(
	fwts_framework *fw,
	const char *objname,
	const char *crs_name)
{
	fwts_log_info_verbatim(fw, "%s (%s):", objname, crs_name);
}

static void crsdump_show_info(
	fwts_framework *fw,
	const uint8_t *data,
	const size_t length,
	const crsdump_info *info)
{
	/*
	 *  Walk through fields and dump data according to the mapping
	 */
	for ( ; info->label; info++) {
		uint64_t val;
		uint64_t mask = info->bitmask;
		int hexdigits = info->bitlength >> 2;

		if (info->offset + (info->bitlength >> 3) > length)
			continue;

		if (info->bitmask) {
			/*
			 *  CRS_BIT*() data
			 */
			val = (uint64_t)*(const uint8_t *)(data + info->offset);
			while (mask && ((mask & 1) == 0)) {
				val >>= 1;
				mask >>= 1;
			}
			val &= mask;
			hexdigits = 2;
		} else {
			/*
			 *  CRS_UINT*() data
			 */
			switch (info->bitlength) {
			case 8:
				val = (uint64_t)*(const uint8_t*)(data + info->offset);
				break;
			case 16:
				val = (uint64_t)*(const uint16_t*)(data + info->offset);
				break;
			case 32:
				val = (uint64_t)*(const uint32_t*)(data + info->offset);
				break;
			case 64:
				val = (uint64_t)*(const uint64_t*)(data + info->offset);
				break;
			default:
				val = ~0;
				break;
			}
		}
		val = val << info->shift;

		if (info->annotation) {
			fwts_log_info_verbatim(fw, "  0x%4.4" PRIx16 ": %-30.30s: 0x%-*.*" PRIx64 " (%s)",
				(uint16_t)info->offset, info->label, hexdigits, hexdigits, val,
				info->annotation[val]);
		} else if (info->callback) {
			fwts_log_info_verbatim(fw, "  0x%4.4" PRIx16 ": %-30.30s: 0x%-*.*" PRIx64 " (%s)",
				(uint16_t)info->offset, info->label, hexdigits, hexdigits, val,
				info->callback(val));
		} else {
			fwts_log_info_verbatim(fw, "  0x%4.4" PRIx16 ": %-30.30s: 0x%-*.*" PRIx64,
				(uint16_t)info->offset, info->label, hexdigits, hexdigits, val);
		}
	}
}

static void crsdump_show(
	fwts_framework *fw,
	const char *objname,
	const char *crs_name,
	const uint8_t *data,
	const size_t length,
	const crsdump_info *header,
	const crsdump_info *info)
{
	crsdump_show_header(fw, objname, crs_name);
	crsdump_show_info(fw, data, length, header);
	crsdump_show_info(fw, data, length, info);
}

static void crsdump_data(
	fwts_framework *fw,
	const uint8_t *data,
	const size_t from,
	const size_t to)
{
	size_t i;
	char buffer[120];

	for (i = from; i < to; i+= 16) {
		size_t n = to - i;

		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, n > 16 ? 16 : n);
		buffer[56] = '\0';	/* Truncate off text version of hex dump */

		fwts_log_info_verbatim(fw, "  0x%4.4" PRIx16 ": %-30.30s: %s",
			(uint16_t)i, "Hex Dump", buffer + 8);
	}
}

/*
 *  crsdump_init()
 *	initialize ACPI
 */
static int crsdump_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  crsdump_deinit
 *	de-intialize ACPI
 */
static int crsdump_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

/*
 *  See section 6.4.2.7 Fixed DMA Descriptor, DMA transfer width
 */
static const char *crs_dma_transfer_width(const uint64_t val)
{
	switch (val) {
	case 0x00:
		return "8 bit";
	case 0x01:
		return "16 bit";
	case 0x02:
		return "32 bit";
	case 0x03:
		return "64 bit";
	case 0x04:
		return "128 bit";
	case 0x05:
		return "256 bit";
	default:
		return "reserved";
	}
}

/*
 *  See section 6.4.3.5.2 DWord Address Space Descriptor Resource Type
 */
static const char *crs_resource_type(const uint64_t val)
{
	switch (val) {
	case 0x00:
		return "Memory range";
	case 0x01:
		return "I/O range";
	case 0x02:
		return "Bus number range";
	case 0xc0 ... 0xff:
		return "Hardware Vendor Defined";
	default:
		return "Reserved";
	}
}

/*
 *  See section 6.4.8.1. Generic Register Descriptor Address Space ID
 */
static const char *crs_address_space_id(const uint64_t val)
{
	switch (val) {
	case 0x00:
		return "System Memory";
	case 0x01:
		return "System I/O";
	case 0x02:
		return "PCI Configuration Space";
	case 0x03:
		return "Embedded Controller";
	case 0x04:
		return "SMBus";
	case 0x0a:
		return "PCC";
	case 0x7f:
		return "Functional Fixed Hardware";
	default:
		return "Unknown";
	}
}

/*
 *  See section 6.4.8.1. Generic Register Descriptor Address Size
 */
static const char *crs_address_size(const uint64_t val)
{
	switch (val) {
	case 0x00:
		return "Undefined (legacy)";
	case 0x01:
		return "Byte Access";
	case 0x02:
		return "Word Access";
	case 0x04:
		return "Dword Access";
	case 0x08:
		return "Qword Access";
	default:
		return "Unknown";
	}
}

/*
 *  See section 6.4.3.8.1 GPIO Connection Description
 */
static const char *crs_gpio_connection_type(const uint64_t val)
{
	switch (val) {
	case 0x00:
		return "Interrupt Connection";
	case 0x01:
		return "I/O Connection";
	default:
		return "Reserved";
	}
}

/*
 *  See section 6.4.3.8.1 GPIO Connection Description
 */
static const char *crs_pin_configuration(const uint64_t val)
{
	switch (val) {
	case 0x00:
		return "Default Configuration";
	case 0x01:
		return "Pull-Up";
	case 0x02:
		return "Pull-Down";
	case 0x03:
		return "No Pull";
	case 0x80 ... 0xff:
		return "Vendor Defined";
	default:
		return "Reserved";
	}
}

/*
 *  Convert IRQ mask into list of IRQs
 */
static const char *crs_irq_map(const uint64_t val)
{
	static char buf[6 + (32 * 4)];

	strncpy(buf, "IRQ:", 5);

	if (!val) {
		strncat(buf, " none", 6);
	} else {
		unsigned int i;

		for (i = 0; i < 32; i++) {
			if (val & (1UL << i)) {
				char tmp[5];

				snprintf(tmp, sizeof(tmp), " %u", i);
				strlcat(buf, tmp, sizeof(tmp));
			}
		}
	}
	return buf;
}

/*
 *  CRS small resource checks, simple checking
 */
static void crsdump_small_resource_items(
	fwts_framework *fw,
	const char *objname,
	const uint8_t *data,
	const size_t length)
{
	uint8_t tag_item = (data[0] >> 3) & 0xf;
	size_t crs_length = 1 + (data[0] & 7);

	static const crsdump_info header[] = {
		CRS_BITS("Tag Type",		0, 128),
		CRS_BITS("Tag Item ID",		0, 64 | 32 | 16 | 8),
		CRS_BITS("Tag Length",		0, 4 | 2 | 1),
	};

	/* Ensure we just dump minimum _CRS buffer length */
	if (crs_length > length)
		crs_length = length;

	switch (tag_item) {
	case 0x4: /* 6.4.2.1 IRQ Descriptor */
		{
			static const char *sharing[] = {
				"Exclusive",
				"Shared",
				"Exclusive And Wake",
				"Shared And Wake"
			};

			static const char *polarity[] = {
				"Active-High",
				"Active-Low"
			};

			static const char *mode[] = {
				"Level-Triggered",
				"Edge-Triggered"
			};

			static const crsdump_info info[] = {
				CRS_UINTX("IRQ Mask",		1, 16, crs_irq_map),
				CRS_BITS("Reserved",		3, 128 | 64),
				CRS_BITX("Interrupt Sharing",	3, 32 | 16, sharing),
				CRS_BITX("Interrupt Polarity",	3, 8, polarity),
				CRS_BITS("Ignored",		3, 4 | 2),
				CRS_BITX("Interrupt Mode",	3, 1, mode),
				CRS_END
			};

			crsdump_show(fw, objname, "IRQ Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x5: /* 6.4.2.2 DMA Descriptor */
		{
			static const char *dma_speed[] = {
				"Compatibility Mode",
				"Type A DMA",
				"Type B DMA",
				"Type F DMA"
			};

			static const char *bus_master[] = {
				"Not a bus master",
				"Is a bus master"
			};

			static const char *dma_size[] = {
				"8 bit only",
				"8 and 16 bit",
				"16 bit only",
				"Reserved"
			};

			static const crsdump_info info[] = {
				CRS_UINT("DMA channel mask",		1, 16),
				CRS_BITS("Reserved",			2, 128),
				CRS_BITX("DMA channel speed",		2, 64 | 32, dma_speed),
				CRS_BITS("Ignored",			2, 16 | 8),
				CRS_BITX("Logical device bus master",	2, 4, bus_master),
				CRS_BITX("DMA transfer type preference",2, 2 | 1, dma_size),
				CRS_END
			};

			crsdump_show(fw, objname, "DMA Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x6: /* 6.4.2.3 Start Dependent Functions Descriptor */
		{
			static const char *config[] = {
				"Good configurarion",
				"Acceptable configuration",
				"Sub-optimal configuration",
				"Reserved"
			};

			static const crsdump_info info[] = {
				CRS_BITS("Reserved",			1, 0xf0),
				CRS_BITX("Performance/robustness",	1, 8 | 4, config),
				CRS_BITX("Compatibility priority",	1, 2 | 1, config),
				CRS_END
			};

			crsdump_show(fw, objname, "Start Dependent Functions Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x7: /* 6.4.2.4 End Dependent Functions Descriptor */
		crsdump_show_header(fw, objname, "End Dependent Functions Descriptor");
		break;
	case 0x8: /* 6.4.2.5 I/O Port Descriptor */
		{
			static const char *decodes[] = {
				"16 bit addresses",
				"10 bit addresses"
			};
			static const crsdump_info info[] = {
				CRS_BITS("Reserved",			1, 0xfe),
				CRS_BITX("Logical Device Decode",	1, 1, decodes),
				CRS_UINT("Minimum Base Address",	2, 16),
				CRS_UINT("Maximum Base Address",	4, 16),
				CRS_UINT("Base Alignment",		6, 8),
				CRS_UINT("Range Length",		7, 8),
				CRS_END
			};

			crsdump_show(fw, objname, "I/O Port Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x9: /* 6.4.2.6 Fixed Location I/O Port Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINT("Range Base Address",		1, 16),
				CRS_UINT("Range Length",		3, 8),
				CRS_END
			};

			crsdump_show(fw, objname, "Fixed Location I/O Port Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0xa: /* 6.4.2.7 Fixed DMA Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINT("DMA Request Line",		1, 16),
				CRS_UINT("DMA Channel",			3, 16),
				CRS_UINTX("DMA Transfer Width",		5, 1, crs_dma_transfer_width),
				CRS_END
			};

			crsdump_show(fw, objname, "Fixed DMA Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0xe: /* 6.4.2.8 Vendor-Defined Descriptor */
		crsdump_show_header(fw, objname, "Vendor-Defined Descriptor");
		crsdump_show_info(fw, data, crs_length, header);
		crsdump_data(fw, data, 3, crs_length);
		break;
	case 0xf: /* 6.4.2.9 End Tag */
		{
			static const crsdump_info info[] = {
				CRS_UINT("Checksum",			1, 8),
				CRS_END
			};

			crsdump_show(fw, objname, "End Tag",
				data, crs_length, header, info);
		}
		break;
	default:
		crsdump_show_header(fw, objname, "Unknown type");
		crsdump_show_info(fw, data, crs_length, header);
		crsdump_data(fw, data, 1, crs_length);
		break;
	}

	fwts_log_nl(fw);
}

/*
 *  CRS large resource checks, simple checking
 */
static void crsdump_large_resource_items(
	fwts_framework *fw,
	const char *objname,
	const uint8_t *data,
	const uint64_t length)
{
	uint8_t tag_item = data[0] & 0x7f;
	size_t crs_length = data[1] + (data[2] << 8) + 3;

	static const crsdump_info header[] = {
		CRS_BITS("Tag Type",		0, 128),
		CRS_BITS("Tag Item ID",		0, 0x7f),
		CRS_UINT("Length",		1, 16),
		CRS_END
	};

	static const char *write_status[] = {
		"non-writeable, read-only",
		"writeable, read/write"
	};

	static const char *mifmaf[] = {
		"Not fixed",
		"Fixed"
	};

	static const char *decode_type[] = {
		"Bridge Positively decodes this address",
		"Bridge Subtractively decode this address"
	};

	static const char *sharing[] = {
		"Exclusive",
		"Shared",
		"Exclusive and Wake",
		"Shared and Wake"
	};

	static const char *polarity[] = {
		"Active-High",
		"Active-Low",
		"Active-Both",
		"Reserved"
	};

	static const char *mode[] = {
		"Level-Triggered",
		"Edge-Triggered"
	};

	static const char *consumer[] = {
		"producer and consumer",
		"consumer"
	};

	/* Ensure we just dump minimum _CRS buffer length */
	if (crs_length > length)
		crs_length = length;

	switch (tag_item) {
	case 0x1: /* 6.4.3.1 24-Bit Memory Range Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_BITX("Write Status",		3, 1, write_status),
				CRS_UINT24("Range Minimum Base",	4, 16),
				CRS_UINT24("Range Maximum Base",	6, 16),
				CRS_UINT("Base Alignment",		8, 16),
				CRS_UINT("Range Length",		10, 16),
				CRS_END
			};

			crsdump_show(fw, objname, "24-Bit Memory Range Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x2: /* 6.4.3.7 Generic Register Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINTX("Address Space ID",		3, 8, crs_address_space_id),
				CRS_UINT("Address Bit Width",		4, 8),
				CRS_UINT("Register Bit Offset",		5, 8),
				CRS_UINTX("Address Size",		6, 8, crs_address_size),
				CRS_UINT("Register Address",		7, 64),
				CRS_END
			};

			crsdump_show(fw, objname, "Generic Register Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x4: /* 6.4.3.2 Vendor-Defined Descriptor */
		crsdump_show_header(fw, objname, "Vendor-Defined Descriptor");
		crsdump_show_info(fw, data, crs_length, header);
		crsdump_data(fw, data, 3, crs_length);
		break;
	case 0x5: /* 6.4.3.3 32-Bit Memory Range Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_BITX("Write Status",		3, 1, write_status),
				CRS_UINT("Range Minimum Address",	4, 32),
				CRS_UINT("Range Maximum Address",	8, 32),
				CRS_UINT("Base Alignment",		12, 32),
				CRS_UINT("Range Length",		16, 32),
				CRS_END
			};

			crsdump_show(fw, objname, "32-Bit Memory Range Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x6: /* 6.4.3.4 32-Bit Fixed Memory Range Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_BITX("Write Status",		3, 1, write_status),
				CRS_UINT("Range Base Address",		4, 32),
				CRS_UINT("Range Length",		8, 32),
				CRS_END
			};

			crsdump_show(fw, objname, "32-Bit Fixed Memory Range Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x7: /* 6.4.3.5.2 DWord Address Space Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINTX("Resource Type",		3, 8, crs_resource_type),
				CRS_BITS("Reserved",			4, 0xf0),
				CRS_BITX("Max Address Fixed",		4, 8, mifmaf),
				CRS_BITX("Min Address Fixed",		4, 4, mifmaf),
				CRS_BITX("Decode Type",			4, 2, decode_type),
				CRS_BITS("Ignored",			4, 1),
				CRS_UINT("Type Specific Flags",		5, 8),
				CRS_UINT("Address Space Granularity",	6, 32),
				CRS_UINT("Address Range Minimum",	10, 32),
				CRS_UINT("Address Range Maximum",	14, 32),
				CRS_UINT("Address Translation Offset",	18, 32),
				CRS_UINT("Address Length",		22, 32),
				CRS_UINT("Resource Source Index",	26, 1),
				/* Skip Resource Source String */
				CRS_END
			};

			crsdump_show(fw, objname, "DWord Address Space Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x8: /* 6.4.3.5.3 Word Address Space Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINTX("Resource Type",		3, 8, crs_resource_type),
				CRS_BITS("Reserved",			4, 0xf0),
				CRS_BITX("Max Address Fixed",		4, 8, mifmaf),
				CRS_BITX("Min Address Fixed",		4, 4, mifmaf),
				CRS_BITX("Decode Type",			4, 2, decode_type),
				CRS_BITS("Ignored",			4, 1),
				CRS_UINT("Type Specific Flags",		5, 8),
				CRS_UINT("Address Space Granularity",	6, 16),
				CRS_UINT("Address Range Minimum",	8, 16),
				CRS_UINT("Address Range Maximum",	10, 16),
				CRS_UINT("Address Translation Offset",	12, 16),
				CRS_UINT("Address Length",		14, 16),
				CRS_UINT("Resource Source Index",	16, 1),
				/* Skip Resource Source String */
				CRS_END
			};

			crsdump_show(fw, objname, "Word Address Space Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0x9: /* 6.4.3.6 Extended Interrupt Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_BITS("Reserved",			3, 128 | 64 | 32),
				CRS_BITX("Interrupt Sharing",		3, 16 | 8, sharing),
				CRS_BITX("Interrupt Polarity",		7, 4, polarity),
				CRS_BITX("Interrupt Mode",		7, 2, mode),
				CRS_BITX("Interrupt Consumer/Producer",	7, 1, consumer),
				CRS_UINT("Interrupt Table Length",	8, 8),
				CRS_END
			};

			crsdump_show(fw, objname, "Extended Interrupt Descriptor",
				data, crs_length, header, info);
			crsdump_data(fw, data, 5, crs_length);
		}
		break;
	case 0xa: /* 6.4.3.5.1 QWord Address Space Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINTX("Resource Type",		3, 8, crs_resource_type),
				CRS_BITS("Reserved",			4, 0xf0),
				CRS_BITX("Max Address Fixed",		4, 8, mifmaf),
				CRS_BITX("Min Address Fixed",		4, 4, mifmaf),
				CRS_BITX("Decode Type",			4, 2, decode_type),
				CRS_BITS("Ignored",			4, 1),
				CRS_UINT("Type Specific Flags",		5, 8),
				CRS_UINT("Address Space Granularity",	6, 64),
				CRS_UINT("Address Range Minimum",	14, 64),
				CRS_UINT("Address Range Maximum",	22, 64),
				CRS_UINT("Address Translation Offset",	30, 64),
				CRS_UINT("Address Length",		38, 64),
				CRS_UINT("Resource Source Index",	46, 1),
				/* Skip Resource Source String */
				CRS_END
			};

			crsdump_show(fw, objname, "QWord Address Space Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0xb: /* 6.4.3.5.4 Extended Address Space Descriptor */
		{
			static const crsdump_info info[] = {
				CRS_UINTX("Resource Type",		3, 8, crs_resource_type),
				CRS_BITS("Reserved",			4, 0xf0),
				CRS_BITX("Max Address Fixed",		4, 8, mifmaf),
				CRS_BITX("Min Address Fixed",		4, 4, mifmaf),
				CRS_BITX("Decode Type",			4, 2, decode_type),
				CRS_BITX("Consumer/Producer",		4, 1, consumer),
				CRS_UINT("Type Specific Flags",		5, 8),
				CRS_UINT("Revision ID",			6, 8),
				CRS_UINT("Reserved",			7, 8),
				CRS_UINT("Address Space Granularity",	8, 64),
				CRS_UINT("Address Range Minimum",	16, 64),
				CRS_UINT("Address Range Maximum",	24, 64),
				CRS_UINT("Address Translation Offset",	32, 64),
				CRS_UINT("Address Length",		40, 64),
				CRS_UINT("Type Specific Attribute",	48, 64),
				CRS_END
			};

			crsdump_show(fw, objname, "Extended Address Space Descriptor",
				data, crs_length, header, info);
		}
		break;
	case 0xc: /* 6.4.3.8.1 GPIO Connection Descriptor */
		{
			if (crs_length < 4)
				break;

			if (data[4] == 0) {

				/* Interrupt connection */
				static const crsdump_info info[] = {
					CRS_UINT("Revision ID",			3, 8),
					CRS_UINTX("GPIO Connection Type",	4, 8, crs_gpio_connection_type),
					CRS_BITS("Reserved",			5, 0xfe),
					CRS_BITX("Consumer/Producer",		5, 1, consumer),
					CRS_UINT("Reserved",			6, 8),
					CRS_BITS("Reserved",			7, 128 | 64 | 32),
					CRS_BITX("Interrupt Sharing and Wake",	7, 16 | 8, sharing),
					CRS_BITX("Interrupt Polarity",		7, 4 | 2, polarity),
					CRS_BITX("Interrupt Mode",		7, 1, mode),
					CRS_UINT("Interrupt and I/O Flags",	8, 8),
					CRS_UINTX("Pin Configuration",		9, 8, crs_pin_configuration),
					CRS_UINT("Output Driver Strength",	10, 16),
					CRS_UINT("Debounce Timeout (bits)",	12, 16),
					CRS_UINT("Pin Table Offset",		14, 16),
					CRS_UINT("Resource Source Index",	16, 8),
					CRS_UINT("Resource Name Offset",	17, 16),
					CRS_UINT("Vendor Data Offset",		19, 16),
					CRS_UINT("Vendor Data Length",		21, 16),
					/* Skip pin table */
					CRS_END
				};

				crsdump_show(fw, objname, "GPIO Connection Descriptor",
					data, crs_length, header, info);
			} else if (data[4] == 1) {
				static const char *int_sharing[] = {
					"Exclusive",
					"Shared",
				};

				static const char *restriction[] = {
					"Input or Output",
					"Input Only",
					"Output Only",
					"Input or Output, config must be preserved"
				};

				/* I/O connection */
				static const crsdump_info info[] = {
					CRS_UINT("Revision ID",			3, 8),
					CRS_UINTX("GPIO Connection Type",	4, 8, crs_gpio_connection_type),
					CRS_BITS("Reserved",			5, 0xfe),
					CRS_BITX("Consumer/Producer",		5, 1, consumer),
					CRS_UINT("Reserved",			6, 8),
					CRS_BITS("Reserved",			7, 128 | 64 | 32 | 16),
					CRS_BITX("Interrupt Sharing",		7, 8, int_sharing),
					CRS_BITS("Reserved",			7, 4),
					CRS_BITX("I/O Restriction",		7, 2 | 1, restriction),
					CRS_UINT("Interrupt and I/O Flags",	8, 8),
					CRS_UINTX("Pin Configuration",		9, 8, crs_pin_configuration),
					CRS_UINT("Output Driver Strength",	10, 16),
					CRS_UINT("Debounce Timeout (bits)",	12, 16),
					CRS_UINT("Pin Table Offset",		14, 16),
					CRS_UINT("Resource Source Index",	16, 8),
					CRS_UINT("Resource Name Offset",	17, 16),
					CRS_UINT("Vendor Data Offset",		19, 16),
					CRS_UINT("Vendor Data Length",		21, 16),
					/* Skip pin table */
					CRS_END
				};

				crsdump_show(fw, objname, "GPIO Connection Descriptor",
					data, crs_length, header, info);
			} else {
				/* No idea of the connection type */
				static const crsdump_info info[] = {
					CRS_UINT("Revision ID",			3, 8),
					CRS_UINT("GPIO Connection Type",	4, 8),
					CRS_END
				};

				crsdump_show(fw, objname, "GPIO Connection Descriptor",
					data, crs_length, header, info);
			}

		}
		break;
	case 0xe: /* 6.4.3.8.2 Serial Bus Connection Descriptors */
		/*  This is not frequently used, deferring implementation to later */
		crsdump_show_header(fw, objname, "Serial Bus Connection Descriptor");
		crsdump_show_info(fw, data, crs_length, header);
		crsdump_data(fw, data, 3, crs_length);
		break;
	default:
		crsdump_show_header(fw, objname, "Unknown type");
		crsdump_show_info(fw, data, crs_length, header);
		crsdump_data(fw, data, 3, crs_length);
		break;
	}

	fwts_log_nl(fw);
}

int resource_dump(fwts_framework *fw, const char *objname)
{
	fwts_list_link	*item;
	fwts_list *objects;
	const size_t name_len = strlen(objname);

	if ((objects = fwts_acpi_object_get_names()) == NULL) {
		fwts_log_info(fw, "Cannot find any ACPI objects");
		return FWTS_ERROR;
	}

	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char*, item);
		const size_t len = strlen(name);
		if (strncmp(objname, name + len - name_len, name_len) == 0) {
			ACPI_OBJECT_LIST arg_list;
			ACPI_BUFFER buf;
			ACPI_OBJECT *obj;
			int ret;

			arg_list.Count   = 0;
			arg_list.Pointer = NULL;

			ret = fwts_acpi_object_evaluate(fw, name, &arg_list, &buf);
			if ((ACPI_FAILURE(ret) != AE_OK) || (buf.Pointer == NULL))
				continue;

			/*  Do we have a valid resource buffer to dump? */
			obj = buf.Pointer;
			if ((obj->Type == ACPI_TYPE_BUFFER) &&
			    (obj->Buffer.Pointer != NULL) &&
			    (obj->Buffer.Length > 0)) {
				uint8_t *data = (uint8_t*)obj->Buffer.Pointer;

				if (data[0] & 128)
					crsdump_large_resource_items(fw, name, data, obj->Buffer.Length);
				else
					crsdump_small_resource_items(fw, name, data, obj->Buffer.Length);
			}
			free(buf.Pointer);
		}
	}
	return FWTS_OK;
}

static int crsdump_test1(fwts_framework *fw)
{
	return resource_dump(fw, "_CRS");
}

static fwts_framework_minor_test crsdump_tests[] = {
	{ crsdump_test1, "Dump ACPI _CRS (Current Resource Settings)." },
	{ NULL, NULL }
};

static fwts_framework_ops crsdump_ops = {
	.description = "Dump ACPI _CRS resources.",
	.init        = crsdump_init,
	.deinit      = crsdump_deinit,
	.minor_tests = crsdump_tests
};

FWTS_REGISTER("crsdump", &crsdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)

#endif
