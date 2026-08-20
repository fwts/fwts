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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef __PCI_H__
#define __PCI_H__

/*
 *  PCI specific definitions
 */
#define FWTS_PCI_DEV_PATH				"/sys/bus/pci/devices"

#define FWTS_PCI_CAPABILITIES_LAST_ID 			(0x00)
#define FWTS_PCI_EXPRESS_CAP_ID				(0x10)

#define FWTS_PCI_CAPABILITIES_NEXT_POINTER		(0x01)
#define FWTS_PCI_CAPABILITIES_DEV_CTRL			(0x08)

/* Offsets common on PCI type 0,1,2 config space header */
#define FWTS_PCI_CONFIG_VENDOR_ID			(0x00)
#define FWTS_PCI_CONFIG_DEVICE_ID			(0x02)
#define FWTS_PCI_CONFIG_COMMAND				(0x04)
#define FWTS_PCI_CONFIG_STATUS				(0x06)
#define FWTS_PCI_CONFIG_REVISION_ID			(0x08)
#define FWTS_PCI_CONFIG_PROG_IF				(0x09)
#define FWTS_PCI_CONFIG_SUBCLASS			(0x0a)
#define FWTS_PCI_CONFIG_CLASS_CODE			(0x0b)
#define FWTS_PCI_CONFIG_CACHE_LINE_SIZE			(0x0c)
#define FWTS_PCI_CONFIG_LATENCY_TIMER			(0x0d)
#define FWTS_PCI_CONFIG_HEADER_TYPE			(0x0e)
#define FWTS_PCI_CONFIG_BIST				(0x0f)

/* Offsets in PCI Type 0 Non-Bridge config space header */
#define FWTS_PCI_CONFIG_TYPE0_BAR0			(0x10)
#define FWTS_PCI_CONFIG_TYPE0_BAR1			(0x14)
#define FWTS_PCI_CONFIG_TYPE0_BAR2			(0x18)
#define FWTS_PCI_CONFIG_TYPE0_BAR3			(0x1c)
#define FWTS_PCI_CONFIG_TYPE0_BAR4			(0x20)
#define FWTS_PCI_CONFIG_TYPE0_BAR5			(0x24)
#define FWTS_PCI_CONFIG_TYPE0_CARDBUS_CIS_POINTER	(0x28)
#define FWTS_PCI_CONFIG_TYPE0_SUBSYSTEM_VENDOR_ID	(0x2c)
#define FWTS_PCI_CONFIG_TYPE0_SUBSYSTEM_ID		(0x2e)
#define FWTS_PCI_CONFIG_TYPE0_EXPENSION_ROM_ADDRESS	(0x30)
#define FWTS_PCI_CONFIG_TYPE0_CAPABILITIES_POINTER	(0x34)
#define FWTS_PCI_CONFIG_TYPE0_INTERRUPT_LINE		(0x3c)
#define FWTS_PCI_CONFIG_TYPE0_INTERRUPT_PIN		(0x3d)
#define FWTS_PCI_CONFIG_TYPE0_MIN_GRANT			(0x3e)
#define FWTS_PCI_CONFIG_TYPE0_MAX_LATENCY		(0x3f)

/* Offsets in PCI Type 1 PCI-to-PCI bridge config space header */
#define FWTS_PCI_CONFIG_TYPE1_BAR0			(0x10)
#define FWTS_PCI_CONFIG_TYPE1_BAR1			(0x14)
#define FWTS_PCI_CONFIG_TYPE1_PRIMARY_BUS_NUMBER	(0x18)
#define FWTS_PCI_CONFIG_TYPE1_SECONDARY_BUS_NUMBER	(0x19)
#define FWTS_PCI_CONFIG_TYPE1_SUBORDINATE_BUS_NUMBER	(0x1a)
#define FWTS_PCI_CONFIG_TYPE1_SECONDARY_LATENCY_TIMER	(0x1b)
#define FWTS_PCI_CONFIG_TYPE1_IO_BASE			(0x1c)
#define FWTS_PCI_CONFIG_TYPE1_IO_LIMIT			(0x1d)
#define FWTS_PCI_CONFIG_TYPE1_SECONDARY_STATUS		(0x1e)
#define FWTS_PCI_CONFIG_TYPE1_MEMORY_BASE		(0x20)
#define FWTS_PCI_CONFIG_TYPE1_MEMORY_LIMIT		(0x22)
#define FWTS_PCI_CONFIG_TYPE1_PREFETCHABLE_MEMORY_BASE	(0x24)
#define FWTS_PCI_CONFIG_TYPE1_PREFETCHABLE_MEMORY_LIMIT	(0x26)
#define FWTS_PCI_CONFIG_TYPE1_PREFETCABLE_BASE_UPPER32	(0x28)
#define FWTS_PCI_CONFIG_TYPE1_PREFETCABLE_LIMIT_UPPER32	(0x2c)
#define FWTS_PCI_CONFIG_TYPE1_IO_BASE_UPPER16		(0x30)
#define FWTS_PCI_CONFIG_TYPE1_IO_LIMIT_UPPER16		(0x32)
#define FWTS_PCI_CONFIG_TYPE1_CAPABILITY_POINTER	(0x34)
#define FWTS_PCI_CONFIG_TYPE1_EXPANSION_ROM_ADDRESS	(0x38)
#define FWTS_PCI_CONFIG_TYPE1_INTERRUPT_LINE		(0x3c)
#define FWTS_PCI_CONFIG_TYPE1_INTERRUPT_PIN		(0x3d)
#define FWTS_PCI_CONFIG_TYPE1_BRIDGE_CONTROL		(0x3e)

