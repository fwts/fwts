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
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

/* PCI IRQ Routing Table, see http://www.microsoft.com/taiwan/whdc/archive/pciirq.mspx */

#define PCIIRQ_REGION_START	(0x000f0000)
#define PCIIRQ_REGION_END  	(0x000fffff)
#define PCIIRQ_REGION_SIZE	(PCIIRQ_REGION_END - PCIIRQ_REGION_START)

#define RESERVED_SIZE		(11)

typedef struct {
	uint8_t		link;
	uint16_t	bitmap;
} __attribute__ ((packed)) INT_entry;

typedef struct {
	uint8_t		pci_bus_number;
	uint8_t		pci_dev_number;
	INT_entry	INT[4];
	uint8_t		slot_number;
	uint8_t		reserved;
}  __attribute__ ((packed)) slot_entry;

typedef struct {
	uint8_t		signature[4];
	uint16_t	version;
	uint16_t	table_size;
	uint8_t		routers_bus;
	uint8_t		routers_devfunc;
	uint16_t	exclusive_irqs;
	uint32_t	compatible_pci_router;
	uint32_t	miniport_data;
	uint8_t		reserved[RESERVED_SIZE];
	uint8_t		checksum;
	slot_entry	slots[0];
}  __attribute__ ((packed)) pci_irq_routing_table;

static const char *pciirq_reserved(const uint8_t *data)
{
	static char buf[1+ (RESERVED_SIZE * 5)];
	char tmp[6];
	int i;

	*buf = '\0';

	for (i = 0; i < RESERVED_SIZE; i++) {
		snprintf(tmp, sizeof(tmp), "%s0x%2.2x", *buf ? ",": "", data[i]);
		strcat(buf, tmp);
	}
	return buf;
}

static const char *pciirq_irq_bitmap(uint16_t val)
{
	static char buf[40];

	*buf = '\0';
	if (val) {
		int i;

		for (i = 0; i < 16; val >>= 1, i++) {
			if (val & 1) {
				char tmp[5];
				snprintf(tmp, sizeof(tmp), "%s%d", *buf ? " ": "", i);
				strcat(buf, tmp);
			}
		}
		return buf;
	} else
		return "none";
}

