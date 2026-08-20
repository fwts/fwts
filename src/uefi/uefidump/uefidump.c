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
#include "fwts.h"

#if defined(FWTS_HAS_UEFI)

#include <stddef.h>
#include <inttypes.h>
#include <ctype.h>

#include "fwts_uefi.h"


typedef void (*uefidump_func)(fwts_framework *fw, fwts_uefi_var *var);

typedef struct {
	char *description;		/* UEFI var */
	uefidump_func	func;		/* Function to dump this variable */
} uefidump_info;

static void uefidump_var_hexdump(fwts_framework *fw, fwts_uefi_var *var)
{
	int i;
	uint8_t *data = (uint8_t*)var->data;

	fwts_log_info_verbatim(fw,  "  Size: %zd bytes of data", var->datalen);

	for (i = 0; i < (int)var->datalen; i+= 16) {
		char buffer[128];
		int left = (int)var->datalen - i;

		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw,  "  Data: %s", buffer+2);
	}
}

static void uefidump_data_hexdump(fwts_framework *fw, uint8_t *data, size_t size)
{
	size_t i;

	for (i = 0; i < size; i += 16) {
		char buffer[128];
		size_t left = size - i;

		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw,  "  Data: %s", buffer+2);
	}
}

static char *uefidump_vprintf(char *str, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/*
 *  uefidump_vprintf()
 *	printf() to str: if str NULL - allocate buffer and print into it,
 *	otherwise allocate more space and append new text to end of existing
 *	string.  Return new string, or NULL if failed.
 */
static char *uefidump_vprintf(char *str, const char *fmt, ...)
{
	va_list args;
	char buffer[4096];

	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (str == NULL)
		str = strdup(buffer);
	else {
		char *tmp;
		tmp = realloc(str, strlen(str) + strlen(buffer) + 1);
		if (!tmp) {
			free(str);
			return NULL;
		}
		str = tmp;
		strcat(str, buffer);
	}

	return str;
}

/*
 *  uefidump_build_dev_path()
 *	recursively scan dev_path and build up a human readable path name
 */
static char *uefidump_build_dev_path(char *path, fwts_uefi_dev_path *dev_path, const size_t dev_path_len)
{
	switch (dev_path->type & 0x7f) {
	case FWTS_UEFI_END_DEV_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_END_ENTIRE_DEV_PATH_SUBTYPE:
			break;
		case FWTS_UEFI_END_THIS_DEV_PATH_SUBTYPE:
			path = uefidump_vprintf(path, "\n  Device Path: ");
			break;
		default:
			return uefidump_vprintf(path, "\\Unknown-End(0x%" PRIx8 ")", dev_path->subtype);
		}
		break;
	case FWTS_UEFI_HARDWARE_DEV_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_PCI_DEV_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_pci_dev_path)) {
				fwts_uefi_pci_dev_path *p = (fwts_uefi_pci_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\PCI(0x%" PRIx8 ",0x%" PRIx8 ")",
					p->function, p->device);
			}
			break;
		case FWTS_UEFI_PCCARD_DEV_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_pccard_dev_path)) {
				fwts_uefi_pccard_dev_path *p = (fwts_uefi_pccard_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\PCCARD(0x%" PRIx8 ")",
					p->function);
			}
			break;
		case FWTS_UEFI_MEMORY_MAPPED_DEV_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_mem_mapped_dev_path)) {
				fwts_uefi_mem_mapped_dev_path *m = (fwts_uefi_mem_mapped_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\Memmap(0x%" PRIx32 ",0x%" PRIx64 ",0x%" PRIx64 ")",
					m->memory_type,
					m->start_addr,
					m->end_addr);
			}
			break;
		case FWTS_UEFI_VENDOR_DEV_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_vendor_dev_path)) {
				fwts_uefi_vendor_dev_path *v = (fwts_uefi_vendor_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\VENDOR(%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)",
					v->guid.info1, v->guid.info2, v->guid.info3,
					v->guid.info4[0], v->guid.info4[1], v->guid.info4[2], v->guid.info4[3],
					v->guid.info4[4], v->guid.info4[5], v->guid.info4[6], v->guid.info4[7]);
			}
			break;
		case FWTS_UEFI_CONTROLLER_DEV_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_controller_dev_path)) {
				fwts_uefi_controller_dev_path *c = (fwts_uefi_controller_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\Controller(0x%" PRIx32 ")",
					c->controller);
			}
			break;
		case FWTS_UEFI_BMC_DEV_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_bmc_dev_path)) {
				fwts_uefi_bmc_dev_path *b = (fwts_uefi_bmc_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\BMC(0x%" PRIx8 ",0x%" PRIx64 ")",
					b->interface_type, b->base_addr);
			}
			break;
		default:
			path = uefidump_vprintf(path, "\\Unknown-HW-DEV-PATH(0x%" PRIx8 ")", dev_path->subtype);
			break;
		}
		break;

	case FWTS_UEFI_ACPI_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_ACPI_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_acpi_dev_path)) {
				fwts_uefi_acpi_dev_path *a = (fwts_uefi_acpi_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\ACPI(0x%" PRIx32 ",0x%" PRIx32 ")",
					a->hid, a->uid);
			}
			break;
		case FWTS_UEFI_EXPANDED_ACPI_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_expanded_acpi_dev_path)) {
				fwts_uefi_expanded_acpi_dev_path *a = (fwts_uefi_expanded_acpi_dev_path*)dev_path;
				char *hidstr= a->hidstr;
				path = uefidump_vprintf(path, "\\ACPI(");
				if (hidstr[0] == '\0')
					path = uefidump_vprintf(path, "0x%" PRIx32 ",", a->hid);
				else
					path = uefidump_vprintf(path, "%s,", hidstr);
				hidstr += strlen(hidstr) + 1;
				if (hidstr[0] == '\0')
					path = uefidump_vprintf(path, "0x%" PRIx32 ",", a->uid);
				else
					path = uefidump_vprintf(path, "%s,", hidstr);
				hidstr += strlen(hidstr) + 1;
				if (hidstr[0] == '\0')
					path = uefidump_vprintf(path, "0x%" PRIx32 ",", a->cid);
				else
					path = uefidump_vprintf(path, "%s,", hidstr);
			}
			break;
		case FWTS_UEFI_ACPI_ADR_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_acpi_adr_dev_path)) {
				fwts_uefi_acpi_adr_dev_path *a = (fwts_uefi_acpi_adr_dev_path *)dev_path;
				uint16_t len = a->dev_path.length[0] | (((uint16_t)a->dev_path.length[1]) << 8);
				uint8_t *adr_add = (uint8_t *)a + sizeof(fwts_uefi_acpi_adr_dev_path);
				path = uefidump_vprintf(path, "\\ACPI_ADR(0x%" PRIx32, a->adr);

				/* Adding additional _ADR */
				size_t offset = 0;
				while ((len - sizeof(fwts_uefi_acpi_adr_dev_path) - offset) >= sizeof(uint32_t)) {
					path = uefidump_vprintf(path, ", 0x%" PRIx32 , *(uint32_t *)(adr_add + offset));
					offset += sizeof(uint32_t);
				}

				path = uefidump_vprintf(path, ")");
			}
			break;
		default:
			path = uefidump_vprintf(path, "\\Unknown-ACPI-DEV-PATH(0x%" PRIx8 ")", dev_path->subtype);
			break;
		}
		break;

	case FWTS_UEFI_MESSAGING_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
			case FWTS_UEFI_ATAPI_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_atapi_dev_path)) {
				fwts_uefi_atapi_dev_path *a = (fwts_uefi_atapi_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\ATAPI(0x%" PRIx8 ",0x%" PRIx8 ",0x%" PRIx16 ")",
					a->primary_secondary, a->slave_master, a->lun);
			}
			break;
		case FWTS_UEFI_SCSI_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_scsi_dev_path)) {
				fwts_uefi_scsi_dev_path *s = (fwts_uefi_scsi_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\SCSI(0x%" PRIx16 ",0x%" PRIx16 ")",
					s->pun, s->lun);
			}
			break;
		case FWTS_UEFI_FIBRE_CHANNEL_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_fibre_channel_dev_path)) {
				fwts_uefi_fibre_channel_dev_path *f = (fwts_uefi_fibre_channel_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\FIBRECHANNEL(0x%" PRIx64 ",0x%" PRIx64 ")",
					f->wwn, f->lun);
			}
			break;
		case FWTS_UEFI_1394_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_1394_dev_path)) {
				fwts_uefi_1394_dev_path *fw = (fwts_uefi_1394_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\1394(0x%" PRIx64 ")",
					fw->guid);
			}
			break;
		case FWTS_UEFI_USB_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_usb_dev_path)) {
				fwts_uefi_usb_dev_path *u = (fwts_uefi_usb_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\USB(0x%" PRIx8 ",0x%" PRIx8 ")",
					u->parent_port_number, u->interface);
			}
			break;
		case FWTS_UEFI_USB_CLASS_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_usb_class_dev_path)) {
				fwts_uefi_usb_class_dev_path *u = (fwts_uefi_usb_class_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\USBCLASS(0x%" PRIx16 ",0x%" PRIx16
					",0x%" PRIx8 ",0x%" PRIx8 ",0x%" PRIx8 ")",
					u->vendor_id, u->product_id,
					u->device_class, u->device_subclass,
					u->device_protocol);
			}
			break;
		case FWTS_UEFI_I2O_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_i2o_dev_path)) {
				fwts_uefi_i2o_dev_path *i2o = (fwts_uefi_i2o_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\I2O(0x%" PRIx32 ")", i2o->tid);
			}
			break;
		case FWTS_UEFI_MAC_ADDRESS_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_mac_addr_dev_path)) {
				fwts_uefi_mac_addr_dev_path *m = (fwts_uefi_mac_addr_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\MACADDR(%" PRIx8 ":%" PRIx8 ":%" PRIx8
					":%" PRIx8 ":%" PRIx8 ":%" PRIx8 ",0x%" PRIx8 ")",
					m->mac_addr[0], m->mac_addr[1],
					m->mac_addr[2], m->mac_addr[3],
					m->mac_addr[4], m->mac_addr[5],
					m->if_type);
			}
			break;
		case FWTS_UEFI_IPV4_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_ipv4_dev_path)) {
				fwts_uefi_ipv4_dev_path *i = (fwts_uefi_ipv4_dev_path *)dev_path;
				uint16_t len = i->dev_path.length[0] | (((uint16_t)i->dev_path.length[1]) << 8);
				path = uefidump_vprintf(path, "\\IPv4("
					"%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ","
					"%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ","
					"%" PRIu16 ",%" PRIu16 ",%" PRIx16 ",%" PRIx8 ,
					i->local_ip_addr[0], i->local_ip_addr[1],
					i->local_ip_addr[2], i->local_ip_addr[3],
					i->remote_ip_addr[0], i->remote_ip_addr[1],
					i->remote_ip_addr[2], i->remote_ip_addr[3],
					i->local_port, i->remote_port,
					i->protocol, i->static_ip_address);
				if (len >= sizeof(fwts_uefi_ipv4_dev_path_v2) && dev_path_len >= sizeof(fwts_uefi_ipv4_dev_path_v2)) {
					fwts_uefi_ipv4_dev_path_v2 *iv2 = (fwts_uefi_ipv4_dev_path_v2 *)dev_path;
					path = uefidump_vprintf(path,
						",%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ","
						"%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ,
						iv2->gateway_ip_addr[0], iv2->gateway_ip_addr[1],
						iv2->gateway_ip_addr[2], iv2->gateway_ip_addr[3],
						iv2->subnet_mask[0], iv2->subnet_mask[1],
						iv2->subnet_mask[2], iv2->subnet_mask[3]);
				}
				path = uefidump_vprintf(path, ")");
			}
			break;
		case FWTS_UEFI_IPV6_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_ipv6_dev_path)) {
				fwts_uefi_ipv6_dev_path *i = (fwts_uefi_ipv6_dev_path *)dev_path;
				uint16_t len = i->dev_path.length[0] | (((uint16_t)i->dev_path.length[1]) << 8);
				path = uefidump_vprintf(path, "\\IPv6("
					"%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16
					":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ","
					"%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16
					":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ","
					"%" PRIu16 ",%" PRIu16 ",%" PRIx16 ",%" PRIx8,
					i->local_ip_addr[0], i->local_ip_addr[1],
					i->local_ip_addr[2], i->local_ip_addr[3],
					i->local_ip_addr[4], i->local_ip_addr[5],
					i->local_ip_addr[6], i->local_ip_addr[7],
					i->remote_ip_addr[0], i->remote_ip_addr[1],
					i->remote_ip_addr[2], i->remote_ip_addr[3],
					i->remote_ip_addr[4], i->remote_ip_addr[5],
					i->remote_ip_addr[6], i->remote_ip_addr[7],
					i->local_port, i->remote_port,
					i->protocol, i->static_ip_address);
				if (len >= sizeof(fwts_uefi_ipv6_dev_path_v2) && dev_path_len >= sizeof(fwts_uefi_ipv6_dev_path_v2)) {
					fwts_uefi_ipv6_dev_path_v2 *iv2 = (fwts_uefi_ipv6_dev_path_v2 *)dev_path;
					path = uefidump_vprintf(path,
					",%" PRIu8 ","
					"%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16
					":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16,
					iv2->prefix_length,
					iv2->gateway_ip_addr[0], iv2->gateway_ip_addr[1],
					iv2->gateway_ip_addr[2], iv2->gateway_ip_addr[3],
					iv2->gateway_ip_addr[4], iv2->gateway_ip_addr[5],
					iv2->gateway_ip_addr[6], iv2->gateway_ip_addr[7]);
				}
				path = uefidump_vprintf(path, ")");
			}
			break;
		case FWTS_UEFI_INFINIBAND_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_infiniband_dev_path)) {
				fwts_uefi_infiniband_dev_path *i = (fwts_uefi_infiniband_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\InfiniBand("
					"%" PRIx8 ",%" PRIx64 ",%" PRIx64 ",%" PRIx64 ")",
					i->port_gid[0], i->remote_id,
					i->target_port_id, i->device_id);
			}
			break;
		case FWTS_UEFI_UART_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_uart_dev_path)) {
				fwts_uefi_uart_dev_path *u = (fwts_uefi_uart_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\UART("
					"%" PRIu64 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ")",
					u->baud_rate, u->data_bits, u->parity, u->stop_bits);
			}
			break;
		case FWTS_UEFI_VENDOR_MESSAGING_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_vendor_messaging_dev_path)) {
				fwts_uefi_vendor_messaging_dev_path *v = (fwts_uefi_vendor_messaging_dev_path *)dev_path;
				size_t i;
				static const fwts_uefi_guid guid[] = {
					EFI_PC_ANSI_GUID,
					EFI_VT_100_GUID,
					EFI_VT_100_PLUS_GUID,
					EFI_VT_UTF8_GUID,
					EFI_UART_DEVICE_PATH_GUID,
					EFI_SAS_DEVICE_PATH_GUID
				};
				static const char *str[] = {
					"PC-ANSI",
					"VT-100",
					"VT-100+",
					"VT-UTF8",
					"UARTFLOWCTRL",
					"SAS",
					"UnknownGUID"
				};
				for (i = 0; i < sizeof(guid)/sizeof(guid[0]); i++)
					if (memcmp(&v->guid, &guid[i], sizeof(fwts_uefi_guid)) == 0)
						break;
				path = uefidump_vprintf(path, "\\VENDOR(%s", str[i]);
				if (i == 4) {
					uint32_t flow_control_map = v->vendor_defined_data[0]
							+ (((uint32_t)v->vendor_defined_data[1]) << 8)
							+ (((uint32_t)v->vendor_defined_data[2]) << 16)
							+ (((uint32_t)v->vendor_defined_data[3] << 24));
					path = uefidump_vprintf(path, ",0x%" PRIx32, flow_control_map);
				}
				if (i == 5 && dev_path_len >= sizeof(fwts_uefi_sas_messaging_dev_path)) {
					fwts_uefi_sas_messaging_dev_path *s = (fwts_uefi_sas_messaging_dev_path *)dev_path;
					path = uefidump_vprintf(path, ",0x%" PRIx64
						",0x%" PRIx64 ",0x%" PRIx16 ",0x%" PRIx16 ,
						s->sas_addr, s->lun, s->dev_topology_info, s->rtp);
				}
				if (i == 6)
					path = uefidump_vprintf(path, " %08" PRIx32
						"-%04" PRIx16 "-%04" PRIx16 "-%02" PRIx8
						"-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8
						"-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 ,
						v->guid.info1, v->guid.info2, v->guid.info3,
						v->guid.info4[0], v->guid.info4[1], v->guid.info4[2], v->guid.info4[3],
						v->guid.info4[4], v->guid.info4[5], v->guid.info4[6], v->guid.info4[7]);

				path = uefidump_vprintf(path, ")");
			}
			break;
		case FWTS_UEFI_FIBRE_CHANNEL_EX_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_fibre_channel_ex_dev_path)) {
				fwts_uefi_fibre_channel_ex_dev_path *f = (fwts_uefi_fibre_channel_ex_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\FIBREEX(0x%" PRIx64 ",0x%" PRIx64 ")", f->wwn, f->lun);
			}
			break;
		case FWTS_UEFI_SATA_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_sata_dev_path)) {
				fwts_uefi_sata_dev_path *s = (fwts_uefi_sata_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\SATA(0x%" PRIx16 ",0x%" PRIx16 ",0x%" PRIx16 ")",
					s->hbapn, s->pmpn, s->lun);
			}
			break;
		case FWTS_UEFI_USB_WWID_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_usb_wwid_dev_path)) {
				fwts_uefi_usb_wwid_dev_path *u = (fwts_uefi_usb_wwid_dev_path *)dev_path;
				uint16_t len = u->dev_path.length[0] | (((uint16_t)u->dev_path.length[1]) << 8);
				path = uefidump_vprintf(path, "\\USBWWID(0x%" PRIx16 ",0x%" PRIx16 ",0x%" PRIx16,
					u->interface_num, u->vendor_id, u->product_id);
				ssize_t sz;

				/* Adding Serial Number */
				if (len <= sizeof(fwts_uefi_usb_wwid_dev_path)) {
					path = uefidump_vprintf(path, ")");
					break;
				}
				sz = ((ssize_t)len - sizeof(fwts_uefi_usb_wwid_dev_path)) / sizeof(uint16_t) + 1;
				if ((sz > 0) && (sz <= 0xffff)) {
					char *tmp;

					tmp = malloc(sz);
					if (tmp) {
						fwts_uefi_str16_to_str(tmp, sz, u->serial_number);
						path = uefidump_vprintf(path, ",%s", tmp);
						free(tmp);
					}
				}
				path = uefidump_vprintf(path, ")");
			}
			break;
		case FWTS_UEFI_VLAN_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_vlan_dev_path)) {
				fwts_uefi_vlan_dev_path *v = (fwts_uefi_vlan_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\VLAN(0x%" PRIx16 ")", v->vlanid);
			}
			break;
		case FWTS_UEFI_LOGICAL_UNIT_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_logical_unit_dev_path)) {
				fwts_uefi_logical_unit_dev_path *l = (fwts_uefi_logical_unit_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\DEVICELOGICALUNIT(0x%" PRIx8 ")", l->lun);
			}
			break;
		case FWTS_UEFI_SAS_EX_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_sas_ex_dev_path)) {
				fwts_uefi_sas_ex_dev_path *s = (fwts_uefi_sas_ex_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\SASEX(0x%" PRIx64 ",0x%" PRIx64 ",0x%" PRIx16 ",0x%" PRIx16 ")",
					s->sas_addr, s->lun, s->dev_topology_info, s->rtp);
			}
			break;
		case FWTS_UEFI_ISCSI_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_iscsi_dev_path)) {
				fwts_uefi_iscsi_dev_path *i = (fwts_uefi_iscsi_dev_path *)dev_path;
				uint16_t len = i->dev_path.length[0] | (((uint16_t)i->dev_path.length[1]) << 8);
				path = uefidump_vprintf(path, "\\iSCSI(0x%" PRIx16 ",0x%" PRIx16 ",0x%" PRIx64 ",0x%" PRIx16,
					i->protocol, i->options, i->lun, i->tpg_tag);

				/* Adding iSCSI target name */
				if (len - sizeof(fwts_uefi_iscsi_dev_path) > 223) {
					path = uefidump_vprintf(path, ")");
					break;
				}
				path = uefidump_vprintf(path, ",%s)", i->iscsi_tn);
			}
			break;
		case FWTS_UEFI_NVM_EXPRESS_NAMESP_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_nvm_express_namespace_dev_path)) {
				fwts_uefi_nvm_express_namespace_dev_path *n = (fwts_uefi_nvm_express_namespace_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\NVMEXPRESS(0x%" PRIx32 ",0x%" PRIx64 ")",
					n->namesp_id, n->ext_unique_id);
			}
			break;
		case FWTS_UEFI_URI_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_uri_dev_path)) {
				fwts_uefi_uri_dev_path *u = (fwts_uefi_uri_dev_path *)dev_path;
				uint16_t len = u->dev_path.length[0] | (((uint16_t)u->dev_path.length[1]) << 8);
				if ((len > sizeof(fwts_uefi_uri_dev_path)) &&
				    (len <= dev_path_len - sizeof(fwts_uefi_uri_dev_path))) {
					uint16_t tmp_len = len - sizeof(fwts_uefi_uri_dev_path);
					char tmp[tmp_len + 1];

					memcpy(tmp, u->uri, tmp_len);
					tmp[tmp_len] = '\0';
					path = uefidump_vprintf(path, "\\URI(%s)", tmp);
				}
			}
			break;
		case FWTS_UEFI_UFS_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_ufs_dev_path)) {
				fwts_uefi_ufs_dev_path *u = (fwts_uefi_ufs_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\UFS(0x%" PRIx8 ",0x%" PRIx8 ")",
					u->target_id, u->lun);
			}
			break;
		case FWTS_UEFI_SD_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_sd_dev_path)) {
				fwts_uefi_sd_dev_path *s = (fwts_uefi_sd_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\SD(0x%" PRIx8 ")", s->slot_number);
			}
			break;
		case FWTS_UEFI_BLUETOOTH_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_bluetooth_dev_path)) {
				fwts_uefi_bluetooth_dev_path *b = (fwts_uefi_bluetooth_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\BT("
					"%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 ")",
					b->bluetooth_addr[0], b->bluetooth_addr[1], b->bluetooth_addr[2],
					b->bluetooth_addr[3], b->bluetooth_addr[4], b->bluetooth_addr[5]);
			}
			break;
		case FWTS_UEFI_WIRELESS_DEVICE_PATH_SUBTYPE:
			if (dev_path_len > sizeof(fwts_uefi_wireless_dev_path)) {
				fwts_uefi_wireless_dev_path *w = (fwts_uefi_wireless_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\WiFi(%s)", w->ssid);
			}
			break;
		case FWTS_UEFI_EMMC_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_emmc_dev_path)) {
				fwts_uefi_emmc_dev_path *e = (fwts_uefi_emmc_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\eMMC(0x%" PRIx8 ")", e->slot_num);
			}
			break;
		case FWTS_UEFI_BLUETOOTHLE_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_bluetoothle_dev_path)) {
				fwts_uefi_bluetoothle_dev_path *b = (fwts_uefi_bluetoothle_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\BluetoothLE("
					"%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 ",0x%" PRIx8 ")",
					b->bluetooth_addr[0], b->bluetooth_addr[1], b->bluetooth_addr[2],
					b->bluetooth_addr[3], b->bluetooth_addr[4], b->bluetooth_addr[5], b->addr_type);
			}
			break;
		case FWTS_UEFI_DNS_DEVICE_PATH_SUBTYPE:
			if (dev_path_len > sizeof(fwts_uefi_dns_dev_path)) {
				fwts_uefi_dns_dev_path *d = (fwts_uefi_dns_dev_path *)dev_path;
				const uint16_t len = d->dev_path.length[0] | (((uint16_t)d->dev_path.length[1]) << 8);
				const ssize_t sz = (ssize_t)len - sizeof(fwts_uefi_dns_dev_path);
				path = uefidump_vprintf(path, "\\DNS(0x%" PRIx8 ",", d->isipv6);

				/*
				 *  Add a little more sanity checking, but we can never be sure
				 *  we will not overrun d->dns_addr as we have to trust the given
				 *  length.
				 */
				if ((sz > 0) && (sz <= 0xffff)) {
					ssize_t i;

					/* dump one or more DNS server address */
					for (i = 0; i < sz; i++)
						path = uefidump_vprintf(path, "%02" PRIx8 , d->dns_addr[i]);
					path = uefidump_vprintf(path, ")");
				}
			}
			break;
		default:
			path = uefidump_vprintf(path, "\\Unknown-MESSAGING-DEV-PATH(0x%" PRIx8 ")", dev_path->subtype);
			break;
		}
		break;

	case FWTS_UEFI_MEDIA_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_hard_drive_dev_path)) {
				fwts_uefi_hard_drive_dev_path *h = (fwts_uefi_hard_drive_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\HARDDRIVE("
				"%" PRIu32 ",%" PRIx64 ",%" PRIx64 ","
				"%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8
				"%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 ","
				"%" PRIx8 ",%" PRIx8 ")",
				h->partition_number,
				h->partition_start,
				h->partition_size,
				h->partition_signature[0], h->partition_signature[1],
				h->partition_signature[2], h->partition_signature[3],
				h->partition_signature[4], h->partition_signature[5],
				h->partition_signature[6], h->partition_signature[7],
				h->partition_signature[8], h->partition_signature[9],
				h->partition_signature[10], h->partition_signature[11],
				h->partition_signature[12], h->partition_signature[13],
				h->partition_signature[14], h->partition_signature[15],
				h->mbr_type, h->signature_type);
			}
			break;
		case FWTS_UEFI_CDROM_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_cdrom_dev_path)) {
				fwts_uefi_cdrom_dev_path *c = (fwts_uefi_cdrom_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\CDROM(%" PRIu32 ",%" PRIx64 ",%" PRIx64 ")",
					c->boot_entry, c->partition_start, c->partition_size);
			}
			break;
		case FWTS_UEFI_VENDOR_MEDIA_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_uefi_vendor_media_dev_path)) {
				fwts_uefi_vendor_media_dev_path *v = (fwts_uefi_vendor_media_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\VENDOR("
					"%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-"
					"%02" PRIx8 "-%02" PRIx8 "-"
					"%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 ")",
					v->guid.info1, v->guid.info2, v->guid.info3,
					v->guid.info4[0], v->guid.info4[1], v->guid.info4[2], v->guid.info4[3],
					v->guid.info4[4], v->guid.info4[5], v->guid.info4[6], v->guid.info4[7]);
			}
			break;
		case FWTS_UEFI_FILE_PATH_DEVICE_PATH_SUBTYPE:
			{
				char tmp[4096];
				fwts_uefi_file_path_dev_path *f = (fwts_uefi_file_path_dev_path*)dev_path;
				fwts_uefi_str16_to_str(tmp, sizeof(tmp), f->path_name);
				path = uefidump_vprintf(path, "\\FILE('%s')", tmp);
			}
			break;
		case FWTS_UEFI_PROTOCOL_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_media_protocol_dev_path)) {
				fwts_media_protocol_dev_path *m = (fwts_media_protocol_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\MEDIAPROTOCOL("
					"%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-"
					"%02" PRIx8 "-%02" PRIx8 "-"
					"%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 ")",
					m->protocol_guid.info1, m->protocol_guid.info2, m->protocol_guid.info3,
					m->protocol_guid.info4[0], m->protocol_guid.info4[1], m->protocol_guid.info4[2],
					m->protocol_guid.info4[3], m->protocol_guid.info4[4], m->protocol_guid.info4[5],
					m->protocol_guid.info4[6], m->protocol_guid.info4[7]);
			}
			break;
		case FWTS_UEFI_PIWG_FW_FILE_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_piwg_fw_file_dev_path)) {
				fwts_piwg_fw_file_dev_path *p = (fwts_piwg_fw_file_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\FVFILE("
					"%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-"
					"%02" PRIx8 "-%02" PRIx8 "-"
					"%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 ")",
					p->fw_file_name.info1, p->fw_file_name.info2, p->fw_file_name.info3,
					p->fw_file_name.info4[0], p->fw_file_name.info4[1], p->fw_file_name.info4[2],
					p->fw_file_name.info4[3], p->fw_file_name.info4[4], p->fw_file_name.info4[5],
					p->fw_file_name.info4[6], p->fw_file_name.info4[7]);
			}
			break;
		case FWTS_UEFI_PIWG_FW_VOLUME_DEVICE_PATH_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_piwg_fw_volume_dev_path)) {
				fwts_piwg_fw_volume_dev_path *p = (fwts_piwg_fw_volume_dev_path *)dev_path;
				path = uefidump_vprintf(path, "\\FV("
					"%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-"
					"%02" PRIx8 "-%02" PRIx8 "-"
					"%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 ")",
					p->fw_volume_name.info1, p->fw_volume_name.info2, p->fw_volume_name.info3,
					p->fw_volume_name.info4[0], p->fw_volume_name.info4[1], p->fw_volume_name.info4[2],
					p->fw_volume_name.info4[3], p->fw_volume_name.info4[4], p->fw_volume_name.info4[5],
					p->fw_volume_name.info4[6], p->fw_volume_name.info4[7]);
			}
			break;
		case FWTS_UEFI_RELATIVE_OFFSET_RANGE_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_relative_offset_range_path)) {
				fwts_relative_offset_range_path *r = (fwts_relative_offset_range_path *)dev_path;
				path = uefidump_vprintf(path, "\\RELATIVEOFFSETRANGE("
					"0x%" PRIx64 ",0x%" PRIx64 ")",
					r->starting_offset, r->ending_offset);
			}
			break;
		case FWTS_UEFI_RAM_DISK_SUBTYPE:
			if (dev_path_len >= sizeof(fwts_ram_disk_path)) {
				fwts_ram_disk_path *r = (fwts_ram_disk_path *)dev_path;
				path = uefidump_vprintf(path, "\\RAMDISK("
					"0x%" PRIx64 ",0x%" PRIx64
					"%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-"
					"%02" PRIx8 "-%02" PRIx8 "-"
					"%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8 "-%02" PRIx8
					",0x%" PRIx16 ")",
					r->starting_addr, r->ending_addr,
					r->disk_type_guid.info1, r->disk_type_guid.info2, r->disk_type_guid.info3,
					r->disk_type_guid.info4[0], r->disk_type_guid.info4[1], r->disk_type_guid.info4[2],
					r->disk_type_guid.info4[3], r->disk_type_guid.info4[4], r->disk_type_guid.info4[5],
					r->disk_type_guid.info4[6], r->disk_type_guid.info4[7],
					r->disk_instance);
			}
			break;
		default:
			path = uefidump_vprintf(path, "\\Unknown-MEDIA-DEV-PATH(0x%" PRIx8 ")", dev_path->subtype);
			break;
		}
		break;

	case FWTS_UEFI_BIOS_DEVICE_PATH_TYPE:
		switch (dev_path->subtype) {
		case FWTS_UEFI_BIOS_DEVICE_PATH_SUBTYPE:
			{
				fwts_uefi_bios_dev_path *b = (fwts_uefi_bios_dev_path*)dev_path;
				path = uefidump_vprintf(path, "\\BIOS(%" PRIx16 ",%" PRIx16 ",%s)",
					b->device_type, b->status_flags, b->description);
			}
			break;
		default:
			path = uefidump_vprintf(path, "\\Unknown-BIOS-DEV-PATH(0x%" PRIx8 ")", dev_path->subtype);
			break;
		}
		break;
	default:
		path = uefidump_vprintf(path, "\\Unknown-TYPE(0x%" PRIx8 ")", dev_path->type);
		break;
	}

	/* Not end? - collect more */
	if (!((dev_path->type & 0x7f) == (FWTS_UEFI_END_DEV_PATH_TYPE) &&
	      (dev_path->subtype == FWTS_UEFI_END_ENTIRE_DEV_PATH_SUBTYPE))) {
		uint16_t len = dev_path->length[0] | (((uint16_t)dev_path->length[1])<<8);
		if (len > 0) {
			dev_path = (fwts_uefi_dev_path*)((char *)dev_path + len);
			path = uefidump_build_dev_path(path, dev_path, dev_path_len - len);
		}
	}

	return path;
}

