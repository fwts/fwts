/*
 * Copyright (C) 2014-2026 Canonical
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

#if defined(FWTS_HAS_UEFI)

#include <inttypes.h>
#include <stddef.h>
#include <ctype.h>

#include "fwts_uefi.h"

static int errors;

static inline bool uefibootpath_check_pnpid(uint32_t id)
{
	return (id & 0xFFFF) == 0x41D0;
}

static int uefibootpath_init(fwts_framework *fw)
{
	if (fw->firmware_type != FWTS_FIRMWARE_UEFI) {
		fwts_log_info(fw, "Cannot detect any UEFI firmware. Aborted.");
		return FWTS_ABORTED;
	}

	return FWTS_OK;
}

static int uefibootpath_check_dev_path(fwts_framework *fw, fwts_uefi_dev_path *dev_path, const size_t dev_path_len)
{
	uint16_t len;

	len = dev_path->length[0] | (((uint16_t)dev_path->length[1]) << 8);

	switch (dev_path->type & 0x7f) {
	case FWTS_UEFI_END_DEV_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_END_ENTIRE_DEV_PATH_SUBTYPE:
		case FWTS_UEFI_END_THIS_DEV_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIEndHwDevPathLength",
					"The length of End of Hardware Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_dev_path));
				errors++;
			}
			break;
		default:
			fwts_log_info_verbatim(fw, "Unknown subtype of End of Hardware Device Path.");
			break;
		}
		break;

	case FWTS_UEFI_HARDWARE_DEV_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_PCI_DEV_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_pci_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIPCIDevPathLength",
					"The length of PCI Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_pci_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_PCCARD_DEV_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_pccard_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIPCCARDDevPathLength",
					"The length of PCCARD Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_pccard_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_MEMORY_MAPPED_DEV_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_mem_mapped_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIMemoryMapDevPathLength",
					"The length of Memory Mapped Device Path  is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_mem_mapped_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_VENDOR_DEV_PATH_SUBTYPE:
			if (len < (sizeof(fwts_uefi_vendor_dev_path) - sizeof(uint8_t))) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVendorDevPathLength",
					"The length of Vendor Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)(sizeof(fwts_uefi_vendor_dev_path) - sizeof(uint8_t)));
				errors++;
			}
			break;
		case FWTS_UEFI_CONTROLLER_DEV_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_controller_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIControllerDevPathLength",
					"The length of Controller Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_controller_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_BMC_DEV_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_bmc_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBMCDevPathLength",
					"The length of BMC Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_bmc_dev_path));
				errors++;
			}

			fwts_uefi_bmc_dev_path *b = (fwts_uefi_bmc_dev_path *)dev_path;
			if (b->interface_type > 3) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBMCDevPathIntfTypeInvalid",
					"The definition on interface type of BMC Device Path is 0-3"
					", which is out of the defined range.");
				errors++;
			}
			break;
		default:
			fwts_log_info_verbatim(fw, "Unknown subtype of Hardware Device Path.");
			break;
		}
		break;

	case FWTS_UEFI_ACPI_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_ACPI_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_acpi_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIACPIDevPathLength",
					"The length of ACPI Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_acpi_dev_path));
				errors++;
			}

			fwts_uefi_acpi_dev_path *a = (fwts_uefi_acpi_dev_path *)dev_path;
			if (!uefibootpath_check_pnpid(a->hid)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIACPIDevPathPnPId",
					"The ID value 0x%" PRIx32 " is not complied with "
					"the compressed EISA-type ID.",
					a->hid);
				errors++;
			}
			break;
		case FWTS_UEFI_EXPANDED_ACPI_DEVICE_PATH_SUBTYPE:
			if (len < (sizeof(fwts_uefi_expanded_acpi_dev_path) + 2*sizeof(char))) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIExpACPIDevPathLength",
					"The length of Expanded ACPI Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)(sizeof(fwts_uefi_expanded_acpi_dev_path) + 2*sizeof(char)));
				errors++;
			}

			fwts_uefi_expanded_acpi_dev_path *e = (fwts_uefi_expanded_acpi_dev_path *)dev_path;
			if (!uefibootpath_check_pnpid(e->hid)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIExpACPIDevPathPnPId",
					"The ID value 0x%" PRIx32 " is not complied with "
					"the compressed EISA-type ID.",
					e->hid);
				errors++;
			}

			if (!uefibootpath_check_pnpid(e->cid)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIExpACPIDevPathPnPId",
					"The ID value 0x%" PRIx32 " is not complied with "
					"the compressed EISA-type ID.",
					e->cid);
				errors++;
			}
			break;
		case FWTS_UEFI_ACPI_ADR_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_acpi_adr_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIADRDevPathLength",
					"The length of _ADR Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_acpi_adr_dev_path));
				errors++;
			}
			break;
		default:
			fwts_log_info_verbatim(fw, "Unknown subtype of ACPI Device Path.");
			break;
		}
		break;

	case FWTS_UEFI_MESSAGING_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_ATAPI_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_atapi_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIATAPIDevPathLength",
					"The length of ATAPI Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_atapi_dev_path));
				errors++;
				break;
			}

			fwts_uefi_atapi_dev_path *a = (fwts_uefi_atapi_dev_path *)dev_path;
			if (a->primary_secondary > 1) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIATAPIDevPathConfInvalid",
					"The PrimarySecondary value of ATAPI Device Path is %" PRIu8
					" which is out of configuration range.",
					a->primary_secondary);
				errors++;
			}
			if (a->slave_master > 1) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIATAPIDevPathConfInvalid",
					"The SlaveMaster value of ATAPI Device Path is %" PRIu8
					" which is out of configuration range.",
					a->slave_master);
				errors++;
			}
			break;
		case FWTS_UEFI_SCSI_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_scsi_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFISCSIDevPathLength",
					"The length of SCSI Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_scsi_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_FIBRE_CHANNEL_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_fibre_channel_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIFibreChannelDevPathLength",
					"The length of Fibre Channel Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_fibre_channel_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_1394_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_1394_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFI1394DevPathLength",
					"The length of 1394 Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_1394_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_USB_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_usb_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUSBDevPathLength",
					"The length of USB Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_usb_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_USB_CLASS_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_usb_class_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUSBClassDevPathLength",
					"The length of USB Class Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_usb_class_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_I2O_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_i2o_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFII2ODevPathLength",
					"The length of i2o Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_i2o_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_MAC_ADDRESS_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_mac_addr_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIMACAddrDevPathLength",
					"The length of MAC Address Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_mac_addr_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_IPV4_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_ipv4_dev_path_v2) && len != sizeof(fwts_uefi_ipv4_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIIPv4DevPathLength",
					"The length of IPv4 Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_ipv4_dev_path));
				errors++;
				break;
			}

			fwts_uefi_ipv4_dev_path *i4 = (fwts_uefi_ipv4_dev_path *)dev_path;
			if (i4->static_ip_address > 1) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIIPv4DevPathConfInvalid",
					"The StaticIPAddress value of IPv4 Device Path is %" PRIu8
					" which is out of configuration range.",
					i4->static_ip_address);
				errors++;
			}
			break;
		case FWTS_UEFI_IPV6_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_ipv6_dev_path_v2) && len != sizeof(fwts_uefi_ipv6_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIIPv6DevPathLength",
					"The length of IPv6 Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_ipv6_dev_path));
				errors++;
				break;
			}

			fwts_uefi_ipv6_dev_path *i6 = (fwts_uefi_ipv6_dev_path *)dev_path;
			if (i6->static_ip_address > 2) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIIPV6DevPathConfInvalid",
					"The IPAddressOrigin value of IPv6 Device Path is %" PRIu8
					" which is out of configuration range.",
					i6->static_ip_address);
				errors++;
			}
			break;
		case FWTS_UEFI_INFINIBAND_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_infiniband_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIInfiniBandDevPathLength",
					"The length of InfiniBand Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_infiniband_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_UART_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_uart_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUARTDevPathLength",
					"The length of UART Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_uart_dev_path));
				errors++;
				break;
			}

			fwts_uefi_uart_dev_path *u = (fwts_uefi_uart_dev_path *)dev_path;
			if (u->parity > 5) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUARTDevPathConfInvalid",
					"The Parity value of UART Device Path is %" PRIu8
					" which is out of configuration range.",
					u->parity);
				errors++;
			}
			if (u->stop_bits > 3) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUARTDevPathConfInvalid",
					"The Stop Bits value of UART Device Path is %" PRIu8
					" which is out of configuration range.",
					u->stop_bits);
				errors++;
			}
			break;
		case FWTS_UEFI_VENDOR_MESSAGING_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_vendor_messaging_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVendorMessagingDevPathLength",
					"The length of Vendor-Defined Messaging Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_vendor_messaging_dev_path));
				errors++;
			}

			size_t i;
			static const fwts_uefi_guid guid[] = {
				EFI_PC_ANSI_GUID,
				EFI_VT_100_GUID,
				EFI_VT_100_PLUS_GUID,
				EFI_VT_UTF8_GUID,
				EFI_UART_DEVICE_PATH_GUID,
				EFI_SAS_DEVICE_PATH_GUID
			};

			fwts_uefi_vendor_messaging_dev_path *v = (fwts_uefi_vendor_messaging_dev_path *)dev_path;
			for (i = 0; i < sizeof(guid)/sizeof(guid[0]); i++)
				if (memcmp(&v->guid, &guid[i], sizeof(fwts_uefi_guid)) == 0)
					break;
			switch (i) {
			case 0:
			case 1:
			case 2:
			case 3:
				if (len != sizeof(fwts_uefi_vendor_messaging_dev_path)) {
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVendorMessagingDevPathLength",
						"The length of Vendor-Defined Messaging Device Path is %" PRIu16 " bytes "
						"and differs from UEFI specification defined %" PRIu16 " bytes.",
						len,
						(uint16_t)sizeof(fwts_uefi_vendor_messaging_dev_path));
					errors++;
				}
				break;
			case 4:
				if (len != (sizeof(fwts_uefi_uart_flowctl_messaging_dev_path))) {
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVendorMessagingDevPathLength",
						"The length of UART Flow Control Device Path is %" PRIu16 " bytes "
						"and differs from UEFI specification defined %" PRIu16 " bytes.",
						len,
						(uint16_t)(sizeof(fwts_uefi_uart_flowctl_messaging_dev_path)));
					errors++;
				}
				break;
			case 5:
				if (len != (sizeof(fwts_uefi_sas_messaging_dev_path))) {
					fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVendorMessagingDevPathLength",
						"The length of Serial Attached SCSI (SAS) Device Path is %" PRIu16 " bytes "
						"and differs from UEFI specification defined %" PRIu16 " bytes.",
						len,
						(uint16_t)(sizeof(fwts_uefi_sas_messaging_dev_path)));
					errors++;
				}
				break;
			}
			break;
		case FWTS_UEFI_FIBRE_CHANNEL_EX_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_fibre_channel_ex_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIFibreChannelExDevPathLength",
					"The length of Fibre Channel Ex Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_fibre_channel_ex_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_SATA_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_sata_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFISATADevPathLength",
					"The length of SATA Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_sata_dev_path));
				errors++;
				break;
			}

			fwts_uefi_sata_dev_path *s = (fwts_uefi_sata_dev_path *)dev_path;
			if (s->hbapn == 0xFFFF) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFISATADevPathConfInvalid",
					"The HBA Port Number value of length of SATA Device Path is %" PRIu16
					" which is reserved.",
					s->hbapn);
				errors++;
			}
			break;
		case FWTS_UEFI_USB_WWID_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_usb_wwid_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUSBWWIDDevPathLength",
					"The length of USB WWID Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_usb_wwid_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_VLAN_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_vlan_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVLANDevPathLength",
					"The length of VLAN Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_vlan_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_LOGICAL_UNIT_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_logical_unit_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIDeviceLogicalUnitLength",
					"The length of Device Logical Unit is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_logical_unit_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_SAS_EX_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_sas_ex_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFISASExDevPathLength",
					"The length of Serial Attached SCSI Ex Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_sas_ex_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_ISCSI_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_iscsi_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIiSCSIDevPathLength",
					"The length of iSCSI Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_iscsi_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_NVM_EXPRESS_NAMESP_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_nvm_express_namespace_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFINVMExpressDevPathLength",
					"The length of NVM Express Namespace Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_nvm_express_namespace_dev_path));
				errors++;
				break;
			}

			fwts_uefi_nvm_express_namespace_dev_path *n = (fwts_uefi_nvm_express_namespace_dev_path *)dev_path;
			if ((n->namesp_id == 0xFFFFFFFF) || (n->namesp_id == 0)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFINVMExpressDevPathInvalid",
					"The Namespace Identifier value of NVM Express namespace Device Path is %" PRIu32
					" which is invalid.",
					n->namesp_id);
				errors++;
			}
			break;
		case FWTS_UEFI_URI_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_uri_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIURIDevPathLength",
					"The length of URI Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_uri_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_UFS_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_ufs_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIUFSDevPathLength",
					"The length of UFS(Universal Flash Storage) is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_ufs_dev_path));
				errors++;
			}

			fwts_uefi_ufs_dev_path *ufs = (fwts_uefi_ufs_dev_path *)dev_path;
			if (ufs->target_id != 0) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIPUNFieldInvalid",
					"The Target ID on the UFS interface(PUN) is %" PRIu8 " ."
					"This value should be 0 for current UFS2.0 spec compliance "
					"and reserve/introduce this field to support multiple devices "
					"per UFS port.",
					ufs->target_id);
				errors++;
			}
			break;
		case FWTS_UEFI_SD_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_sd_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFISDDevPathLength",
					"The length of SD device path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_sd_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_BLUETOOTH_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_bluetooth_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBluetoothDevPathLength",
					"The length of Bluetooth device path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_bluetooth_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_WIRELESS_DEVICE_PATH_SUBTYPE:
			if (len <= sizeof(fwts_uefi_wireless_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIWirelessDevPathLength",
					"The length of wireless Device Path is %" PRIu16 " bytes, "
					"should at least include the SSID filed.",
					len);
				errors++;
			}
			if (len > (sizeof(fwts_uefi_wireless_dev_path) + 32)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIWirelessDevPathLength",
					"The length of wireless Device Path is %" PRIu16 " bytes, "
					"the ssid field should not be larger than 32 bytes.",
					len);
				errors++;
			}
			break;
		case FWTS_UEFI_EMMC_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_emmc_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIEMMCDevPathLength",
					"The length of eMMC device path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_emmc_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_BLUETOOTHLE_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_bluetoothle_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBTLEDevPathLength",
					"The length of BluetoothLE device path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_bluetoothle_dev_path));
				errors++;
			}

			fwts_uefi_bluetoothle_dev_path *b = (fwts_uefi_bluetoothle_dev_path *)dev_path;
			if (b->addr_type > 1) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBTLEDevPathTypeInvalid",
					"The Address Type of BluetoothLE Device Path is %" PRIu8
					" which is out of UEFI specification defined range 0 or 1.",
					b->addr_type);
				errors++;
			}
			break;
		case FWTS_UEFI_DNS_DEVICE_PATH_SUBTYPE:
			if (len <= sizeof(fwts_uefi_dns_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIDNSDevPathLength",
					"The length of DNS device path is %" PRIu16 " bytes, "
					"it should also include one or more instances of the DNS server "
					"address.", len);
				errors++;
			}

			fwts_uefi_dns_dev_path *d = (fwts_uefi_dns_dev_path *)dev_path;
			if (d->isipv6 > 1) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIDNSDevPathTypeInvalid",
					"The Ip type of DNS Device Path is %" PRIu8
					" which is out of UEFI specification defined range 0 or 1.",
					d->isipv6);
				errors++;
			}
			break;
		default:
			fwts_log_info_verbatim(fw, "Unknown subtype of Messaging Device Path.");
			break;
		}
		break;

	case FWTS_UEFI_MEDIA_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_hard_drive_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIHardDriveDevPathLength",
					"The length of Hard Drive Media Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_hard_drive_dev_path));
				errors++;
				break;
			}

			fwts_uefi_hard_drive_dev_path *h = (fwts_uefi_hard_drive_dev_path *)dev_path;
			if ((h->mbr_type > 2) || (h->mbr_type == 0)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIHardDriveDevPathConfInvalid",
					"The Partition Format value of Hard Drive Media Device Path is %" PRIu8
					" which is reserved.",
					h->mbr_type);
				errors++;
			}
			if (h->signature_type > 2) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIHardDriveDevPathConfInvalid",
					"The Signature Type value of Hard Drive Media Device Path is %" PRIu8
					" which is reserved.",
					h->signature_type);
				errors++;
			}
			break;
		case FWTS_UEFI_CDROM_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_uefi_cdrom_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFICDROMDevPathLength",
					"The length of CD-ROM Media Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_cdrom_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_VENDOR_MEDIA_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_vendor_media_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIVendorMediaDevPathLength",
					"The length of Vendor-Defined Media Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_vendor_media_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_FILE_PATH_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_file_path_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIFilePathMediaDevPathLength",
					"The length of File Path Media Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_file_path_dev_path));
				errors++;
				break;
			}
			fwts_uefi_file_path_dev_path *f = (fwts_uefi_file_path_dev_path *)dev_path;
			if (len != (sizeof(fwts_uefi_file_path_dev_path) + ((fwts_uefi_str16len(f->path_name) + 1) * sizeof(uint16_t)))) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIFilePathMediaPathLength",
					"The length of File Path Media Device Path is %" PRIu16 " bytes "
					"is not matching with adding the length of Path String %" PRIu16 " bytes.",
					len,
					(uint16_t)(sizeof(fwts_uefi_file_path_dev_path) + ((fwts_uefi_str16len(f->path_name) + 1) * sizeof(uint16_t))));
				errors++;
				break;
			}
			break;
		case FWTS_UEFI_PROTOCOL_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_media_protocol_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIMediaProtocolDevPathLength",
					"The length of Media Protocol Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_media_protocol_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_PIWG_FW_FILE_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_piwg_fw_file_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIPIWGFwFileDevPathLength",
					"The length of PIWG Firmware File Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_piwg_fw_file_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_PIWG_FW_VOLUME_DEVICE_PATH_SUBTYPE:
			if (len != sizeof(fwts_piwg_fw_volume_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIPIWGFwVolumeDevPathLength",
					"The length of PIWG Firmware Volume Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_piwg_fw_volume_dev_path));
				errors++;
			}
			break;
		case FWTS_UEFI_RELATIVE_OFFSET_RANGE_SUBTYPE:
			if (len != sizeof(fwts_relative_offset_range_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIRelativeOffsetRangeLength",
					"The length of Relative Offset Range is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_relative_offset_range_path));
				errors++;
			}
			break;
		case FWTS_UEFI_RAM_DISK_SUBTYPE:
			if (len != sizeof(fwts_ram_disk_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIRamDiskDevPathLength",
					"The length of Ram Disk Device Path is %" PRIu16 " bytes "
					"and differs from UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_ram_disk_path));
				errors++;
			}
			break;
		default:
			fwts_log_info_verbatim(fw, "Unknown subtype of Media Device Path.");
			break;
		}
		break;

	case FWTS_UEFI_BIOS_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_BIOS_DEVICE_PATH_SUBTYPE:
			if (len < sizeof(fwts_uefi_bios_dev_path)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBiosBootDevPathLength",
					"The length of BIOS Boot Specification Device Path is %" PRIu16 " bytes "
					"should not be smaller than UEFI specification defined %" PRIu16 " bytes.",
					len,
					(uint16_t)sizeof(fwts_uefi_bios_dev_path));
				errors++;
				break;
			}
			fwts_uefi_bios_dev_path *b = (fwts_uefi_bios_dev_path *)dev_path;
			if (len != (sizeof(fwts_uefi_bios_dev_path) + strlen(b->description) + 1)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "UEFIBiosBootDevPathLength",
					"The length of BIOS Boot Specification Device Path is %" PRIu16 " bytes "
					"is not matching with adding the length of Description String %" PRIu16 " bytes.",
					len,
					(uint16_t)(sizeof(fwts_uefi_bios_dev_path) + strlen(b->description) + 1));
				errors++;
			}
			break;
		default:
			fwts_log_info_verbatim(fw, "Unknown subtype of BIOS Boot Specification Device Path.");
			break;
		}
		break;
	default:
		fwts_log_info_verbatim(fw, "Unknown Type of Device Path.");
		break;

	}

	/* Not end? - collect more */
	if (!((dev_path->type & 0x7f) == (FWTS_UEFI_END_DEV_PATH_TYPE) &&
	      (dev_path->subtype == FWTS_UEFI_END_ENTIRE_DEV_PATH_SUBTYPE))) {
		len = dev_path->length[0] | (((uint16_t)dev_path->length[1]) << 8);
		if (len > 0) {
			dev_path = (fwts_uefi_dev_path *)((char *)dev_path + len);
			uefibootpath_check_dev_path(fw, dev_path, dev_path_len - len);
		}
	}

	return FWTS_OK;
}

