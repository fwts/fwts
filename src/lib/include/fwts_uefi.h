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

#ifndef __FWTS_UEFI_H__
#define __FWTS_UEFI_H__

PRAGMA_PUSH
PRAGMA_PACK_WARN_OFF

#define FWTS_UEFI_LOAD_OPTION_ACTIVE 		0x00000001
#define FWTS_UEFI_LOAD_OPTION_FORCE_RECONNECT 	0x00000002

#define EFI_GLOBAL_VARIABLE \
{ \
	0x8BE4DF61, 0x93CA, 0x11d2, { 0xAA, 0x0D, 0x00, \
						0xE0, 0x98, 0x03, 0x2B, 0x8C} \
}

typedef struct {
	uint16_t	*varname;
	uint8_t		guid[16];
	size_t		datalen;
	uint8_t		*data;
	uint64_t	status;
	uint32_t	attributes;
} fwts_uefi_var;

typedef uint8_t  fwts_uefi_mac_addr[32];
typedef uint8_t  fwts_uefi_ipv4_addr[4];
typedef uint16_t fwts_uefi_ipv6_addr[8];

enum {
	FWTS_UEFI_VAR_NON_VOLATILE =					0x00000001,
	FWTS_UEFI_VAR_BOOTSERVICE_ACCESS =				0x00000002,
	FWTS_UEFI_VAR_RUNTIME_ACCESS =					0x00000004,
	FWTS_UEFI_VARIABLE_HARDWARE_ERROR_RECORD =			0x00000008,
	FWTS_UEFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS =			0x00000010,
	FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS =	0x00000020,
	FWTS_UEFI_VARIABLE_APPEND_WRITE =				0x00000040,
	FWTS_UEFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS =		0x00000080
};

enum {
	FWTS_UEFI_TIME_ADJUST_DAYLIGHT =	0x01,
	FWTS_UEFI_TIME_IN_DAYLIGHT = 		0x02
};

#define BITS_PER_LONG	(sizeof(long) * 8)

#define HIGH_BIT_SET	(1UL << (BITS_PER_LONG-1))

#define EFI_SUCCESS			0
#define EFI_LOAD_ERROR			(1 | HIGH_BIT_SET)
#define EFI_INVALID_PARAMETER		(2 | HIGH_BIT_SET)
#define EFI_UNSUPPORTED			(3 | HIGH_BIT_SET)
#define EFI_BAD_BUFFER_SIZE 		(4 | HIGH_BIT_SET)
#define EFI_BUFFER_TOO_SMALL		(5 | HIGH_BIT_SET)
#define EFI_NOT_READY			(6 | HIGH_BIT_SET)
#define EFI_DEVICE_ERROR		(7 | HIGH_BIT_SET)
#define EFI_WRITE_PROTECTED		(8 | HIGH_BIT_SET)
#define EFI_OUT_OF_RESOURCES		(9 | HIGH_BIT_SET)
#define EFI_VOLUME_CORRUPTED		(10 | HIGH_BIT_SET)
#define EFI_VOLUME_FULL			(11 | HIGH_BIT_SET)
#define EFI_NO_MEDIA			(12 | HIGH_BIT_SET)
#define EFI_MEDIA_CHANGED		(13 | HIGH_BIT_SET)
#define EFI_NOT_FOUND			(14 | HIGH_BIT_SET)
#define EFI_ACCESS_DENIED		(15 | HIGH_BIT_SET)
#define EFI_NO_RESPONSE			(16 | HIGH_BIT_SET)
#define EFI_NO_MAPPING			(17 | HIGH_BIT_SET)
#define EFI_TIMEOUT			(18 | HIGH_BIT_SET)
#define EFI_NOT_STARTED			(19 | HIGH_BIT_SET)
#define EFI_ALREADY_STARTED		(20 | HIGH_BIT_SET)
#define EFI_ABORTED			(21 | HIGH_BIT_SET)
#define EFI_ICMP_ERROR			(22 | HIGH_BIT_SET)
#define EFI_TFTP_ERROR			(23 | HIGH_BIT_SET)
#define EFI_PROTOCOL_ERROR		(24 | HIGH_BIT_SET)
#define EFI_INCOMPATIBLE_VERSION	(25 | HIGH_BIT_SET)
#define EFI_SECURITY_VIOLATION		(26 | HIGH_BIT_SET)
#define EFI_CRC_ERROR			(27 | HIGH_BIT_SET)
#define EFI_END_OF_MEDIA		(28 | HIGH_BIT_SET)
#define EFI_END_OF_FILE			(31 | HIGH_BIT_SET)
#define EFI_INVALID_LANGUAGE		(32 | HIGH_BIT_SET)
#define EFI_COMPROMISED_DATA		(33 | HIGH_BIT_SET)
#define EFI_IP_ADDRESS_CONFLICT		(34 | HIGH_BIT_SET)
#define EFI_HTTP_ERROR			(35 | HIGH_BIT_SET)