static void uefidump_info_dev_path(fwts_framework *fw, fwts_uefi_var *var)
{
	char *path;

	path = uefidump_build_dev_path(NULL, (fwts_uefi_dev_path*)var->data, var->datalen);

	fwts_log_info_verbatim(fw, "  Device Path: %s", path);

	free(path);
}

static void uefidump_info_lang(fwts_framework *fw, fwts_uefi_var *var)
{
	uint8_t *data = (uint8_t*)var->data;
	fwts_log_info_verbatim(fw, "  Language: %c%c%c%c", data[0], data[1], data[2], data[3]);
}

static void uefidump_info_langcodes(fwts_framework *fw, fwts_uefi_var *var)
{
	char buffer[2048];
	char *dst = buffer;
	char *data = (char*)var->data;

	for (;;) {
		*dst++ = *data++;
		*dst++ = *data++;
		*dst++ = *data++;
		if (*data < ' ')
			break;
		*dst++ = ',';
	}
	*dst = '\0';

	fwts_log_info_verbatim(fw, "  Language Codes: %s", buffer);
}

static void uefidump_info_platform_lang(fwts_framework *fw, fwts_uefi_var *var)
{
	uint8_t *data = (uint8_t*)var->data;
	fwts_log_info_verbatim(fw, "  Platform Language: %c%c%c%c%c%c.", data[0], data[1], data[2], data[3], data[4], data[5]);
}