static int uefibootpath_info_bootdev(fwts_framework *fw, fwts_uefi_var *var)
{
	fwts_uefi_load_option *load_option;
	size_t len, offset;
	char tmp[2048];

	if (var->datalen < sizeof(fwts_uefi_load_option)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFIBootPathLength",
			"The length of boot path variable is less than "
			"the load option structure.");
		return FWTS_ERROR;
	}

	load_option = (fwts_uefi_load_option *)var->data;
	fwts_uefi_str16_to_str(tmp, sizeof(tmp), load_option->description);
	len = fwts_uefi_str16len(load_option->description);
	fwts_log_info_verbatim(fw, "Info: %s\n", tmp);

	if (var->datalen < (sizeof(uint16_t) * (len + 1))) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFIBootPathDescriptionLength",
			"The length of description is large than "
			"the boot path variable length.");
		return FWTS_ERROR;
	}

	/* Skip over description */
	offset = sizeof(load_option->attributes) +
		 sizeof(load_option->file_path_list_length) +
		 (sizeof(uint16_t) * (len + 1));

	uefibootpath_check_dev_path(fw, (fwts_uefi_dev_path *)(var->data + offset), var->datalen - offset);

	return FWTS_OK;

}

static void uefibootpath_var(fwts_framework *fw, fwts_uefi_var *var)
{
	char varname[512];

	fwts_uefi_get_varname(varname, sizeof(varname), var);

	/* Check the boot load option Boot####. #### is a printed hex value */
	if ((strlen(varname) == 8) && (strncmp(varname, "Boot", 4) == 0)
			&& isxdigit(varname[4]) && isxdigit(varname[5])
			&& isxdigit(varname[6]) && isxdigit(varname[7])) {
		int ret;

		fwts_log_info_verbatim(fw, "Name: %s", varname);
		errors = 0;
		ret = uefibootpath_info_bootdev(fw, var);

		if (!errors && (ret == FWTS_OK))
			fwts_passed(fw, "Check bootpath %s test passed.", varname);

		return;
	}

}

static int uefibootpath_test1(fwts_framework *fw)
{
	fwts_list name_list;

	if (fwts_uefi_get_variable_names(&name_list) == FWTS_ERROR) {
		fwts_skipped(fw, "Cannot find any UEFI variables.");
		return FWTS_SKIP;
	} else {
		fwts_list_link *item;

		fwts_list_foreach(item, &name_list) {
			fwts_uefi_var var;
			char *name = fwts_list_data(char *, item);

			if (fwts_uefi_get_variable(name, &var) == FWTS_OK) {
				uefibootpath_var(fw, &var);
				fwts_uefi_free_variable(&var);
			}
		}
	}

	fwts_uefi_free_variable_names(&name_list);

	return FWTS_OK;
}

static fwts_framework_minor_test uefibootpath_tests[] = {
	{ uefibootpath_test1, "Test UEFI Boot Path Boot####." },
	{ NULL, NULL }
};

static fwts_framework_ops uefibootpath_ops = {
	.description     = "Sanity check for UEFI Boot Path Boot####.",
	.init            = uefibootpath_init,
	.minor_tests     = uefibootpath_tests
};

FWTS_REGISTER("uefibootpath", &uefibootpath_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_XBBR);

#endif