static int pciirq_test1(fwts_framework *fw)
{
	uint8_t *mem;
	int i;
	int found = 0;

	static uint8_t empty_reserved[RESERVED_SIZE];

	fwts_log_info(fw,
		"This test tries to find and sanity check the PCI IRQ Routing "
		"Table, as defined by "
		"http://www.microsoft.com/taiwan/whdc/archive/pciirq.mspx "
		" and described in pages 233-238 of PCI System Architecture, "
		"Fourth Edition, Mindshare, Inc. (1999). "
		"NOTE: The PCI IRQ Routing Table only really knows about ISA IRQs "
		"and is generally not used with APIC.");

        if ((mem = fwts_mmap(PCIIRQ_REGION_START,
			PCIIRQ_REGION_SIZE)) == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap firmware region.");
		return FWTS_ERROR;
	}
	if (fwts_safe_memread(mem, PCIIRQ_REGION_SIZE) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read firmware region.");
		(void)fwts_munmap(mem, PCIIRQ_REGION_SIZE);
		return FWTS_ERROR;
	}

	for (i = 0; i < PCIIRQ_REGION_SIZE; i+= 16) {
		pci_irq_routing_table *pciirq = (pci_irq_routing_table*)(mem+i);
		if ((memcmp(pciirq->signature, "$PIR", 4) == 0) &&
		    (fwts_checksum(mem+i, pciirq->table_size) == 0)) {
			int j, k;
			slot_entry *slot;
			int slot_count = (pciirq->table_size - 32) / sizeof(slot_entry);
			int expected_size = (32 + (slot_count * sizeof(slot_entry)));
			bool slot_ok = true;

			fwts_log_nl(fw);
			fwts_log_info(fw, "Found PCI IRQ Routing Table at 0x%8.8x", PCIIRQ_REGION_START+i);
			fwts_log_info_verbatim(fw, "  Signature             : %4.4s",
				pciirq->signature);
			fwts_log_info_verbatim(fw, "  Version               : 0x%4.4x (%u.%u)",
				pciirq->version,
				pciirq->version >> 8,
				pciirq->version & 0xff);
			fwts_log_info_verbatim(fw, "  Table Size            : 0x%4.4x bytes (%d slot entries)",
				pciirq->table_size, (pciirq->table_size - 32) / 16);
			fwts_log_info_verbatim(fw, "  PCI Router ID         : %02x:%02x.%1x",
				pciirq->routers_bus,
				pciirq->routers_devfunc >> 3,
				pciirq->routers_devfunc & 0x7);
			fwts_log_info_verbatim(fw, "  PCI Exclusive IRQs    : 0x%4.4x (%s)",
				pciirq->exclusive_irqs,
				pciirq_irq_bitmap(pciirq->exclusive_irqs));
			fwts_log_info_verbatim(fw, "  Compatible PCI Router : %4.4x:%4.4x",
				pciirq->compatible_pci_router & 0xffff,
				pciirq->compatible_pci_router >> 16);
			fwts_log_info_verbatim(fw, "  Miniport Data         : 0x%8.8x%s",
				pciirq->miniport_data,
				pciirq->miniport_data ? "" : " (none)");
			fwts_log_info_verbatim(fw, "  Reserved              : %s",
				pciirq_reserved(pciirq->reserved));
			fwts_log_info_verbatim(fw, "  Checksum              : 0x%2.2x",
				pciirq->checksum);
			fwts_log_nl(fw);

			/*
			 *  Dump table
			 */
			fwts_log_info_verbatim(fw, "Bus:Dev Slot  INTA#   INTB#   INTC#   INTD#");
			for (slot = pciirq->slots, j = 0; j < slot_count; j++, slot++) {
				char buffer[80];
				char *ptr = buffer;

				ptr += snprintf(ptr, sizeof(buffer),
					" %2.2x:%2.2x   %2.2x  ",
					slot->pci_bus_number, slot->pci_dev_number >> 3,
					slot->slot_number);
				for (k = 0; k < 4; k++) {
					if (slot->INT[k].link)
						ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer),
							"%2.2x/%4.4x ", slot->INT[k].link, slot->INT[k].bitmap);
					else
						ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer), "        ");
				}
				fwts_log_info_verbatim(fw, "%s", buffer);
			}
			fwts_log_nl(fw);

			found++;

			/* Minimal tests */
			if (pciirq->compatible_pci_router == 0)
				fwts_failed(fw, LOG_LEVEL_LOW, "PCIIRQBadCompatPCIRouterID",
					"The Compatible PCI Interrupt Router was zero, which is "
					"probably undefined.");
			else
				fwts_passed(fw, "The Compatible PCI Interrupt Router is defined.");
			/* Bad sized table? */
			if (expected_size == pciirq->table_size)
				fwts_passed(fw, "Table size was correct for %d slot entries.", slot_count);
			else
				fwts_failed(fw, LOG_LEVEL_LOW, "PCIIRQBadTableSize",
					"The table had %d slot entries and was expected "
					"to be %d bytes in size, in fact it was %d bytes.",
					slot_count, expected_size, (int)pciirq->table_size);

			/* Spec states reserved needs to be set to zero */
			if (memcmp(pciirq->reserved, empty_reserved, RESERVED_SIZE))
				fwts_failed(fw, LOG_LEVEL_LOW, "PCIIRQBadReserved",
					"The reserved region in the table should be set "
					"to zero, however some of the data is non-zero "
					"and therefore non-compliant.");
			else
				fwts_passed(fw, "Reserved region is set to zero.");

			/*
			 *  This is a fairly shallow test
			 */
			for (slot = pciirq->slots, j = 0; j < slot_count; j++, slot++) {
				for (k = 0; k < 4; k++) {
					if ((slot->INT[k].link != 0) && (slot->INT[k].bitmap == 0)) {
						fwts_failed(fw, LOG_LEVEL_MEDIUM, "PCIIRQLinkBitmap",
							"Slot %d INT%c# has a has an link connected "
							"but the IRQ bitmap is not defined.", j, k + 'A');
						slot_ok = false;
					}
				}
			}
			if (slot_ok)
				fwts_passed(fw, "All %d slots have sane looking link and IRQ bitmaps.", slot_count);
		}
	}

	if (found == 0) {
		fwts_log_nl(fw);
		fwts_log_info(fw,
			"Could not find PCI IRQ Routing Table. Since this table "
			"is for legacy BIOS systems which don't have ACPI support "
			"this is generally not a problem.");
	} else if (found > 1)
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"PCIIRQMultipleTables",
			"Found %d instances of PCI Routing Tables, there should only be 1.", found);

        (void)fwts_munmap(mem, PCIIRQ_REGION_SIZE);

	return FWTS_OK;
}

static fwts_framework_minor_test pciirq_tests[] = {
	{ pciirq_test1, "PCI IRQ Routing Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops pciirq_ops = {
	.description = "PCI IRQ Routing Table test.",
	.minor_tests = pciirq_tests
};

FWTS_REGISTER("pciirq", &pciirq_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