static void uefidump_info_platform_langcodes(fwts_framework *fw, fwts_uefi_var *var)
{
	char buffer[2048];

	char *dst = buffer;
	char *data = (char*)var->data;

	for (;;) {
		if (*data < ' ')
			break;
		if (*data == ';')
			*dst++ = ',';
		else
			*dst++ = *data;
		data++;
	}
	*dst = '\0';

	fwts_log_info_verbatim(fw, "  Platform Language Codes: %s", buffer);
}

static void uefidump_info_timeout(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen >= sizeof(uint16_t)) {
		uint16_t *data = (uint16_t*)var->data;

		fwts_log_info_verbatim(fw, "  Timeout: %" PRId16 " seconds", *data);
	}
}

static void uefidump_info_bootcurrent(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen >= sizeof(uint16_t)) {
		uint16_t *data = (uint16_t *)var->data;

		fwts_log_info_verbatim(fw, "  BootCurrent: 0x%4.4" PRIx16, *data);
	}
}

static void uefidump_info_bootnext(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen >= sizeof(uint16_t)) {
		uint16_t *data = (uint16_t *)var->data;

		fwts_log_info_verbatim(fw, "  BootNext: 0x%4.4" PRIx16, *data);
	}
}

static void uefidump_info_bootoptionsupport(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen >= sizeof(uint16_t)) {
		uint16_t *data = (uint16_t *)var->data;

		fwts_log_info_verbatim(fw, "  BootOptionSupport: 0x%4.4" PRIx16, *data);
	}
}

