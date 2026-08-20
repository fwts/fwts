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

#include <stdint.h>

#include "fwts.h"

/*
 *  class_code, subclass_code --> description mapping
 */
typedef struct {
	uint8_t		class_code;
	uint8_t		subclass_code;
	const char 	*description;
} fwts_pci_description_info;

/*
 *  http://wiki.osdev.org/PCI
 */
static const fwts_pci_description_info descriptions[] = {
	{ 0x00, 0x00, "Any device" },
	{ 0x00, 0x01, "VGA compatible device" },
	{ 0x01, 0x00, "SCSI bus controller" },
	{ 0x01, 0x01, "IDE controller" },
	{ 0x01, 0x02, "Floppy disk controller" },
	{ 0x01, 0x03, "IPI bus controller" },
	{ 0x01, 0x04, "RAID controller" },
	{ 0x01, 0x05, "ATA controller" },
	{ 0x01, 0x06, "SATA controller" },
	{ 0x01, 0x07, "SATA controller (AHCI 1.0)" },
	{ 0x01, 0x80, "Mass storage controller" },
	{ 0x02, 0x00, "Ethernet controller" },
	{ 0x02, 0x01, "Token ring controller" },
	{ 0x02, 0x02, "FDDI controller" },
	{ 0x02, 0x03, "ATM controller" },
	{ 0x02, 0x04, "ISDN controller" },
	{ 0x02, 0x05, "WordlFip controller" },
	{ 0x02, 0x06, "PICMG 2.14 multi computing" },
	{ 0x02, 0x80, "Network controller" },
	{ 0x03, 0x00, "VGA/8512 compatible controller" },
	{ 0x03, 0x01, "XGA controller" },
	{ 0x03, 0x02, "3D controller" },
	{ 0x03, 0x80, "Display controller" },
	{ 0x04, 0x00, "Video Device" },
	{ 0x04, 0x01, "Audio Device" },
	{ 0x04, 0x02, "Computer telephony device" },
	{ 0x04, 0x80, "Media controller" },
	{ 0x05, 0x00, "RAM controller" },
	{ 0x05, 0x01, "Flash controller" },
	{ 0x05, 0x80, "Memory controller" },
	{ 0x06, 0x00, "Host bridge" },
	{ 0x06, 0x01, "ISA bridge" },
	{ 0x06, 0x02, "EISA bridge" },
	{ 0x06, 0x03, "MCA bridge" },
	{ 0x06, 0x04, "PCI-to-PCI bridge" },
	{ 0x06, 0x05, "PCMCIA bridge" },
	{ 0x06, 0x06, "NuBus bridge" },
	{ 0x06, 0x07, "CardBus bridge" },
	{ 0x06, 0x08, "RACEway bridge" },
	{ 0x06, 0x09, "PCI-to-PCI bridge" },
	{ 0x06, 0x0a, "PCI-to-PCI bridge" },
	{ 0x06, 0x80, "Bridge device" },
	{ 0x07, 0x00, "Serial controller" },
	{ 0x07, 0x01, "Parellel port" },
	{ 0x07, 0x02, "Multiport serial controller" },
	{ 0x07, 0x03, "Generic modem" },
	{ 0x07, 0x04, "IEEE 488.1/2 (GPIB) controller" },
	{ 0x07, 0x05, "Smart card" },
	{ 0x07, 0x80, "Communications device" },
	{ 0x08, 0x00, "PIC/APIC" },
	{ 0x08, 0x01, "DMA controller" },
	{ 0x08, 0x02, "System timer" },
	{ 0x08, 0x03, "RTC controller" },
	{ 0x08, 0x04, "Generic PCI hot-plug controller" },
	{ 0x08, 0x80, "System peripheral" },
	{ 0x09, 0x00, "Keyboard controller" },
	{ 0x09, 0x01, "Digitizer" },
	{ 0x09, 0x02, "Mouse controller" },
	{ 0x09, 0x03, "Scanner controller" },
	{ 0x09, 0x04, "Gameport controller" },
	{ 0x09, 0x80, "Input controller" },
	{ 0x0a, 0x00, "Generic Docking station" },
	{ 0x0a, 0x80, "Docking station" },
	{ 0x0b, 0x00, "386 processor" },
	{ 0x0b, 0x01, "486 processor" },
	{ 0x0b, 0x02, "Pentium processor" },
	{ 0x0b, 0x10, "Alpha processor" },
	{ 0x0b, 0x20, "PowerPC processor" },
	{ 0x0b, 0x30, "MIPS processor" },
	{ 0x0b, 0x40, "Co-processor" },
	{ 0x0c, 0x00, "IEEE 1394 controller" },
	{ 0x0c, 0x01, "ACCESS.bus" },
	{ 0x0c, 0x02, "SSA" },
	{ 0x0c, 0x03, "USB" },
	{ 0x0c, 0x04, "Fibre Channel" },
	{ 0x0c, 0x05, "SMBus" },
	{ 0x0c, 0x06, "InfiniBand" },
	{ 0x0c, 0x07, "IPMI" },
	{ 0x0c, 0x08, "SERCOS Interface Standard (IEC 61491)" },
	{ 0x0c, 0x09, "CANbus" },
	{ 0x0d, 0x00, "IRDA compatible controller" },
	{ 0x0d, 0x01, "Consumer IR controller" },
	{ 0x0d, 0x10, "RF controller" },
	{ 0x0d, 0x11, "Bluetooth controller" },
	{ 0x0d, 0x20, "Ethernet controller (802.11a)" },
	{ 0x0d, 0x21, "Ethernet controller (802.11b)" },
	{ 0x0d, 0x80, "Wireless controller" },
	{ 0x0e, 0x00, "I2O architecture / message FIFO" },
	{ 0x0f, 0x01, "TV controller" },
	{ 0x0f, 0x02, "Audio controller" },
	{ 0x0f, 0x03, "Voice controller" },
	{ 0x0f, 0x04, "Data controller" },
	{ 0x10, 0x00, "Network and computing encryption/decryption" },
	{ 0x10, 0x10, "Entertainment encryption/decryption" },
	{ 0x10, 0x80, "Encryption/decryption" },
	{ 0x11, 0x00, "DPIO modules" },
	{ 0x11, 0x01, "Performance counters" },
	{ 0x11, 0x10, "Communications synchronization" },
	{ 0x11, 0x20, "Management card" },
	{ 0x11, 0x80, "Data acquisition/signal processing controller" },
	{ 0x00, 0x00, NULL },
};

/*
 *  fwts_pci_description()
 *	turn PCI class code and subclass to a human description
 */
const char *fwts_pci_description(const uint8_t class_code, const uint8_t subclass_code)
{
	int i;

	for (i = 0; descriptions[i].description; i++)
		if (class_code == descriptions[i].class_code &&
		    subclass_code == descriptions[i].subclass_code)
			return descriptions[i].description;

	return "Unknown";
}