#define FWTS_UEFI_UNSPECIFIED_TIMEZONE 0x07FF

#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI			0x0000000000000001
#define EFI_OS_INDICATIONS_TIMESTAMP_REVOCATION 		0x0000000000000002
#define EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED	0x0000000000000004
#define EFI_OS_INDICATIONS_FMP_CAPSULE_SUPPORTED 		0x0000000000000008
#define EFI_OS_INDICATIONS_CAPSULE_RESULT_VAR_SUPPORTED		0x0000000000000010

#define ESRT_FW_TYPE_UNKNOWN		0x00000000
#define ESRT_FW_TYPE_SYSTEMFIRMWARE	0x00000001
#define ESRT_FW_TYPE_DEVICEFIRMWARE	0x00000002
#define ESRT_FW_TYPE_UEFIDRIVER		0x00000003

#define LAST_ATTEMPT_STATUS_SUCCESS				0x00000000
#define LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL			0x00000001
#define LAST_ATTEMPT_STATUS_ERR_INSUFFICIENT_RESOURCES		0x00000002
#define LAST_ATTEMPT_STATUS_ERR_INCORRECT_VERSION		0x00000003
#define LAST_ATTEMPT_STATUS_ERR_INVALID_FORMAT			0x00000004
#define LAST_ATTEMPT_STATUS_ERR_AUTH_ERROR			0x00000005
#define LAST_ATTEMPT_STATUS_ERR_PWR_EVT_AC			0x00000006
#define LAST_ATTEMPT_STATUS_ERR_PWR_EVT_BATT			0x00000007
#define LAST_ATTEMPT_STATUS_ERR_UNSATISFIED_DEPENDENCIES	0x00000008
#define LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL_VENDOR_RANGE_MIN	0x00001000
#define LAST_ATTEMPT_STATUS_ERR_UNSUCCESSFUL_VENDOR_RANGE_MAX	0x00004000


#define EFI_RT_SUPPORTED_GET_TIME			0x0001
#define EFI_RT_SUPPORTED_SET_TIME			0x0002
#define EFI_RT_SUPPORTED_GET_WAKEUP_TIME		0x0004
#define EFI_RT_SUPPORTED_SET_WAKEUP_TIME		0x0008
#define EFI_RT_SUPPORTED_GET_VARIABLE			0x0010
#define EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME		0x0020
#define EFI_RT_SUPPORTED_SET_VARIABLE			0x0040
#define EFI_RT_SUPPORTED_SET_VIRTUAL_ADDRESS_MAP	0x0080
#define EFI_RT_SUPPORTED_CONVERT_POINTER		0x0100
#define EFI_RT_SUPPORTED_GET_NEXT_HIGH_MONOTONIC_COUNT	0x0200
#define EFI_RT_SUPPORTED_RESET_SYSTEM			0x0400
#define EFI_RT_SUPPORTED_UPDATE_CAPSULE			0x0800
#define EFI_RT_SUPPORTED_QUERY_CAPSULE_CAPABILITIES	0x1000
#define EFI_RT_SUPPORTED_QUERY_VARIABLE_INFO		0x2000

#define EFI_RT_SUPPORTED_ALL				0x3fff