static void uefidump_info_bootorder(fwts_framework *fw, fwts_uefi_var *var)
{
	uint16_t *data = (uint16_t*)var->data;
	int i;
	int n = (int)var->datalen / sizeof(uint16_t);
	char *str = NULL;

	for (i = 0; i < n; i++) {
		str = uefidump_vprintf(str, "0x%04" PRIx16 "%s",
			*data++, i < (n - 1) ? "," : "");
	}
	fwts_log_info_verbatim(fw, "  Boot Order: %s", str);
	free(str);
}

static void uefidump_info_bootdev(fwts_framework *fw, fwts_uefi_var *var)
{
	fwts_uefi_load_option *load_option;
	char tmp[2048];
	size_t len, offset;

	if (var->datalen < sizeof(fwts_uefi_load_option))
		return;

 	load_option = (fwts_uefi_load_option *)var->data;
	fwts_log_info_verbatim(fw, "  Active: %s\n",
		(load_option->attributes & FWTS_UEFI_LOAD_OPTION_ACTIVE) ? "Yes" : "No");
	fwts_uefi_str16_to_str(tmp, sizeof(tmp), load_option->description);
	len = fwts_uefi_str16len(load_option->description);
	fwts_log_info_verbatim(fw, "  Info: %s\n", tmp);

	if (load_option->file_path_list_length != 0) {
		char *path;

		/* Skip over description to get to packed path, unpack path and print */
		offset = sizeof(load_option->attributes) +
			 sizeof(load_option->file_path_list_length) +
			 (sizeof(uint16_t) * (len + 1));

		path = uefidump_build_dev_path(NULL,
				(fwts_uefi_dev_path *)(var->data + offset), var->datalen - offset);
		fwts_log_info_verbatim(fw, "  Path: %s", path);
		free(path);
	}

	offset = sizeof(load_option->attributes) +
		 sizeof(load_option->file_path_list_length) +
		 (sizeof(uint16_t) * (len + 1)) +
		 load_option->file_path_list_length;
	if ((var->datalen - offset) > 0) {
		fwts_log_info_verbatim(fw, "  OptionalData:");
		uefidump_data_hexdump(fw, var->data + offset, var->datalen - offset);
	}
}