/* Offsets in PCI Type 2 PCI-to-CardBus bridge config space header */
#define FWTS_PCI_CONFIG_TYPE2_CARDBUS_BASE_ADDRESS	(0x10)
#define FWTS_PCI_CONFIG_TYPE2_CAPABILITY_POINTER	(0x14)
#define FWTS_PCI_CONFIG_TYPE2_SECONDARY_STATUS		(0x16)
#define FWTS_PCI_CONFIG_TYPE2_PCI_BUS_NUMBER		(0x18)
#define FWTS_PCI_CONFIG_TYPE2_CARDBUS_BUS_NUMBER	(0x19)
#define FWTS_PCI_CONFIG_TYPE2_SUBORDINATE_BUS_NUMBER	(0x1a)
#define FWTS_PCI_CONFIG_TYPE2_CARDBUS_LATENCY_TIMER	(0x1b)
#define FWTS_PCI_CONFIG_TYPE2_MEMORY_BASE_ADDRESS0	(0x1c)
#define FWTS_PCI_CONFIG_TYPE2_MEMORY_LIMIT0		(0x20)
#define FWTS_PCI_CONFIG_TYPE2_MEMORY_BASE_ADDRESS1	(0x24)
#define FWTS_PCI_CONFIG_TYPE2_MEMORY_LIMIT1		(0x28)
#define FWTS_PCI_CONFIG_TYPE2_IO_BASE_ADDRESS0		(0x2c)
#define FWTS_PCI_CONFIG_TYPE2_IO_BASE_LIMIT0		(0x30)
#define FWTS_PCI_CONFIG_TYPE2_IO_BASE_ADDRESS1		(0x34)
#define FWTS_PCI_CONFIG_TYPE2_IO_BASE_LIMIT1		(0x38)
#define FWTS_PCI_CONFIG_TYPE2_INTERRUPT_LINE		(0x3c)
#define FWTS_PCI_CONFIG_TYPE2_INTERRUPT_PIN		(0x3d)
#define FWTS_PCI_CONFIG_TYPE2_BRIDGE_CONTROL		(0x3e)
#define FWTS_PCI_CONFIG_TYPE2_SUBSYSTEM_DEVICE_ID	(0x40)
#define FWTS_PCI_CONFIG_TYPE2_SUBSYSTEM_VENDOR_ID	(0x42)
#define FWTS_PCI_CONFIG_TYPE2_LEGACY_MODE_BASE_ADDRESS	(0x44)

/* PCI config device port type */
#define FWTS_PCI_EXP_FLAGS_TYPE				0x00f0
#define  FWTS_PCI_EXP_TYPE_ENDPOINT			(0x0)
#define  FWTS_PCI_EXP_TYPE_LEGACY_ENDPOINT		(0x1)
#define  FWTS_PCI_EXP_TYPE_ROOT_PORT			(0x4)
#define  FWTS_PCI_EXP_TYPE_UPSTREAM_PORT		(0x5)
#define  FWTS_PCI_EXP_TYPE_DOWNSTREAM_PORT		(0x6)
#define  FWTS_PCI_EXP_TYPE_PCI_BRIDGE			(0x7)
#define  FWTS_PCI_EXP_TYPE_PCIE_BRIDGE			(0x8)
#define  FWTS_PCI_EXP_TYPE_RC_ENDPOINT			(0x9)
#define  FWTS_PCI_EXP_TYPE_RC_EVENT_COLLECTOR		(0xa)