#define EFI_CERT_SHA256_GUID \
{ 0xc1c41626, 0x504c, 0x4092, { 0xac, 0xa9, 0x41, 0xf9, 0x36, 0x93, 0x43, 0x28 }}

#define EFI_CERT_RSA2048_GUID \
{ 0x3c5766e8, 0x269c, 0x4e34, { 0xaa, 0x14, 0xed, 0x77, 0x6e, 0x85, 0xb3, 0xb6 }}

#define EFI_CERT_RSA2048_SHA256_GUID \
{ 0xe2b36190, 0x879b, 0x4a3d, { 0xad, 0x8d, 0xf2, 0xe7, 0xbb, 0xa3, 0x27, 0x84 }}

#define EFI_CERT_SHA1_GUID \
{ 0x826ca512, 0xcf10, 0x4ac9, { 0xb1, 0x87, 0xbe, 0x1, 0x49, 0x66, 0x31, 0xbd }}

#define EFI_CERT_RSA2048_SHA1_GUID \
{ 0x67f8444f, 0x8743, 0x48f1, { 0xa3, 0x28, 0x1e, 0xaa, 0xb8, 0x73, 0x60, 0x80 }}

#define EFI_CERT_X509_GUID \
{ 0xa5c059a1, 0x94e4, 0x4aa7, { 0x87, 0xb5, 0xab, 0x15, 0x5c, 0x2b, 0xf0, 0x72 }}

#define EFI_CERT_SHA224_GUID \
{ 0xb6e5233, 0xa65c, 0x44c9, { 0x94, 0x7, 0xd9, 0xab, 0x83, 0xbf, 0xc8, 0xbd }}

#define EFI_CERT_SHA384_GUID \
{ 0xff3e5307, 0x9fd0, 0x48c9, { 0x85, 0xf1, 0x8a, 0xd5, 0x6c, 0x70, 0x1e, 0x1 }}

#define EFI_CERT_SHA512_GUID \
{ 0x93e0fae, 0xa6c4, 0x4f50, { 0x9f, 0x1b, 0xd4, 0x1e, 0x2b, 0x89, 0xc1, 0x9a }}

#define EFI_CERT_X509_SHA256_GUID \
{ 0x3bd2a492, 0x96c0, 0x4079, { 0xb4, 0x20, 0xfc, 0xf9, 0x8e, 0xf1, 0x03, 0xed }}

#define EFI_CERT_X509_SHA384_GUID \
{ 0x7076876e, 0x80c2, 0x4ee6, { 0xaa, 0xd2, 0x28, 0xb3, 0x49, 0xa6, 0x86, 0x5b }}

#define EFI_CERT_X509_SHA512_GUID \
{ 0x446dbf63, 0x2502, 0x4cda, { 0xbc, 0xfa, 0x24, 0x65, 0xd2, 0xb0, 0xfe, 0x9d }}

#define EFI_PC_ANSI_GUID \
{ 0xe0c14753, 0xf9be, 0x11d2, { 0x9a, 0x0c, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d }}

#define EFI_VT_100_GUID \
{ 0xdfa66065, 0xb419, 0x11d3, { 0x9a, 0x2d, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d }}

#define EFI_VT_100_PLUS_GUID \
{ 0x7baec70b, 0x57e0, 0x4c76, { 0x8e, 0x87, 0x2f, 0x9e, 0x28, 0x08, 0x83, 0x43 }}

#define EFI_VT_UTF8_GUID \
{ 0xad15a0d6, 0x8bec, 0x4acf, { 0xa0, 0x73, 0xd0, 0x1d, 0xe7, 0x7e, 0x2d, 0x88 }}

#define EFI_UART_DEVICE_PATH_GUID \
{ 0x37499a9d, 0x542f, 0x4c89, { 0xa0, 0x26, 0x35, 0xda, 0x14, 0x20, 0x94, 0xe4 }}

#define EFI_SAS_DEVICE_PATH_GUID \
{ 0xd487ddb4, 0x008b, 0x11d9, { 0xaf, 0xdc, 0x00, 0x10, 0x83, 0xff, 0xca, 0x4d }}