/*
 *  Dump kernel oops log messages
 */
static void uefidump_info_dump_type0(fwts_framework *fw, fwts_uefi_var *var)
{
	char *ptr = (char*)var->data;
	size_t len = var->datalen;
	bool printable = true;

	while (len && *ptr) {
		if (!(isspace(*ptr) || isprint(*ptr))) {
			printable = false;
			break;
		}
		ptr++;
		len--;
	}

	ptr = (char*)var->data;
	len = var->datalen;

	if (printable) {
		while (len && *ptr) {
			char *start = ptr;
			while (len && *ptr && *ptr != '\n') {
				ptr++;
				len--;
			}
			if (*ptr == '\n') {
				*ptr++ = '\0';
				len--;
				fwts_log_info_verbatim(fw, "  KLog: %s", start);
			}
		}
	} else {
		uefidump_var_hexdump(fw, var);
	}
}

static void uefidump_info_secure_boot(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 1) {
		/* Should be 1 byte, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = " (Secure Boot Mode Off)";
			break;
		case 1:
			mode = " (Secure Boot Mode On)";
			break;
		default:
			mode = "";
			break;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2x%s", value, mode);
	}
}

static void uefidump_info_setup_mode(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 1) {
		/* Should be 1 byte, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = " (User Mode)";
			break;
		case 1:
			mode = " (Setup Mode)";
			break;
		default:
			mode = "";
			break;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2" PRIx8 "%s", value, mode);
	}
}

/*
 *  See TCG Platform Reset Attack Mitigation Specification, Revision 1.0,
 *  section 5.
 */
static void uefidump_info_morc(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 1) {
		/* Should be 1 byte, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value & 1) {
		case 0:
			mode = " (Firmware should not clear memory on reboot)";
			break;
		case 1:
			mode = " (Firmware should clear memory on reboot)";
			break;
		default:
			mode = "";
			break;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2" PRIx8 "%s", value, mode);
	}
}

/*
 *  Dump ACPI global variable address
 */
static void uefidump_info_acpi_global_variable(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 8) {
		/* Should be 8 bytes, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		uint64_t value;

		memcpy(&value, var->data, sizeof(uint64_t));
		fwts_log_info_verbatim(fw, "  ACPI Global Variable Address: 0x%16.16" PRIx64, value);
	}
}

/*
 *  Dump Supported Signature GUIDs
 */
static void uefidump_info_signature_support(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen % 16) {
		/* Should be multiple of 16 bytes, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		/* Signatures are an array of GUIDs */
		uint8_t *data = var->data;
		char guid_str[37];

		if (var->datalen)
			fwts_log_info_verbatim(fw, "  Signature GUIDs:");

		while (data - var->data < (ptrdiff_t)var->datalen) {
			fwts_guid_buf_to_str(data, guid_str, sizeof(guid_str));
			fwts_log_info_verbatim(fw, "    %s", guid_str);
			data += 16;
		}
	}
}

static void uefidump_info_hwerrrec_support(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 2) {
		/* Should be 2 byte, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		char *support;
		uint16_t *value = (uint16_t *)var->data;

		switch (*value) {
		case 0:
			support = " (Not support for Hardware Error Record Persistence)";
			break;
		case 1:
			support = " (Support for Hardware Error Record Persistence)";
			break;
		default:
			support = " (reserved value)";
			break;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%4.4" PRIx16 "%s", *value, support);
	}
}

static void uefidump_info_osindications_supported(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 8) {
		/* Should be 8 bytes, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		uint64_t value;
		char str[300];

		memcpy(&value, var->data, sizeof(uint64_t));
		*str = 0;

		if (value & EFI_OS_INDICATIONS_BOOT_TO_FW_UI)
			strcat(str, "EFI_OS_INDICATIONS_BOOT_TO_FW_UI");

		if (value & EFI_OS_INDICATIONS_TIMESTAMP_REVOCATION) {
			if (*str)
				strcat(str, ",");
			strcat(str, "EFI_OS_INDICATIONS_TIMESTAMP_REVOCATION");
		}

		if (value & EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED) {
			if (*str)
				strcat(str, ",");
			strcat(str, "EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED");
		}

		if (value & EFI_OS_INDICATIONS_FMP_CAPSULE_SUPPORTED) {
			if (*str)
				strcat(str, ",");
			strcat(str, "EFI_OS_INDICATIONS_FMP_CAPSULE_SUPPORTED");
		}

		if (value & EFI_OS_INDICATIONS_CAPSULE_RESULT_VAR_SUPPORTED) {
			if (*str)
				strcat(str, ",");
			strcat(str, "EFI_OS_INDICATIONS_CAPSULE_RESULT_VAR_SUPPORTED");
		}

		fwts_log_info_verbatim(fw, "  Value: 0x%16.16" PRIx64 " (%s)", value, str);
	}
}

static void uefidump_info_vendor_keys(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen >= sizeof(uint8_t)) {
		uint8_t value = (uint8_t)var->data[0];

		fwts_log_info_verbatim(fw, "  Value: 0x%2.2" PRIx8, value);
	}
}

static void uefidump_info_driverorder(fwts_framework *fw, fwts_uefi_var *var)
{
	uint16_t *data = (uint16_t*)var->data;
	size_t i, n = var->datalen / sizeof(uint16_t);
	char *str = NULL;

	for (i = 0; i < n; i++) {
		str = uefidump_vprintf(str, "0x%04" PRIx16 "%s",
			*data++, i < (n - 1) ? "," : "");
	}
	fwts_log_info_verbatim(fw, "  Driver Order: %s", str);
	free(str);
}

static void uefidump_info_driverdev(fwts_framework *fw, fwts_uefi_var *var)
{
	fwts_uefi_load_option *load_option;
	size_t len;
	size_t offset;

	if (var->datalen < sizeof(fwts_uefi_load_option))
		return;

	load_option = (fwts_uefi_load_option *)var->data;
	fwts_log_info_verbatim(fw, "  Force Reconnect: %s\n",
		(load_option->attributes & FWTS_UEFI_LOAD_OPTION_FORCE_RECONNECT) ? "Yes" : "No");

	len = fwts_uefi_str16len(load_option->description);
	if (len != 0) {
		char *tmp = malloc(len + 1);
		if (tmp) {
			fwts_uefi_str16_to_str(tmp, len + 1, load_option->description);
			fwts_log_info_verbatim(fw, "  Info: %s\n", tmp);
			free(tmp);
		}
	}

	if (load_option->file_path_list_length != 0) {
		char *path;

		/* Skip over description to get to packed path, unpack path and print */
		offset = sizeof(load_option->attributes) +
			 sizeof(load_option->file_path_list_length) +
			 (sizeof(uint16_t) * (len + 1));
		path = uefidump_build_dev_path(NULL,
			(fwts_uefi_dev_path *)(var->data + offset), var->datalen - offset);
		fwts_log_info_verbatim(fw, "  Path: %s", path);
		free(path);
	}

	offset = sizeof(load_option->attributes) +
		 sizeof(load_option->file_path_list_length) +
		 (sizeof(uint16_t) * (len + 1)) +
		 load_option->file_path_list_length;
	if ((var->datalen - offset) > 0) {
		fwts_log_info_verbatim(fw, "  OptionalData:");
		uefidump_data_hexdump(fw, var->data + offset, var->datalen - offset);
	}
}

static void uefidump_info_keyoption(fwts_framework *fw, fwts_uefi_var *var)
{
	fwts_uefi_key_option *key_option = (fwts_uefi_key_option *)var->data;
	fwts_uefi_input_key *inputkey = (fwts_uefi_input_key *) (((uint8_t *) var->data) + sizeof (fwts_uefi_key_option));
	char str[300];
	size_t keyoptionsize, inputkeycount, index = 0;

	*str = 0;

	if (var->datalen < sizeof (fwts_uefi_key_option)) {
		uefidump_var_hexdump(fw, var);
		return;
	}

	if (key_option->keydata & (1 << 8))
		strcat(str, "ShiftPressed");

	if (key_option->keydata & (1 << 9)) {
		if (*str)
			strcat(str, ",");
		strcat(str, "ControlPressed");
	}

	if (key_option->keydata & (1 << 10)) {
		if (*str)
			strcat(str, ",");
		strcat(str, "AltPressed");
	}

	if (key_option->keydata &(1 << 11)) {
		if (*str)
			strcat(str, ",");
		strcat(str, "LogoPressed");
	}

	if (key_option->keydata & (1 << 12)) {
		if (*str)
			strcat(str, ",");
		strcat(str, "MenuPressed");
	}

	if (key_option->keydata & (1 << 13)) {
		if (*str)
			strcat(str, ",");
		strcat(str, "SysReqPressed");
	}

	if (*str != 0)
		fwts_log_info_verbatim(fw, "  PackedValue: 0x%8.8" PRIx32 " (%s)", key_option->keydata, str);
	else
		fwts_log_info_verbatim(fw, "  PackedValue: 0x%8.8" PRIx32, key_option->keydata);

	fwts_log_info_verbatim(fw, "  BootOptionCrc: 0x%8.8" PRIx32, key_option->bootoptioncrc);
	fwts_log_info_verbatim(fw, "  BootOption: %4.4" PRIx16, key_option->bootoption);

	inputkeycount = (key_option->keydata & 0xC0000000) >> 30;
	for (index = 0; index < inputkeycount; index++) {
		fwts_log_info_verbatim(fw, "  ScanCode: 0x%4.4" PRIx16, inputkey[index].scancode);
		fwts_log_info_verbatim(fw, "  UnicodeChar: 0x%4.4" PRIx16, inputkey[index].unicodechar);
	}

	keyoptionsize = sizeof (fwts_uefi_key_option) + inputkeycount * sizeof (fwts_uefi_input_key);

	/*
	 * there are extra data following the keyoption data structure which the firmware is using,
	 * dump all data for reference
	 */
	if (var->datalen > keyoptionsize) {
		uefidump_var_hexdump(fw, var);
	}
}

#define GUID_MAP(x) { x, # x }

static void uefidump_info_signaturedatabase(fwts_framework *fw, fwts_uefi_var *var)
{
	char guid_str[37];
	size_t list_start = 0;
	size_t i;

	typedef struct {
		const fwts_uefi_guid guid;
		const char *str;
	} guid_map;

	static const guid_map guids[] = {
		GUID_MAP(EFI_CERT_X509_GUID),
		GUID_MAP(EFI_CERT_SHA256_GUID),
		GUID_MAP(EFI_CERT_RSA2048_GUID),
		GUID_MAP(EFI_CERT_RSA2048_SHA256_GUID),
		GUID_MAP(EFI_CERT_SHA1_GUID),
		GUID_MAP(EFI_CERT_RSA2048_SHA1_GUID),
		GUID_MAP(EFI_CERT_SHA224_GUID),
		GUID_MAP(EFI_CERT_SHA384_GUID),
		GUID_MAP(EFI_CERT_SHA512_GUID),
		GUID_MAP(EFI_CERT_X509_SHA256_GUID),
		GUID_MAP(EFI_CERT_X509_SHA384_GUID),
		GUID_MAP(EFI_CERT_X509_SHA512_GUID),
		{ { 0, 0, 0, { 0, }}, NULL }
	};

	if (var->datalen < sizeof(fwts_uefi_signature_list))
		return;

	do {
		fwts_uefi_signature_list *signature_list =
			(fwts_uefi_signature_list *)(var->data + list_start);
		const char *str = "Unknown GUID";
		size_t offset = 0;

		fwts_guid_buf_to_str(var->data, guid_str, sizeof(guid_str));

		for (i = 0; guids[i].str; i++)
			if (!memcmp(var->data, &guids[i].guid, sizeof(fwts_uefi_guid))) {
				str = guids[i].str;
				break;
			}

		fwts_log_info_verbatim(fw, "  SignatureType: %s (%s)", guid_str, str);
		fwts_log_info_verbatim(fw, "  SignatureListSize: 0x%" PRIx32, signature_list->signaturelistsize);
		fwts_log_info_verbatim(fw, "  SignatureHeaderSize: 0x%" PRIx32, signature_list->signatureheadersize);
		fwts_log_info_verbatim(fw, "  SignatureSize: 0x%" PRIx32, signature_list->signaturesize);

		offset = list_start + sizeof (fwts_uefi_signature_list);
		if (signature_list->signatureheadersize > 0) {
			fwts_log_info_verbatim(fw, "  SignatureHeader:");
			uefidump_data_hexdump(fw, (uint8_t *)(var->data + offset), signature_list->signatureheadersize);
		}
		offset += signature_list->signatureheadersize;
		while ((signature_list->signaturelistsize - offset + list_start) > 0) {
			if ((signature_list->signaturelistsize - offset + list_start) >= signature_list->signaturesize) {
				fwts_guid_buf_to_str((uint8_t *)(var->data + offset), guid_str, sizeof(guid_str));
				fwts_log_info_verbatim(fw, "  SignatureOwner: %s", guid_str);
				offset += sizeof(fwts_uefi_guid);
				fwts_log_info_verbatim(fw, "  SignatureData:");
				uefidump_data_hexdump(fw, (uint8_t *)(var->data + offset), signature_list->signaturesize - sizeof(fwts_uefi_guid));
				offset += signature_list->signaturesize - sizeof(fwts_uefi_guid);
			} else {
				/* have Signature data left, but not correspond to the SignatureSize, just dump raw data */
				fwts_log_info_verbatim(fw, "  Data:");
				uefidump_data_hexdump(fw, (uint8_t *)(var->data + offset), (signature_list->signaturelistsize - offset + list_start));
				break;
			}
		}

		list_start += signature_list->signaturelistsize;

	} while ((var->datalen - list_start) > sizeof(fwts_uefi_signature_list));

}

static void uefidump_info_audit_mode(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 1) {
		/* Should be 1 byte, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = " (Not in Audit Mode)";
			break;
		case 1:
			mode = " (In Audit Mode)";
			break;
		default:
			mode = "";
			break;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2x%s", value, mode);
	}
}

static void uefidump_info_deployed_mode(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen != 1) {
		/* Should be 1 byte, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = " (Not in Deployed Mode)";
			break;
		case 1:
			mode = " (In Deployed Mode)";
			break;
		default:
			mode = "";
			break;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2x%s", value, mode);
	}
}

static void uefidump_info_osrecoverorder(fwts_framework *fw, fwts_uefi_var *var)
{
	if (var->datalen % 16) {
		/* Should be multiple of 16 bytes, of not, dump it out as a hex dump */
		uefidump_var_hexdump(fw, var);
	} else {
		/* OSRecoveryOrder are an array of GUIDs */
		uint8_t *data = var->data;
		char guid_str[37];

		if (var->datalen)
			fwts_log_info_verbatim(fw, "  OSRecoveryOrder GUIDs:");

		while (data - var->data < (ptrdiff_t)var->datalen) {
			fwts_guid_buf_to_str(data, guid_str, sizeof(guid_str));
			fwts_log_info_verbatim(fw, "    %s", guid_str);
			data += 16;
		}
	}
}

