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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <arpa/inet.h>

#include "fwts.h"

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

#define FWTS_HWINFO_LISTS_SAME		(0)
#define FWTS_HWINFO_LISTS_DIFFER	(1)
#define FWTS_HWINFO_LISTS_OUT_OF_MEMORY	(-1)

#define FWTS_HWINFO_SYS_NET		"/sys/class/net"
#define FWTS_HWINFO_SYS_INPUT		"/sys/class/input"
#define FWTS_HWINFO_SYS_BLUETOOTH	"/sys/class/bluetooth"
#define FWTS_HWINFO_SYS_TYPEC		"/sys/class/typec"
#define FWTS_HWINFO_SYS_SCSI_DISK	"/sys/class/scsi_disk"
#define FWTS_HWINFO_SYS_DRM		"/sys/class/drm"

typedef struct {
	char name[NAME_MAX + 1];	/* PCI name */
	uint8_t	config[64];		/* PCI config data */
	ssize_t config_len;		/* Length of config data */
} fwts_pci_config;

typedef struct {
	char *name;			/* Network device name */
	char *addr;			/* Address */
	char *hw_addr;			/* H/W MAC address */
} fwts_net_config;

typedef struct {
	char *name;			/* Input class name */
	char *dev_name;			/* Input device name */
	char *phys;			/* Physical name */
} fwts_input_config;

typedef struct {
	char *name;			/* class name */
	char *bt_name;			/* BT name */
	char *address;			/* BT address */
	char *bus;			/* Bus device is on */
	char *type;			/* Type info */
} fwts_bluetooth_config;

typedef struct {
	char *name;
	char *data_role;
	char *port_type;
	char *power_role;
	char *power_operation_mode;
} fwts_typec_config;

typedef struct {
	char *name;
	char *model;
	char *state;
	char *vendor;
} fwts_scsi_disk_config;

typedef struct {
	char *name;
	char *status;
	char *enabled;
} fwts_drm_config;

/* compare H/W info */
typedef int (*hwinfo_cmp)(void *data1, void *data2);

/* dump H/W info */
typedef void (*hwinfo_dump)(fwts_framework *fw, fwts_list *list);

static char *fwts_hwinfo_data_get(const char *sys, const char *dev, const char *file)
{
	char path[PATH_MAX];
	char *data;

	snprintf(path, sizeof(path), "%s/%s/%s", sys, dev, file);
	if ((data = fwts_get(path)) == NULL)
		return strdup("");

	fwts_chop_newline(data);
	return data;
}


/*
 *  fwts_hwinfo_bluetooth_free()
 *	free bluetooth data
 */
static void fwts_hwinfo_bluetooth_free(void *data)
{
	fwts_bluetooth_config *config = (fwts_bluetooth_config *)data;

	free(config->name);
	free(config->bt_name);
	free(config->address);
	free(config->bus);
	free(config->type);
	free(config);
}

/*
 *  fwts_hwinfo_bluetooth_name_cmp()
 *	compare bluetooth config device name for sorting purposes
 */
static int fwts_hwinfo_bluetooth_name_cmp(void *data1, void *data2)
{
	fwts_bluetooth_config *config1 = (fwts_bluetooth_config *)data1;
	fwts_bluetooth_config *config2 = (fwts_bluetooth_config *)data2;

	return strcmp(config1->name, config2->name);
}

/*
 *  fwts_hwinfo_bluetooth_cmp()
 *	compare bluetooth config data
 */
static int fwts_hwinfo_bluetooth_config_cmp(void *data1, void *data2)
{
	fwts_bluetooth_config *config1 = (fwts_bluetooth_config *)data1;
	fwts_bluetooth_config *config2 = (fwts_bluetooth_config *)data2;

	return strcmp(config1->name, config2->name) ||
	       strcmp(config1->bt_name, config2->bt_name) ||
	       strcmp(config1->address, config2->address) ||
	       strcmp(config1->bus, config2->bus) ||
	       strcmp(config1->type, config2->type);
}

/*
 *  fwts_hwinfo_bluetooth_get()
 * 	read a specific bluetooth device config
 */