#if 0
typedef struct {
	char *description;
	uefidump_func	func;
} uefidump_info;
#endif

typedef struct {
	uint8_t type;
  	uint8_t subtype;
  	uint8_t length[2];
} __attribute__ ((packed)) fwts_uefi_dev_path;

typedef struct {
        uint32_t attributes;
        uint16_t file_path_list_length;
        uint16_t description[1];
        fwts_uefi_dev_path unused_file_path_list[1];
} __attribute__((packed)) fwts_uefi_load_option;

typedef struct {
        uint32_t keydata;
        uint32_t bootoptioncrc;
        uint16_t bootoption;
} __attribute__((packed)) fwts_uefi_key_option;

typedef struct {
        uint16_t scancode;
        uint16_t unicodechar;
} __attribute__((packed)) fwts_uefi_input_key;

typedef enum {
	FWTS_UEFI_HARDWARE_DEV_PATH_TYPE =		(0x01),
	FWTS_UEFI_ACPI_DEVICE_PATH_TYPE =		(0x02),
	FWTS_UEFI_MESSAGING_DEVICE_PATH_TYPE =		(0x03),
	FWTS_UEFI_MEDIA_DEVICE_PATH_TYPE =		(0x04),
	FWTS_UEFI_BIOS_DEVICE_PATH_TYPE =		(0x05),
	FWTS_UEFI_END_DEV_PATH_TYPE =			(0x7f)
} dev_path_types;

typedef enum {
	FWTS_UEFI_END_THIS_DEV_PATH_SUBTYPE = 		(0x01),
	FWTS_UEFI_END_ENTIRE_DEV_PATH_SUBTYPE = 	(0xff)
} dev_end_subtypes;

typedef enum {
	FWTS_UEFI_PCI_DEV_PATH_SUBTYPE =		(0x01),
	FWTS_UEFI_PCCARD_DEV_PATH_SUBTYPE =		(0x02),
	FWTS_UEFI_MEMORY_MAPPED_DEV_PATH_SUBTYPE =	(0x03),
	FWTS_UEFI_VENDOR_DEV_PATH_SUBTYPE =		(0x04),
	FWTS_UEFI_CONTROLLER_DEV_PATH_SUBTYPE =		(0x05),
	FWTS_UEFI_BMC_DEV_PATH_SUBTYPE =		(0x06)
} hw_dev_path_subtypes;

typedef enum {
	FWTS_UEFI_ACPI_DEVICE_PATH_SUBTYPE = 		(0x01),
	FWTS_UEFI_EXPANDED_ACPI_DEVICE_PATH_SUBTYPE = 	(0x02),
	FWTS_UEFI_ACPI_ADR_DEVICE_PATH_SUBTYPE = 	(0x03)
} acpi_dev_path_subtypes;