static void uefidump_info_syspreporder(fwts_framework *fw, fwts_uefi_var *var)
{
	uint16_t *data = (uint16_t*)var->data;
	int i;
	int n = (int)var->datalen / sizeof(uint16_t);
	char *str = NULL;

	for (i = 0; i < n; i++) {
		str = uefidump_vprintf(str, "0x%04" PRIx16 "%s",
			*data++, i < (n - 1) ? "," : "");
	}
	fwts_log_info_verbatim(fw, "  SysPrep Order: %s", str);
	free(str);
}

static const uefidump_info uefidump_info_table[] = {
	{ "PlatformLangCodes",	uefidump_info_platform_langcodes },
	{ "PlatformLang",	uefidump_info_platform_lang },
	{ "BootOptionSupport", 	uefidump_info_bootoptionsupport },
	{ "BootCurrent", 	uefidump_info_bootcurrent },
	{ "BootOrder",		uefidump_info_bootorder },
	{ "BootNext", 		uefidump_info_bootnext },
	{ "ConInDev",		uefidump_info_dev_path },
	{ "ConIn",		uefidump_info_dev_path },
	{ "ConOutDev",		uefidump_info_dev_path },
	{ "ConOut",		uefidump_info_dev_path },
	{ "ConErrDev",		uefidump_info_dev_path },
	{ "ErrOutDev",		uefidump_info_dev_path },
	{ "ErrOut",		uefidump_info_dev_path },
	{ "LangCodes",		uefidump_info_langcodes },
	{ "Lang",		uefidump_info_lang },
	{ "Timeout",		uefidump_info_timeout },
	{ "dump-type0-",	uefidump_info_dump_type0 },
	{ "SecureBoot",		uefidump_info_secure_boot },
	{ "SetupMode",		uefidump_info_setup_mode },
	{ "MemoryOverwriteRequestControl",	uefidump_info_morc },
	{ "AcpiGlobalVariable",	uefidump_info_acpi_global_variable },
	{ "SignatureSupport",	uefidump_info_signature_support },
	{ "HwErrRecSupport",	uefidump_info_hwerrrec_support },
	{ "OsIndicationsSupported",	uefidump_info_osindications_supported },
	{ "VendorKeys",		uefidump_info_vendor_keys },
	{ "DriverOrder",	uefidump_info_driverorder },
	{ "db",			uefidump_info_signaturedatabase },
	{ "KEK",		uefidump_info_signaturedatabase },
	{ "PK",			uefidump_info_signaturedatabase },
	{ "AuditMode",		uefidump_info_audit_mode },
	{ "DeployedMode",	uefidump_info_deployed_mode },
	{ "OsRecoveryOrder",	uefidump_info_osrecoverorder },
	{ "SysPrepOrder",	uefidump_info_syspreporder },
	{ NULL, NULL }
};