static int fwts_hwinfo_bluetooth_get(
	fwts_framework *fw,
	fwts_list *devices)
{
	DIR *dp;
	struct dirent *d;

	fwts_list_init(devices);
	if ((dp = opendir(FWTS_HWINFO_SYS_BLUETOOTH)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan bluetooth devices.", FWTS_HWINFO_SYS_BLUETOOTH);
		return FWTS_ERROR;
	}

	while ((d = readdir(dp)) != NULL) {
		fwts_bluetooth_config *bluetooth_config;

		if (d->d_name[0] == '.')
			continue;

		if ((bluetooth_config = calloc(1, sizeof(*bluetooth_config))) == NULL) {
			fwts_log_error(fw, "Cannot allocate bluetooth config data.");
			break;
		}
		bluetooth_config->name = strdup(d->d_name);
		bluetooth_config->bt_name = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_BLUETOOTH, d->d_name, "name");
		bluetooth_config->address = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_BLUETOOTH, d->d_name, "address");
		bluetooth_config->bus = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_BLUETOOTH, d->d_name, "bus");
		bluetooth_config->type = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_BLUETOOTH, d->d_name, "type");

		if (bluetooth_config->name == NULL ||
		    bluetooth_config->bt_name == NULL ||
		    bluetooth_config->address == NULL ||
		    bluetooth_config->bus == NULL ||
		    bluetooth_config->type == NULL) {
			fwts_log_error(fw, "Cannot allocate bluetooth device attributes.");
			fwts_hwinfo_bluetooth_free(bluetooth_config);
			break;
		}
		fwts_list_add_ordered(devices, bluetooth_config, fwts_hwinfo_bluetooth_name_cmp);
	}
	(void)closedir(dp);
	return FWTS_OK;
}

/*
 *  fwts_hwinfo_bluetooth_dump()
 *	simple bluetooth config dump
 */
static void fwts_hwinfo_bluetooth_dump(fwts_framework *fw, fwts_list *devices)
{
	fwts_list_link *item;

	fwts_list_foreach(item, devices) {
		fwts_bluetooth_config *bluetooth_config = fwts_list_data(fwts_bluetooth_config *, item);

		fwts_log_info_verbatim(fw, "  Device:  %s", bluetooth_config->name);
		fwts_log_info_verbatim(fw, "  Name:    %s", bluetooth_config->bt_name);
		fwts_log_info_verbatim(fw, "  Address: %s", bluetooth_config->address);
		fwts_log_info_verbatim(fw, "  Bus:     %s", bluetooth_config->bus);
		fwts_log_info_verbatim(fw, "  Type:    %s", bluetooth_config->type);
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_input_free()
 *	free input data
 */
static void fwts_hwinfo_input_free(void *data)
{
	fwts_input_config *config = (fwts_input_config *)data;

	free(config->name);
	free(config->dev_name);
	free(config->phys);
	free(config);
}

/*
 *  fwts_hwinfo_input_name_cmp()
 *	compare input config device class name for sorting purposes
 */
static int fwts_hwinfo_input_name_cmp(void *data1, void *data2)
{
	fwts_input_config *config1 = (fwts_input_config *)data1;
	fwts_input_config *config2 = (fwts_input_config *)data2;

	return strcmp(config1->name, config2->name);
}

/*
 *  fwts_hwinfo_input_cmp()
 *	compare input config data
 */
static int fwts_hwinfo_input_config_cmp(void *data1, void *data2)
{
	fwts_input_config *config1 = (fwts_input_config *)data1;
	fwts_input_config *config2 = (fwts_input_config *)data2;

	return strcmp(config1->name, config2->name) ||
	       strcmp(config1->dev_name, config2->dev_name) ||
	       strcmp(config1->phys, config2->phys);
}

/*
 *  fwts_hwinfo_input_get()
 * 	read a specific input device config
 */
static int fwts_hwinfo_input_get(
	fwts_framework *fw,
	fwts_list *devices)
{
	DIR *dp;
	struct dirent *d;

	fwts_list_init(devices);
	if ((dp = opendir(FWTS_HWINFO_SYS_INPUT)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan input devices.", FWTS_HWINFO_SYS_INPUT);
		return FWTS_ERROR;
	}

	while ((d = readdir(dp)) != NULL) {
		fwts_input_config *input_config;

		if (d->d_name[0] == '.')
			continue;
		if (!strncmp(d->d_name, "input", 5)) {
			if ((input_config = calloc(1, sizeof(*input_config))) == NULL) {
				fwts_log_error(fw, "Cannot allocate input config data.");
				break;
			}
			input_config->dev_name = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_INPUT, d->d_name, "name");
			input_config->phys = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_INPUT, d->d_name, "phys");
		} else if (!strncmp(d->d_name, "event", 5) ||
			   !strncmp(d->d_name, "mouse", 5)) {
			if ((input_config = calloc(1, sizeof(*input_config))) == NULL) {
				fwts_log_error(fw, "Cannot allocate input config data.");
				break;
			}
			input_config->dev_name = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_INPUT, d->d_name, "device/name");
			input_config->phys = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_INPUT, d->d_name, "device/phys");
		} else {
			/* Don't know what type it is, ignore */
			continue;
		}

		input_config->name = strdup(d->d_name);

		if (input_config->name == NULL ||
		    input_config->dev_name == NULL ||
		    input_config->phys == NULL) {
			fwts_log_error(fw, "Cannot allocate input device attributes.");
			fwts_hwinfo_input_free(input_config);
			break;
		}
		fwts_list_add_ordered(devices, input_config, fwts_hwinfo_input_name_cmp);
	}
	(void)closedir(dp);
	return FWTS_OK;
}