typedef enum {
	FWTS_UEFI_ATAPI_DEVICE_PATH_SUBTYPE = 		(0x01),
	FWTS_UEFI_SCSI_DEVICE_PATH_SUBTYPE = 		(0x02),
	FWTS_UEFI_FIBRE_CHANNEL_DEVICE_PATH_SUBTYPE = 	(0x03),
	FWTS_UEFI_1394_DEVICE_PATH_SUBTYPE = 		(0x04),
	FWTS_UEFI_USB_DEVICE_PATH_SUBTYPE =		(0x05),
	FWTS_UEFI_I2O_DEVICE_PATH_SUBTYPE =		(0x06),
	FWTS_UEFI_INFINIBAND_DEVICE_PATH_SUBTYPE =	(0x09),
	FWTS_UEFI_VENDOR_MESSAGING_DEVICE_PATH_SUBTYPE =(0x0a),
	FWTS_UEFI_MAC_ADDRESS_DEVICE_PATH_SUBTYPE =	(0x0b),
	FWTS_UEFI_IPV4_DEVICE_PATH_SUBTYPE =		(0x0c),
	FWTS_UEFI_IPV6_DEVICE_PATH_SUBTYPE =		(0x0d),
	FWTS_UEFI_UART_DEVICE_PATH_SUBTYPE =		(0x0e),
	FWTS_UEFI_USB_CLASS_DEVICE_PATH_SUBTYPE =	(0x0f),
	FWTS_UEFI_USB_WWID_DEVICE_PATH_SUBTYPE =	(0x10),
	FWTS_UEFI_LOGICAL_UNIT_DEVICE_PATH_SUBTYPE =	(0x11),
	FWTS_UEFI_SATA_DEVICE_PATH_SUBTYPE = 		(0x12),
	FWTS_UEFI_ISCSI_DEVICE_PATH_SUBTYPE = 		(0x13),
	FWTS_UEFI_VLAN_DEVICE_PATH_SUBTYPE = 		(0x14),
	FWTS_UEFI_FIBRE_CHANNEL_EX_DEVICE_PATH_SUBTYPE = (0x15),
	FWTS_UEFI_SAS_EX_DEVICE_PATH_SUBTYPE =		(0x16),
	FWTS_UEFI_NVM_EXPRESS_NAMESP_DEVICE_PATH_SUBTYPE = (0x17),
	FWTS_UEFI_URI_DEVICE_PATH_SUBTYPE = 		(0x18),
	FWTS_UEFI_UFS_DEVICE_PATH_SUBTYPE = 		(0x19),
	FWTS_UEFI_SD_DEVICE_PATH_SUBTYPE = 		(0x1a),
	FWTS_UEFI_BLUETOOTH_DEVICE_PATH_SUBTYPE = 	(0x1b),
	FWTS_UEFI_WIRELESS_DEVICE_PATH_SUBTYPE = 	(0x1c),
	FWTS_UEFI_EMMC_DEVICE_PATH_SUBTYPE =		(0x1d),
	FWTS_UEFI_BLUETOOTHLE_DEVICE_PATH_SUBTYPE =	(0x1e),
	FWTS_UEFI_DNS_DEVICE_PATH_SUBTYPE =		(0x1f)
} messaging_dev_path_subtypes;

typedef enum {
	FWTS_UEFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE =	(0x01),
	FWTS_UEFI_CDROM_DEVICE_PATH_SUBTYPE =		(0x02),
	FWTS_UEFI_VENDOR_MEDIA_DEVICE_PATH_SUBTYPE =	(0x03),
	FWTS_UEFI_FILE_PATH_DEVICE_PATH_SUBTYPE =	(0x04),
	FWTS_UEFI_PROTOCOL_DEVICE_PATH_SUBTYPE =	(0x05),
	FWTS_UEFI_PIWG_FW_FILE_DEVICE_PATH_SUBTYPE =	(0x06),
	FWTS_UEFI_PIWG_FW_VOLUME_DEVICE_PATH_SUBTYPE =	(0x07),
	FWTS_UEFI_RELATIVE_OFFSET_RANGE_SUBTYPE = 	(0x08),
	FWTS_UEFI_RAM_DISK_SUBTYPE =			(0x09)
} media_dev_path_subtypes;

typedef enum {
	FWTS_UEFI_BIOS_DEVICE_PATH_SUBTYPE = 		(0x01)
} bios_dev_path_subtypes;

typedef struct {
	uint32_t info1;
	uint16_t info2;
	uint16_t info3;
	uint8_t  info4[8];
} __attribute__ ((aligned(8))) fwts_uefi_guid;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t function;
	uint8_t device;
} __attribute__ ((packed)) fwts_uefi_pci_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t function;
} __attribute__ ((packed)) fwts_uefi_pccard_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t memory_type;
	uint64_t start_addr;
	uint64_t end_addr;
} __attribute__ ((packed)) fwts_uefi_mem_mapped_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid	guid;
	uint8_t	data[0];
} __attribute__ ((packed)) fwts_uefi_vendor_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t controller;
} __attribute__ ((packed)) fwts_uefi_controller_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t interface_type;
	uint64_t base_addr;
} __attribute__ ((packed)) fwts_uefi_bmc_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t hid;
	uint32_t uid;
} __attribute__((packed)) fwts_uefi_acpi_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t hid;
	uint32_t uid;
	uint32_t cid;
	char hidstr[1];
} __attribute__((packed)) fwts_uefi_expanded_acpi_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t adr;
} __attribute__((packed)) fwts_uefi_acpi_adr_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t primary_secondary;
	uint8_t slave_master;
	uint16_t lun;
} __attribute__((packed)) fwts_uefi_atapi_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t pun;
	uint16_t lun;
} __attribute__((packed)) fwts_uefi_scsi_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t reserved;
	uint64_t wwn;
	uint64_t lun;
} __attribute__((packed)) fwts_uefi_fibre_channel_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t reserved;
	uint64_t guid;
} __attribute__((packed)) fwts_uefi_1394_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t parent_port_number;
	uint8_t interface;
} __attribute__((packed)) fwts_uefi_usb_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t vendor_id;
	uint16_t product_id;
	uint8_t device_class;
	uint8_t device_subclass;
	uint8_t device_protocol;
} __attribute__((packed)) fwts_uefi_usb_class_dev_path;