static void uefidump_var(fwts_framework *fw, fwts_uefi_var *var)
{
	char varname[512];
	char guid_str[37];
	const uefidump_info *info;

	fwts_uefi_get_varname(varname, sizeof(varname), var);

	fwts_log_info_verbatim(fw, "Name: %s", varname);
	fwts_guid_buf_to_str(var->guid, guid_str, sizeof(guid_str));
	fwts_log_info_verbatim(fw, "  GUID: %s", guid_str);
	fwts_log_info_verbatim(fw, "  Attr: 0x%x (%s)", var->attributes, fwts_uefi_attribute_info(var->attributes));

	/* If we've got an appropriate per variable dump mechanism, use this */
	for (info = uefidump_info_table; info->description != NULL; info++) {
		if (strncmp(varname, info->description, strlen(info->description)) == 0) {
			info->func(fw, var);
			return;
		}
	}

	/* Check the boot load option Boot####. #### is a printed hex value */
	if ((strlen(varname) == 8) && (strncmp(varname, "Boot", 4) == 0)
			&& isxdigit(varname[4]) && isxdigit(varname[5])
			&& isxdigit(varname[6]) && isxdigit(varname[7])) {
		uefidump_info_bootdev(fw, var);
		return;
	}

	/* Check the driver load option Driver####. #### is a printed hex value */
	if ((strlen(varname) == 10) && (strncmp(varname, "Driver", 6) == 0)
			&& isxdigit(varname[6]) && isxdigit(varname[7])
			&& isxdigit(varname[8]) && isxdigit(varname[9])) {
		uefidump_info_driverdev(fw, var);
		return;
	}

	/* Check the key option key####. #### is a printed hex value */
	if ((strlen(varname) == 7) && (strncmp(varname, "Key", 3) == 0)
			&& isxdigit(varname[3]) && isxdigit(varname[4])
			&& isxdigit(varname[5]) && isxdigit(varname[6])) {
		uefidump_info_keyoption(fw, var);
		return;
	}

	/*
	 * Check the platformRecovery of boot option PlatformRecovery####. #### is a printed hex value.
	 * PlatformRecovery#### variables share the same structure as Boot#### variables.
	 */
	if ((strlen(varname) == 20) && (strncmp(varname, "PlatformRecovery", 16) == 0)
			&& isxdigit(varname[16]) && isxdigit(varname[17])
			&& isxdigit(varname[18]) && isxdigit(varname[19])) {
		uefidump_info_bootdev(fw, var);
		return;
	}

	/*
	 * Check the System Prep application load option SysPrep####. #### is a printed hex value.
	 */
	if ((strlen(varname) == 11) && (strncmp(varname, "SysPrep", 7) == 0)
			&& isxdigit(varname[7]) && isxdigit(varname[8])
			&& isxdigit(varname[9]) && isxdigit(varname[10])) {
		uefidump_info_bootdev(fw, var);
		return;
	}

	/* otherwise just do a plain old hex dump */
	uefidump_var_hexdump(fw, var);
}