/* PCI config header types */
#define FWTS_PCI_CONFIG_HEADER_TYPE_NON_BRIDGE		(0x00)
#define FWTS_PCI_CONFIG_HEADER_TYPE_PCI_BRIDGE		(0x01)
#define FWTS_PCI_CONFIG_HEADER_TYPE_CARDBUS_BRIDGE	(0x02)

/* PCI config BAR types */
#define FWTS_PCI_CONFIG_BAR_MEM				(0x00)
#define FWTS_PCI_CONFIG_BAR_IO				(0x01)
#define FWTS_PCI_CONFIG_BAR_PREFETCHABLE		(0x08)

/* PCI config BAR address types */
#define FWTS_PCI_CONFIG_BAR_TYPE_ADDR_32BIT		(0x00)
#define FWTS_PCI_CONFIG_BAR_TYPE_ADDR_16BIT		(0x01)
#define FWTS_PCI_CONFIG_BAR_TYPE_ADDR_64BIT		(0x02)

/* PCI class codes */
#define FWTS_PCI_CLASS_CODE_HOST_BRIDGE			(0x00)
#define FWTS_PCI_CLASS_CODE_MASS_STORAGE_CONTOLLER	(0x01)
#define FWTS_PCI_CLASS_CODE_NETWORK_CONTROLLER		(0x02)
#define FWTS_PCI_CLASS_CODE_DISPLAY_CONTROLLER		(0x03)
#define FWTS_PCI_CLASS_CODE_MULTIMEDIA_CONTROLLER	(0x04)
#define FWTS_PCI_CLASS_CODE_MEMORY_CONTROLLER		(0x05)
#define FWTS_PCI_CLASS_CODE_BRIDGE_CONTROLLER		(0x06)
#define FWTS_PCI_CLASS_CODE_SIMPLE_COMMS_CONTROLLER	(0x07)
#define FWTS_PCI_CLASS_CODE_BASE_SYSTEM_PERIPHERALS	(0x08)
#define FWTS_PCI_CLASS_CODE_INPUT_DEVICE		(0x09)
#define FWTS_PCI_CLASS_CODE_DOCKING_STATION		(0x0a)
#define FWTS_PCI_CLASS_CODE_PROCESSOR			(0x0b)
#define FWTS_PCI_CLASS_CODE_SERIAL_BUS_CONTROLLER	(0x0c)
#define FWTS_PCI_CLASS_CODE_WIRELESS_CONTROLLER		(0x0d)
#define FWTS_PCI_CLASS_CODE_INTELLIGENT_IO_CONTROLLER	(0x0e)
#define FWTS_PCI_CLASS_CODE_SATELLITE_COMMS_CONTROLLER	(0x0f)
#define FWTS_PCI_CLASS_CODE_CRYPTO_CONTROLLER		(0x10)
#define FWTS_PCI_CLASS_CODE_SIGNAL_PROC_CONTROLLER	(0x11)
#define FWTS_PCI_CLASS_CODE_NO_CLASSICATION		(0xff)

/* Just a handful of sub-class codes */
#define FWTS_PCI_SUBCLASS_CODE_HOST_BRIDGE		(0x00)
#define FWTS_PCI_SUBCLASS_CODE_PCI_TO_PCI_BRIDGE	(0x04)
#define FWTS_PCI_SUBCLASS_CODE_OTHER_SYSTEM_PERIPHERAL	(0x80)
#define FWTS_PCI_SUBCLASS_CODE_AUDIO_DEVICE		(0x03)

/* PCI Vendor IDs */
#define FWTS_PCI_INTEL_VENDOR_ID			(0x8086)

/*
 * PCI Express Capability Structure is defined in Section 7.8
 * of PCI Express®i Base Specification Revision 2.0
 */
typedef struct {
	uint8_t pcie_cap_id;
	uint8_t next_cap_point;
	uint16_t pcie_cap_reg;
	uint32_t device_cap;
	uint16_t device_contrl;
	uint16_t device_status;
	uint32_t link_cap;
	uint16_t link_contrl;
	uint16_t link_status;
	uint32_t slot_cap;
	uint16_t slot_contrl;
	uint16_t slot_status;
	uint16_t root_contrl;
	uint16_t root_cap;
	uint32_t root_status;
	uint32_t device_cap2;
	uint16_t device_contrl2;
	uint16_t device_status2;
	uint32_t link_cap2;
	uint16_t link_contrl2;
	uint16_t link_status2;
	uint32_t slot_cap2;
	uint16_t slot_contrl2;
 	uint16_t slot_status2;
} __attribute__ ((packed)) fwts_pcie_capability;

typedef struct {
	uint8_t segment;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
	uint8_t config[256];
} fwts_pci_device;

const char *fwts_pci_description(const uint8_t class_code, const uint8_t subclass_code);

#endif