typedef struct
{
	fwts_uefi_dev_path dev_path;
	uint32_t tid;
} __attribute__((packed)) fwts_uefi_i2o_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
  	fwts_uefi_mac_addr mac_addr;
	uint8_t if_type;
} __attribute__((packed)) fwts_uefi_mac_addr_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
  	fwts_uefi_ipv4_addr local_ip_addr;
  	fwts_uefi_ipv4_addr remote_ip_addr;
  	uint16_t local_port;
  	uint16_t remote_port;
  	uint16_t protocol;
  	uint8_t static_ip_address;
} __attribute__((packed)) fwts_uefi_ipv4_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_ipv4_addr local_ip_addr;
	fwts_uefi_ipv4_addr remote_ip_addr;
	uint16_t local_port;
	uint16_t remote_port;
	uint16_t protocol;
	uint8_t static_ip_address;
	fwts_uefi_ipv4_addr gateway_ip_addr;
	fwts_uefi_ipv4_addr subnet_mask;
} __attribute__((packed)) fwts_uefi_ipv4_dev_path_v2;

typedef struct {
	fwts_uefi_dev_path dev_path;
  	fwts_uefi_ipv6_addr local_ip_addr;
  	fwts_uefi_ipv6_addr remote_ip_addr;
  	uint16_t local_port;
  	uint16_t remote_port;
  	uint16_t protocol;
  	uint8_t static_ip_address;
} __attribute__((packed)) fwts_uefi_ipv6_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
  	fwts_uefi_ipv6_addr local_ip_addr;
  	fwts_uefi_ipv6_addr remote_ip_addr;
  	uint16_t local_port;
  	uint16_t remote_port;
  	uint16_t protocol;
  	uint8_t ip_address_origin;
	uint8_t prefix_length;
  	fwts_uefi_ipv6_addr gateway_ip_addr;
} __attribute__((packed)) fwts_uefi_ipv6_dev_path_v2;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t resource_flags;
	uint8_t port_gid[16];
	uint64_t remote_id;
	uint64_t target_port_id;
	uint64_t device_id;
} __attribute__((packed)) fwts_uefi_infiniband_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t reserved;
	uint64_t baud_rate;
	uint8_t data_bits;
	uint8_t parity;
	uint8_t stop_bits;
} __attribute__((packed)) fwts_uefi_uart_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid guid;
	uint8_t vendor_defined_data[0];
} __attribute__((packed)) fwts_uefi_vendor_messaging_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid guid;
	uint32_t flow_control_map;
} __attribute__((packed)) fwts_uefi_uart_flowctl_messaging_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid guid;
	uint32_t reserved;
	uint64_t sas_addr;
	uint64_t lun;
	uint16_t dev_topology_info;
	uint16_t rtp;
} __attribute__((packed)) fwts_uefi_sas_messaging_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t reserved;
	uint64_t wwn;
	uint64_t lun;
} __attribute__((packed)) fwts_uefi_fibre_channel_ex_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t hbapn;
	uint16_t pmpn;
	uint16_t lun;
} __attribute__((packed)) fwts_uefi_sata_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t interface_num;
	uint16_t vendor_id;
	uint16_t product_id;
	uint16_t serial_number[0];
} __attribute__((packed)) fwts_uefi_usb_wwid_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t lun;
} __attribute__((packed)) fwts_uefi_logical_unit_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t vlanid;
} __attribute__((packed)) fwts_uefi_vlan_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint64_t sas_addr;
	uint64_t lun;
	uint16_t dev_topology_info;
	uint16_t rtp;
} __attribute__((packed)) fwts_uefi_sas_ex_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t protocol;
	uint16_t options;
	uint64_t lun;
	uint16_t tpg_tag;
	char iscsi_tn[0];
} __attribute__((packed)) fwts_uefi_iscsi_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t namesp_id;
	uint64_t ext_unique_id;
} __attribute__((packed)) fwts_uefi_nvm_express_namespace_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	char uri[0];
} __attribute__((packed)) fwts_uefi_uri_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t target_id;
	uint8_t lun;
} __attribute__((packed)) fwts_uefi_ufs_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t slot_number;
} __attribute__((packed)) fwts_uefi_sd_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t bluetooth_addr[6];
} __attribute__((packed)) fwts_uefi_bluetooth_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	char ssid[0];
} __attribute__((packed)) fwts_uefi_wireless_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t slot_num;
} __attribute__((packed)) fwts_uefi_emmc_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t bluetooth_addr[6];
	uint8_t addr_type;
} __attribute__((packed)) fwts_uefi_bluetoothle_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint8_t isipv6;
	uint8_t dns_addr[0];
} __attribute__((packed)) fwts_uefi_dns_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t partition_number;
	uint64_t partition_start;
	uint64_t partition_size;
	uint8_t partition_signature[16];
	uint8_t mbr_type;
	uint8_t signature_type;
} __attribute__((packed)) fwts_uefi_hard_drive_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t boot_entry;
	uint64_t partition_start;
	uint64_t partition_size;
} __attribute__((packed)) fwts_uefi_cdrom_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid guid;
	uint8_t vendor_defined_data[0];
} __attribute__((packed)) fwts_uefi_vendor_media_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t path_name[0];
} __attribute__((packed)) fwts_uefi_file_path_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid protocol_guid;
} __attribute__((packed)) fwts_media_protocol_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid fw_file_name;
} __attribute__((packed)) fwts_piwg_fw_file_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	fwts_uefi_guid fw_volume_name;
} __attribute__((packed)) fwts_piwg_fw_volume_dev_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint32_t reserved;
	uint64_t starting_offset;
	uint64_t ending_offset;
} __attribute__((packed)) fwts_relative_offset_range_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint64_t starting_addr;
	uint64_t ending_addr;
	fwts_uefi_guid disk_type_guid;
	uint16_t disk_instance;
} __attribute__((packed)) fwts_ram_disk_path;