static int uefidump_init(fwts_framework *fw)
{
	if (fw->firmware_type != FWTS_FIRMWARE_UEFI) {
		fwts_log_info(fw, "Cannot detect any UEFI firmware. Aborted.");
		return FWTS_ABORTED;
	}

	return FWTS_OK;
}

static int uefidump_test1(fwts_framework *fw)
{
	fwts_list name_list;

	if (fwts_uefi_get_variable_names(&name_list) == FWTS_ERROR) {
		fwts_log_info(fw, "Cannot find any UEFI variables.");
	} else {
		fwts_list_link *item;

		fwts_list_foreach(item, &name_list) {
			fwts_uefi_var var;
			char *name = fwts_list_data(char *, item);

			if (fwts_uefi_get_variable(name, &var) == FWTS_OK) {
				uefidump_var(fw, &var);
				fwts_uefi_free_variable(&var);
				fwts_log_nl(fw);
			}
		}
	}

	fwts_uefi_free_variable_names(&name_list);

	return FWTS_OK;
}

static fwts_framework_minor_test uefidump_tests[] = {
	{ uefidump_test1, "Dump UEFI Variables." },
	{ NULL, NULL }
};

static fwts_framework_ops uefidump_ops = {
	.description = "Dump UEFI variables.",
	.init        = uefidump_init,
	.minor_tests = uefidump_tests
};

FWTS_REGISTER("uefidump", &uefidump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS | FWTS_FLAG_ROOT_PRIV)

#endif