/*
 *  fwts_hwinfo_input_dump()
 *	simple input config dump
 */
static void fwts_hwinfo_input_dump(fwts_framework *fw, fwts_list *devices)
{
	fwts_list_link *item;

	fwts_list_foreach(item, devices) {
		fwts_input_config *input_config = fwts_list_data(fwts_input_config *, item);

		fwts_log_info_verbatim(fw, "  Device:      %s", input_config->name);
		fwts_log_info_verbatim(fw, "  Device Name: %s", input_config->dev_name);
		fwts_log_info_verbatim(fw, "  Phy:         %s", input_config->phys);
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_net_free()
 *	free net config data
 */
static void fwts_hwinfo_net_free(void *data)
{
	fwts_net_config *config = (fwts_net_config *)data;

	free(config->name);
	free(config->addr);
	free(config->hw_addr);
	free(config);
}

/*
 *  fwts_hwinfo_net_name_cmp()
 *	compare net config device name for sorting purposes
 */
static int fwts_hwinfo_net_name_cmp(void *data1, void *data2)
{
	fwts_net_config *config1 = (fwts_net_config *)data1;
	fwts_net_config *config2 = (fwts_net_config *)data2;

	return strcmp(config1->name, config2->name);
}

/*
 *  fwts_hwinfo_net_cmp()
 *	compare net config data
 */
static int fwts_hwinfo_net_cmp(void *data1, void *data2)
{
	fwts_net_config *config1 = (fwts_net_config *)data1;
	fwts_net_config *config2 = (fwts_net_config *)data2;

	return strcmp(config1->name, config2->name) ||
	       strcmp(config1->hw_addr, config2->hw_addr);
}

/*
 *  fwts_hwinfo_new_get()
 * 	read a specific network interface config
 */
static int fwts_hwinfo_net_get(
	fwts_framework *fw,
	fwts_list *devices)
{
	DIR *dp;
	struct dirent *d;
	int sock;

	fwts_list_init(devices);
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		fwts_log_error(fw, "Cannot open socket to interrogate network devices.");
		return FWTS_ERROR;
	}

	if ((dp = opendir(FWTS_HWINFO_SYS_NET)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan network devices.", FWTS_HWINFO_SYS_NET);
		(void)close(sock);
		return FWTS_ERROR;
	}

	while ((d = readdir(dp)) != NULL) {
		struct ifreq buf;
		struct sockaddr_in sockaddr;
		fwts_net_config *net_config;

		if (d->d_name[0] == '.')
			continue;
		if ((net_config = calloc(1, sizeof(*net_config))) == NULL) {
			fwts_log_error(fw, "Cannot allocate net config data.");
			break;
		}
		net_config->name = strdup(d->d_name);
		if (net_config->name == NULL) {
			fwts_log_error(fw, "Cannot allocate net config name.");
			fwts_hwinfo_net_free(net_config);
			break;
		}
		memset(&buf, 0, sizeof(buf));
		strlcpy(buf.ifr_name, d->d_name, sizeof(buf.ifr_name));
		if (ioctl(sock, SIOCGIFHWADDR, &buf) < 0) {
			fwts_log_error(fw, "Cannot get network information for device %s.", d->d_name);
			fwts_hwinfo_net_free(net_config);
			continue;
		}
		net_config->hw_addr = strdup(ether_ntoa(((struct ether_addr *)&buf.ifr_hwaddr.sa_data)));
		if (net_config->hw_addr == NULL) {
			fwts_log_error(fw, "Cannot allocate net config address.");
			fwts_hwinfo_net_free(net_config);
			break;
		}
		memset(&buf, 0, sizeof(buf));
		strlcpy(buf.ifr_name, d->d_name, sizeof(buf.ifr_name));
		if (ioctl(sock, SIOCGIFADDR, &buf) < 0) {
			if (errno != EADDRNOTAVAIL)
				fwts_log_error(fw, "Cannot get address for device %s.", d->d_name);
		}
		/* GCC 4.4 is rather overly pedantic in strict aliasing warnings, this avoids it */
		memcpy(&sockaddr, &buf.ifr_addr, sizeof(sockaddr));
		net_config->addr = strdup(inet_ntoa((struct in_addr)sockaddr.sin_addr));
		if (net_config->addr == NULL) {
			fwts_log_error(fw, "Cannot allocate net config H/W address.");
			fwts_hwinfo_net_free(net_config);
			break;
		}
		fwts_list_add_ordered(devices, net_config, fwts_hwinfo_net_name_cmp);
	}
	(void)closedir(dp);
	(void)close(sock);

	return FWTS_OK;
}

/*
 *  fwts_hwinfo_net_dump()
 *	simple net config dump
 */
static void fwts_hwinfo_net_dump(fwts_framework *fw, fwts_list *devices)
{
	fwts_list_link *item;

	fwts_list_foreach(item, devices) {
		fwts_net_config *net_config = fwts_list_data(fwts_net_config *, item);

		fwts_log_info_verbatim(fw, "  Device:      %s", net_config->name);
		fwts_log_info_verbatim(fw, "  Address:     %s", net_config->addr);
		fwts_log_info_verbatim(fw, "  H/W Address: %s", net_config->hw_addr);
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_pci_get()
 * 	read a specific PCI config based on class code,
 *	retuning matching PCI configs into the configs list
 */
static int fwts_hwinfo_pci_get(
	fwts_framework *fw,
	const uint8_t class_code,
	fwts_list *configs)
{
	DIR *pci;
	struct dirent *d;

	fwts_list_init(configs);
	if ((pci = opendir(FWTS_PCI_DEV_PATH)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan PCI devices.", FWTS_PCI_DEV_PATH);
		return FWTS_ERROR;
	}

	while ((d = readdir(pci)) != NULL) {
		ssize_t	n;
		int	fd;
		fwts_pci_config *pci_config;
		uint8_t	config[64];
		char 	path[PATH_MAX];

		if (d->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), FWTS_PCI_DEV_PATH "/%s/config", d->d_name);

		if ((fd = open(path, O_RDONLY)) < 0) {
			fwts_log_error(fw, "Cannot open PCI config from %s.", d->d_name);
			continue;
		}

		if ((n = read(fd, config, sizeof(config))) < 0) {
			fwts_log_error(fw, "Cannot read PCI config from %s.", d->d_name);
			(void)close(fd);
			continue;
		}
		(void)close(fd);

		if (config[FWTS_PCI_CONFIG_CLASS_CODE] != class_code)
			continue;

		if ((pci_config = (fwts_pci_config *)calloc(1, sizeof(*pci_config))) == NULL) {
			fwts_log_error(fw, "Cannot allocate PCI config.");
			continue;
		}
		memcpy(pci_config->config, config, n);
		pci_config->config_len = n;
		strlcpy(pci_config->name, d->d_name, sizeof(pci_config->name));

		fwts_list_append(configs, pci_config);
	}
	(void)closedir(pci);

	return FWTS_OK;
}

/*
 *  fwts_hwinfo_pci_config_cmp()
 *	compare PCI configs
 */
static int fwts_hwinfo_pci_config_cmp(void *data1, void *data2)
{
	fwts_pci_config *pci_config1 = (fwts_pci_config *)data1;
	fwts_pci_config *pci_config2 = (fwts_pci_config *)data2;

	return (pci_config1->config_len != pci_config2->config_len ||
	    memcmp(pci_config1->config, pci_config2->config, pci_config1->config_len) ||
	    strcmp(pci_config1->name, pci_config2->name));
}

/*
 *  fwts_hwinfo_pci_dump()
 *	simple PCI config dump
 */
static void fwts_hwinfo_pci_dump(fwts_framework *fw, fwts_list *configs)
{
	fwts_list_link *item;

	fwts_list_foreach(item, configs) {
		ssize_t n;

		fwts_pci_config *pci_config = fwts_list_data(fwts_pci_config *, item);
		fwts_log_info_verbatim(fw, "  PCI: %s, VID: %2.2" PRIx8 ":%2.2" PRIx8
			", Class: %2.2" PRIx8 ":%2.2" PRIx8 " (%s)",
			pci_config->name,
			pci_config->config[FWTS_PCI_CONFIG_VENDOR_ID],
			pci_config->config[FWTS_PCI_CONFIG_DEVICE_ID],
			pci_config->config[FWTS_PCI_CONFIG_CLASS_CODE],
			pci_config->config[FWTS_PCI_CONFIG_SUBCLASS],
			fwts_pci_description(pci_config->config[FWTS_PCI_CONFIG_CLASS_CODE],
				pci_config->config[FWTS_PCI_CONFIG_SUBCLASS]));
		fwts_log_info_verbatim(fw, "  Config:");

		/* and do a raw hex dump of the config space */
		for (n = 0; n < pci_config->config_len; n += 16) {
			ssize_t left = pci_config->config_len - n;
			char buffer[128];

			fwts_dump_raw_data(buffer, sizeof(buffer), pci_config->config + n,
				n, left > 16 ? 16 : left);
			fwts_log_info_verbatim(fw, "  %s", buffer);
		}
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_typec_free()
 *	free Type-C data
 */
static void fwts_hwinfo_typec_free(void *data)
{
	fwts_typec_config *config = (fwts_typec_config *)data;

	free(config->name);
	free(config->data_role);
	free(config->port_type);
	free(config->power_role);
	free(config->power_operation_mode);
	free(config);
}

/*
 *  fwts_hwinfo_typec_cmp()
 *	compare Type-C config data
 */
static int fwts_hwinfo_typec_config_cmp(void *data1, void *data2)
{

	fwts_typec_config *config1 = (fwts_typec_config *)data1;
	fwts_typec_config *config2 = (fwts_typec_config *)data2;

	return strcmp(config1->name, config2->name) ||
	       strcmp(config1->data_role, config2->data_role) ||
	       strcmp(config1->port_type, config2->port_type) ||
	       strcmp(config1->power_role, config2->power_role) ||
	       strcmp(config1->power_operation_mode, config2->power_operation_mode);
}

/*
 *  fwts_hwinfo_typec_get()
 * 	read a specific Type-C device config
 */
static int fwts_hwinfo_typec_get(
	fwts_framework *fw,
	fwts_list *devices)
{
	DIR *dp;
	struct dirent *d;

	fwts_list_init(devices);
	if ((dp = opendir(FWTS_HWINFO_SYS_TYPEC)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan Type-C devices.", FWTS_HWINFO_SYS_TYPEC);
		return FWTS_ERROR;
	}

	while ((d = readdir(dp)) != NULL) {
		fwts_typec_config *typec_config;

		if (d->d_name[0] == '.')
			continue;

		if ((typec_config = calloc(1, sizeof(*typec_config))) == NULL) {
			fwts_log_error(fw, "Cannot allocate Type-C config data.");
			break;
		}
		typec_config->name = strdup(d->d_name);
		typec_config->data_role = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_TYPEC, d->d_name, "data_role");
		typec_config->port_type = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_TYPEC, d->d_name, "port_type");
		typec_config->power_role = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_TYPEC, d->d_name, "power_role");
		typec_config->power_operation_mode = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_TYPEC, d->d_name, "power_operation_mode");

		if (typec_config->name == NULL ||
		    typec_config->data_role == NULL ||
		    typec_config->port_type == NULL ||
		    typec_config->power_role == NULL ||
		    typec_config->power_operation_mode == NULL) {
			fwts_log_error(fw, "Cannot allocate Type-C device attributes.");
			fwts_hwinfo_typec_free(typec_config);
			break;
		}
		fwts_list_append(devices, typec_config);
	}
	(void)closedir(dp);

	return FWTS_OK;
}

/*
 *  fwts_hwinfo_typec_dump()
 *	simple Type-C config dump
 */
static void fwts_hwinfo_typec_dump(fwts_framework *fw, fwts_list *devices)
{
	fwts_list_link *item;
	fwts_list_foreach(item, devices) {
		fwts_typec_config *typec_config = fwts_list_data(fwts_typec_config *, item);

		fwts_log_info_verbatim(fw, "  Name:       %s", typec_config->name);
		fwts_log_info_verbatim(fw, "  Data Role:  %s", typec_config->data_role);
		fwts_log_info_verbatim(fw, "  Port Type:  %s", typec_config->port_type);
		fwts_log_info_verbatim(fw, "  Power Role: %s", typec_config->power_role);
		fwts_log_info_verbatim(fw, "  Power Mode: %s", typec_config->power_operation_mode);
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_scsi_disk_free()
 *	free SCSI disk data
 */
static void fwts_hwinfo_scsi_disk_free(void *data)
{
	fwts_scsi_disk_config *config = (fwts_scsi_disk_config *)data;

	free(config->name);
	free(config->model);
	free(config->state);
	free(config->vendor);
	free(config);
}

/*
 *  fwts_hwinfo_scsi_disk_cmp()
 *	compare SCSI disk config data
 */
static int fwts_hwinfo_scsi_disk_config_cmp(void *data1, void *data2)
{

	fwts_scsi_disk_config *config1 = (fwts_scsi_disk_config *)data1;
	fwts_scsi_disk_config *config2 = (fwts_scsi_disk_config *)data2;

	return strcmp(config1->name, config2->name) ||
	       strcmp(config1->model, config2->model) ||
	       strcmp(config1->state, config2->state) ||
	       strcmp(config1->vendor, config2->vendor);
}

/*
 *  fwts_hwinfo_scsi_disk_get()
 * 	read a specific SCSI disk device config
 */
static int fwts_hwinfo_scsi_disk_get(
	fwts_framework *fw,
	fwts_list *devices)
{
	DIR *dp;
	struct dirent *d;

	fwts_list_init(devices);
	if ((dp = opendir(FWTS_HWINFO_SYS_SCSI_DISK)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan SCSI disk devices.", FWTS_HWINFO_SYS_SCSI_DISK);
		return FWTS_ERROR;
	}

	while ((d = readdir(dp)) != NULL) {
		fwts_scsi_disk_config *scsi_disk_config;

		if (d->d_name[0] == '.')
			continue;

		if ((scsi_disk_config = calloc(1, sizeof(*scsi_disk_config))) == NULL) {
			fwts_log_error(fw, "Cannot allocate SCSI disk config data.");
			break;
		}
		scsi_disk_config->name = strdup(d->d_name);
		scsi_disk_config->model = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_SCSI_DISK, d->d_name, "device/model");
		scsi_disk_config->state = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_SCSI_DISK, d->d_name, "device/state");
		scsi_disk_config->vendor = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_SCSI_DISK, d->d_name, "device/vendor");

		if (scsi_disk_config->name == NULL ||
		    scsi_disk_config->model == NULL ||
		    scsi_disk_config->state == NULL ||
		    scsi_disk_config->vendor == NULL) {
			fwts_log_error(fw, "Cannot allocate SCSI disk device attributes.");
			fwts_hwinfo_scsi_disk_free(scsi_disk_config);
			break;
		}
		fwts_list_append(devices, scsi_disk_config);
	}
	(void)closedir(dp);

	return FWTS_OK;
}

/*
 *  fwts_hwinfo_scsi_disk_dump()
 *	simple SCSI disk config dump
 */
static void fwts_hwinfo_scsi_disk_dump(fwts_framework *fw, fwts_list *devices)
{
	fwts_list_link *item;
	fwts_list_foreach(item, devices) {
		fwts_scsi_disk_config *scsi_disk_config = fwts_list_data(fwts_scsi_disk_config *, item);

		fwts_log_info_verbatim(fw, "  Name:       %s", scsi_disk_config->name);
		fwts_log_info_verbatim(fw, "  Vendor:     %s", scsi_disk_config->vendor);
		fwts_log_info_verbatim(fw, "  Model:      %s", scsi_disk_config->model);
		fwts_log_info_verbatim(fw, "  State:      %s", scsi_disk_config->state);
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_drm_free()
 *	free DRM data
 */
static void fwts_hwinfo_drm_free(void *data)
{
	fwts_drm_config *config = (fwts_drm_config *)data;

	free(config->name);
	free(config->status);
	free(config->enabled);
	free(config);
}

/*
 *  fwts_hwinfo_drm_cmp()
 *	compare DRM config data
 */
static int fwts_hwinfo_drm_config_cmp(void *data1, void *data2)
{

	fwts_drm_config *config1 = (fwts_drm_config *)data1;
	fwts_drm_config *config2 = (fwts_drm_config *)data2;

	return strcmp(config1->name, config2->name) ||
	       strcmp(config1->status, config2->status) ||
	       strcmp(config1->enabled, config2->enabled);
}

/*
 *  fwts_hwinfo_drm_get()
 * 	read a specific DRM device config
 */
static int fwts_hwinfo_drm_get(
	fwts_framework *fw,
	fwts_list *devices)
{
	DIR *dp;
	struct dirent *d;

	fwts_list_init(devices);
	if ((dp = opendir(FWTS_HWINFO_SYS_DRM)) == NULL) {
		fwts_log_error(fw, "Cannot open %s to scan DRM devices.", FWTS_HWINFO_SYS_DRM);
		return FWTS_ERROR;
	}

	while ((d = readdir(dp)) != NULL) {
		fwts_drm_config *drm_config;

		if (d->d_name[0] == '.')
			continue;

		if (strncmp(d->d_name, "card", strlen("card")) != 0)
			continue;

		if ((drm_config = calloc(1, sizeof(*drm_config))) == NULL) {
			fwts_log_error(fw, "Cannot allocate DRM config data.");
			break;
		}
		drm_config->name = strdup(d->d_name);
		drm_config->status = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_DRM, d->d_name, "status");
		drm_config->enabled = fwts_hwinfo_data_get(FWTS_HWINFO_SYS_DRM, d->d_name, "enabled");

		if (drm_config->name == NULL ||
		    drm_config->status == NULL ||
		    drm_config->enabled == NULL) {
			fwts_log_error(fw, "Cannot allocate DRM device attributes.");
			fwts_hwinfo_drm_free(drm_config);
			break;
		}
		fwts_list_append(devices, drm_config);
	}
	(void)closedir(dp);

	return FWTS_OK;
}

/*
 *  fwts_hwinfo_drm_dump()
 *	simple SCSI disk config dump
 */
static void fwts_hwinfo_drm_dump(fwts_framework *fw, fwts_list *devices)
{
	fwts_list_link *item;
	fwts_list_foreach(item, devices) {
		fwts_drm_config *drm_config = fwts_list_data(fwts_drm_config *, item);

		fwts_log_info_verbatim(fw, "  Name:       %s", drm_config->name);
		fwts_log_info_verbatim(fw, "  Status:     %s", drm_config->status);
		fwts_log_info_verbatim(fw, "  Enabled:    %s", drm_config->enabled);
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_hwinfo_lists_dump()
 *	dump out contents of two different lists
 */
static void fwts_hwinfo_lists_dump(
	fwts_framework *fw,
	hwinfo_dump dump,
	fwts_list *l1,
	fwts_list *l2,
	const char *message)
{
	fwts_log_info(fw, "%s configurations differ, before:", message);
	if (fwts_list_len(l1))
		dump(fw, l1);
	else
		fwts_log_info(fw, "  (empty)");

	fwts_log_info(fw, "versus after:");
	if (fwts_list_len(l2))
		dump(fw, l2);
	else
		fwts_log_info(fw, "  (empty)");

	fwts_log_nl(fw);
}

/*
 *  fwts_hwinfo_lists_differ()
 *	check lists to see if their contents differ, return 1 for differ, 0 for match,
 *	-1 for out of memory error
 */
static int fwts_hwinfo_lists_differ(
	hwinfo_cmp cmp,
	fwts_list *l1,
	fwts_list *l2)
{
	fwts_list_link *item1;
	fwts_list_link *item2;

	/* Both null - both the same then */
	if ((l1 == NULL) && (l2 == NULL))
		return FWTS_HWINFO_LISTS_SAME;

	/* Either null and the other not? */
	if ((l1 == NULL) || (l2 == NULL))
		return FWTS_HWINFO_LISTS_DIFFER;

	/* Different length, then they differ */
	if (fwts_list_len(l1) != fwts_list_len(l2))
		return FWTS_HWINFO_LISTS_DIFFER;

	item1 = fwts_list_head(l1);
	item2 = fwts_list_head(l2);

	while ((item1 != NULL) && (item2 != NULL)) {
		if (cmp(fwts_list_data(void *, item1), fwts_list_data(void *, item2)))
			return FWTS_HWINFO_LISTS_DIFFER;
		item1 = fwts_list_next(item1);
		item2 = fwts_list_next(item2);
	}

	if ((item1 == NULL) && (item2 == NULL))
		return FWTS_HWINFO_LISTS_SAME;
	else
		return FWTS_HWINFO_LISTS_DIFFER;
}

/*
 *  fwts_hwinfo_lists_compare()
 *	check for differences in a list and if any found, dump out both lists
 */
static void fwts_hwinfo_lists_compare(
	fwts_framework *fw,
	hwinfo_cmp cmp,
	hwinfo_dump dump,
	fwts_list *l1,
	fwts_list *l2,
	const char *message,
	int *differences)
{
	if (fwts_hwinfo_lists_differ(cmp, l1, l2) == FWTS_HWINFO_LISTS_DIFFER) {
		(*differences)++;
		fwts_hwinfo_lists_dump(fw, dump, l1, l2, message);
	}
}

/*
 *  fwts_hwinfo_get()
 *	gather H/W information
 */
int fwts_hwinfo_get(fwts_framework *fw, fwts_hwinfo *hwinfo)
{
	FWTS_UNUSED(fw);

	/* PCI devices */
	fwts_hwinfo_pci_get(fw, FWTS_PCI_CLASS_CODE_NETWORK_CONTROLLER, &hwinfo->network);
	fwts_hwinfo_pci_get(fw, FWTS_PCI_CLASS_CODE_DISPLAY_CONTROLLER, &hwinfo->videocard);
	/* Network devices */
	fwts_hwinfo_net_get(fw, &hwinfo->netdevs);
	fwts_hwinfo_input_get(fw, &hwinfo->input);
	/* Bluetooth devices */
	fwts_hwinfo_bluetooth_get(fw, &hwinfo->bluetooth);
	/* Type-C devices */
	fwts_hwinfo_typec_get(fw, &hwinfo->typec);
	/* SCSI disk devices */
	fwts_hwinfo_scsi_disk_get(fw, &hwinfo->scsi_disk);
	/* DRM devices */
	fwts_hwinfo_drm_get(fw, &hwinfo->drm);

	return FWTS_OK;
}

/*
 *  fwts_hwinfo_compare()
 *	compare data in each hwinfo list, produce a diff comparison output
 */
void fwts_hwinfo_compare(fwts_framework *fw, fwts_hwinfo *hwinfo1, fwts_hwinfo *hwinfo2, int *differences)
{
	*differences = 0;

	/* PCI devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_pci_config_cmp, fwts_hwinfo_pci_dump,
		&hwinfo1->network, &hwinfo2->network, "Network Controller", differences);
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_pci_config_cmp, fwts_hwinfo_pci_dump,
		&hwinfo1->videocard, &hwinfo2->videocard, "Video", differences);
	/* Network devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_net_cmp, fwts_hwinfo_net_dump,
		&hwinfo1->netdevs, &hwinfo2->netdevs, "Network", differences);
	/* Input devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_input_config_cmp, fwts_hwinfo_input_dump,
		&hwinfo1->input, &hwinfo2->input, "Input Devices", differences);
	/* Bluetooth devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_bluetooth_config_cmp, fwts_hwinfo_bluetooth_dump,
		&hwinfo1->bluetooth, &hwinfo2->bluetooth, "Bluetooth Device", differences);
	/* Type-C devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_typec_config_cmp, fwts_hwinfo_typec_dump,
		&hwinfo1->typec, &hwinfo2->typec, "Type-C Device", differences);
	/* SCSI disk devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_scsi_disk_config_cmp, fwts_hwinfo_scsi_disk_dump,
		&hwinfo1->scsi_disk, &hwinfo2->scsi_disk, "SCSI Disk Device", differences);
	/* DRM devices */
	fwts_hwinfo_lists_compare(fw, fwts_hwinfo_drm_config_cmp, fwts_hwinfo_drm_dump,
		&hwinfo1->drm, &hwinfo2->drm, "DRM Device", differences);

}

/*
 *  fwts_hwinfo_free()
 *	free hwinfo lists
 */
int fwts_hwinfo_free(fwts_hwinfo *hwinfo)
{
	if (hwinfo == NULL)
		return FWTS_ERROR;

	/* PCI devices */
	fwts_list_free_items(&hwinfo->network, free);
	fwts_list_free_items(&hwinfo->videocard, free);
	/* Network devices */
	fwts_list_free_items(&hwinfo->netdevs, fwts_hwinfo_net_free);
	/* Input devices */
	fwts_list_free_items(&hwinfo->input, fwts_hwinfo_input_free);
	/* Bluetooth devices */
	fwts_list_free_items(&hwinfo->bluetooth, fwts_hwinfo_bluetooth_free);
	/* Type-C devices */
	fwts_list_free_items(&hwinfo->typec, fwts_hwinfo_typec_free);
	/* SCSI disk devices */
	fwts_list_free_items(&hwinfo->scsi_disk, fwts_hwinfo_scsi_disk_free);
	/* DRM devices */
	fwts_list_free_items(&hwinfo->drm, fwts_hwinfo_drm_free);

	return FWTS_OK;
}