typedef struct {
	fwts_uefi_dev_path dev_path;
	uint16_t device_type;
	uint16_t status_flags;
  	char description[0];
} __attribute__((packed)) fwts_uefi_bios_dev_path;

typedef struct {
	fwts_uefi_guid signaturetype;
	uint32_t signaturelistsize;
	uint32_t signatureheadersize;
	uint32_t signaturesize;
} __attribute__((packed)) fwts_uefi_signature_list;

void fwts_uefi_str16_to_str(char *dst, const size_t len, const uint16_t *src);
size_t fwts_uefi_str16len(const uint16_t *str);
void fwts_uefi_get_varname(char *varname, const size_t len, const fwts_uefi_var *var);
int fwts_uefi_get_variable(const char *varname, fwts_uefi_var *var);
void fwts_uefi_free_variable(fwts_uefi_var *var);
void fwts_uefi_free_variable_names(fwts_list *list);
int fwts_uefi_get_variable_names(fwts_list *list);

void fwts_uefi_print_status_info(fwts_framework *fw, const uint64_t status);
char *fwts_uefi_attribute_info(uint32_t attr);

bool fwts_uefi_efivars_iface_exist(void);

void fwts_uefi_rt_support_status_get(int fd, uint32_t *rtservicessupported);
PRAGMA_POP

#endif
