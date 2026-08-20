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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "fwts.h"
#include "fwts_uefi.h"
#include "fwts_efi_runtime.h"

/* Old sysfs uefi packed binary blob variables */
typedef struct {
	uint16_t	varname[512];
	uint8_t		guid[16];
	uint64_t	datalen;
	uint8_t		data[1024];
	uint64_t	status;
	uint32_t	attributes;
} __attribute__((packed)) fwts_uefi_sys_fs_var;

/* New efifs variable */
typedef struct {
	uint32_t	attributes;
	uint8_t		data[0];	/* variable length, depends on file size */
} __attribute__((packed)) fwts_uefi_efivars_fs_var;

typedef struct {
	const uint64_t statusvalue;
	const char *mnemonic ;
	const char *description;
} uefistatus_info;

/* Which interface are we using? */
#define UEFI_IFACE_UNKNOWN 		(0)	/* Not yet known */
#define UEFI_IFACE_NONE			(1)	/* None found */
#define UEFI_IFACE_SYSFS		(2)	/* sysfs */
#define UEFI_IFACE_EFIVARS		(3)	/* efivar fs */

/* File system magic numbers */
#define PSTOREFS_MAGIC          ((__SWORD_TYPE)0x6165676C)
#define EFIVARFS_MAGIC          ((__SWORD_TYPE)0xde5e81e4)
#define SYS_FS_MAGIC		((__SWORD_TYPE)0x62656572)

/*
 *  fwts_uefi_get_interface()
 *	find which type of EFI variable file system we are using,
 *	sets path to the name of the file system path, returns
 *	the file system interface (if found).
 */
static int fwts_uefi_get_interface(char **path)
{
	static int  efivars_interface = UEFI_IFACE_UNKNOWN;
	static char efivar_path[4096];

	FILE *fp;
	struct statfs statbuf;

	if (path == NULL)	/* Sanity check */
		return FWTS_ERROR;

	/* Already discovered, return the cached values */
	if (efivars_interface != UEFI_IFACE_UNKNOWN) {
		*path = efivar_path;
		return efivars_interface;
	}

	*efivar_path = '\0';	/* Assume none */

	/* Scan mounts to see if sysfs or efivar fs is somewhere else */
	if ((fp = fopen("/proc/mounts", "r")) != NULL) {
		while (!feof(fp)) {
			char mount[4096];
			char fstype[1024];

			if (fscanf(fp, "%*s %4095s %1023s", mount, fstype) == 2) {
				/* Always try to find the newer interface first */
				if (!strcmp(fstype, "efivarfs")) {
					strcpy(efivar_path, mount);
					break;
				}
				/* Failing that, look for sysfs, but don't break out
				   the loop as we need to keep on searching for efivar fs */
				if (!strcmp(fstype, "sysfs"))
					strcpy(efivar_path, "/sys/firmware/efi/vars");
			}
		}
		(void)fclose(fp);
	}

	*path = NULL;

	/* No mounted path found, bail out */
	if (*efivar_path == '\0') {
		efivars_interface = UEFI_IFACE_NONE;
		return UEFI_IFACE_NONE;
	}

	/* Can't stat it, bail out */
	if (statfs(efivar_path, &statbuf) < 0) {
		efivars_interface = UEFI_IFACE_NONE;
		return UEFI_IFACE_NONE;
	};

	/* We've now found a valid file system we can use */
	*path = efivar_path;

	if ((statbuf.f_type == EFIVARFS_MAGIC) ||
	    (statbuf.f_type == PSTOREFS_MAGIC)) {
		efivars_interface = UEFI_IFACE_EFIVARS;
		return UEFI_IFACE_EFIVARS;
	}

	if (statbuf.f_type == SYS_FS_MAGIC) {
		efivars_interface = UEFI_IFACE_SYSFS;
		return UEFI_IFACE_SYSFS;
	}

	return UEFI_IFACE_UNKNOWN;
}

/*
 *  fwts_uefi_str_to_str16()
 *	convert 8 bit C string to 16 bit string.
 */
void fwts_uefi_str_to_str16(uint16_t *dst, const size_t len, const char *src)
{
	size_t i = len;

	while ((*src) && (i > 1)) {
		*dst++ = *src++;
		i--;
	}
	*dst = '\0';
}

/*
 *  fwts_uefi_str16_to_str()
 *	convert 16 bit string to 8 bit C string.
 */
void fwts_uefi_str16_to_str(char *dst, const size_t len, const uint16_t *src)
{
	size_t i = len;

	while ((*src) && (i > 1)) {
		*dst++ = *(src++) & 0xff;
		i--;
	}
	*dst = '\0';
}

/*
 *  fwts_uefi_str16len()
 *	16 bit version of strlen()
 */
size_t fwts_uefi_str16len(const uint16_t *str)
{
	int i;

	for (i = 0; *str; i++, str++)
		;
	return i;
}

/*
 *  fwts_uefi_get_varname()
 *	fetch the UEFI variable name in terms of a 8 bit C string
 */
void fwts_uefi_get_varname(char *varname, const size_t len, const fwts_uefi_var *var)
{
	fwts_uefi_str16_to_str(varname, len, var->varname);
}

/*
 *  fwts_uefi_get_variable_sys_fs()
 *	fetch a UEFI variable given its name, via sysfs
 */
static int fwts_uefi_get_variable_sys_fs(const char *varname, fwts_uefi_var *var, char *path)
{
	int  fd;
	char filename[PATH_MAX];
	fwts_uefi_sys_fs_var	uefi_sys_fs_var;

	memset(var, 0, sizeof(fwts_uefi_var));

	snprintf(filename, sizeof(filename), "%s/%s/raw_var", path, varname);

	if ((fd = open(filename, O_RDONLY)) < 0)
		return FWTS_ERROR;

	memset(&uefi_sys_fs_var, 0, sizeof(uefi_sys_fs_var));

	/* Read the raw fixed sized data */
	if (read(fd, &uefi_sys_fs_var, sizeof(uefi_sys_fs_var)) != sizeof(uefi_sys_fs_var)) {
		(void)close(fd);
		return FWTS_ERROR;
	}
	(void)close(fd);

	/* Sanity check datalen is OK */
	if (uefi_sys_fs_var.datalen > sizeof(uefi_sys_fs_var.data))
		return FWTS_ERROR;

	/* Allocate space for the variable name */
	var->varname = calloc(1, sizeof(uefi_sys_fs_var.varname));
	if (var->varname == NULL)
		return FWTS_ERROR;

	/* Allocate space for the data */
	var->data = calloc(1, (size_t)uefi_sys_fs_var.datalen);
	if (var->data == NULL) {
		free(var->varname);
		return FWTS_ERROR;
	}

	/* And copy the data */
	memcpy(var->varname, uefi_sys_fs_var.varname, sizeof(uefi_sys_fs_var.varname));
	memcpy(var->data, uefi_sys_fs_var.data, uefi_sys_fs_var.datalen);
	memcpy(var->guid, uefi_sys_fs_var.guid, sizeof(var->guid));
	var->datalen = (size_t)uefi_sys_fs_var.datalen;
	var->attributes = uefi_sys_fs_var.attributes;
	var->status = uefi_sys_fs_var.status;

	return FWTS_OK;
}

/*
 *  fwts_uefi_get_variable_efivars_fs()
 *	fetch a UEFI variable given its name, via efivars fs
 */
static int fwts_uefi_get_variable_efivars_fs(const char *varname, fwts_uefi_var *var, char *path)
{
	int  fd;
	char filename[PATH_MAX];
	struct stat	statbuf;
	fwts_uefi_efivars_fs_var	*efivars_fs_var;
	size_t varname_len = strlen(varname);

	memset(var, 0, sizeof(fwts_uefi_var));

	/* Variable names include the GUID, so must be at least 36 chars long */

	if (varname_len < 36)
		return FWTS_ERROR;

	/* Get the GUID */
	fwts_guid_str_to_buf(varname + varname_len - 36, var->guid, sizeof(var->guid));

	snprintf(filename, sizeof(filename), "%s/%s", path, varname);

	if ((fd = open(filename, O_RDONLY)) < 0)
		return FWTS_ERROR;

	if (fstat(fd, &statbuf) < 0) {
		(void)close(fd);
		return FWTS_ERROR;
	}

	/* Variable name, less the GUID, in 16 bit ints */
	var->varname = calloc(1, (varname_len + 1 - 36)  * sizeof(uint16_t));
	if (var->varname == NULL) {
		(void)close(fd);
		return FWTS_ERROR;
	}

	/* Convert name to internal 16 bit version */
	fwts_uefi_str_to_str16(var->varname, varname_len - 36, varname);

	/* Need to read the data in one read, so allocate a buffer big enough */
	if ((efivars_fs_var = calloc(1, statbuf.st_size)) == NULL) {
		(void)close(fd);
		free(var->varname);
		return FWTS_ERROR;
	}

	if (read(fd, efivars_fs_var, statbuf.st_size) != statbuf.st_size) {
		(void)close(fd);
		free(var->varname);
		free(efivars_fs_var);
		return FWTS_ERROR;
	}
	(void)close(fd);

	var->status = 0;

	/*
	 *  UEFI variable data in efifs is:
	 *     4 bytes : attributes
	 *     N bytes : uefi variable contents
 	 */
	var->attributes = efivars_fs_var->attributes;

	var->datalen = statbuf.st_size - 4;
	if ((var->data = calloc(1, var->datalen)) == NULL) {
		free(var->varname);
		free(efivars_fs_var);
		return FWTS_ERROR;
	}
	memcpy(var->data, efivars_fs_var->data, var->datalen);

	free(efivars_fs_var);

	return FWTS_OK;
}

/*
 *  fwts_uefi_get_variable()
 *	fetch a UEFI variable given its name.
 */
int fwts_uefi_get_variable(const char *varname, fwts_uefi_var *var)
{
	char *path;

	if ((!varname) || (!var))	/* Sanity checks */
		return FWTS_ERROR;

	switch (fwts_uefi_get_interface(&path)) {
	case UEFI_IFACE_SYSFS:
		return fwts_uefi_get_variable_sys_fs(varname, var, path);
	case UEFI_IFACE_EFIVARS:
		return fwts_uefi_get_variable_efivars_fs(varname, var, path);
	default:
		return FWTS_ERROR;
	}
}

/*
 *  fwts_uefi_free_variable()
 *	free data and varname associated with a UEFI variable as
 *	fetched by fwts_uefi_get_variable.
 */
void fwts_uefi_free_variable(fwts_uefi_var *var)
{
	free(var->data);
	free(var->varname);
}

static int fwts_uefi_true_filter(const struct dirent *d)
{
	FWTS_UNUSED(d);

	return 1;
}

/*
 *  fwts_uefi_free_variable_names
 *	free the list of uefi variable names
 */
void fwts_uefi_free_variable_names(fwts_list *list)
{
	fwts_list_free_items(list, free);
}

/*
 *  fwts_uefi_get_variable_names
 *	gather a list of all the uefi variable names
 */
int fwts_uefi_get_variable_names(fwts_list *list)
{
	int i, n;
	struct dirent **names = NULL;
	char *path;
	char *name;
	int ret = FWTS_OK;

	fwts_list_init(list);

	switch (fwts_uefi_get_interface(&path)) {
	case UEFI_IFACE_SYSFS:
	case UEFI_IFACE_EFIVARS:
		break;
	default:
		return FWTS_ERROR;
	}

	/* Gather variable names in alphabetical order */
	n = scandir(path, &names, fwts_uefi_true_filter, alphasort);
	if (n < 0)
		return FWTS_ERROR;

	for (i = 0; i < n; i++) {
		if (names[i]->d_name[0] == '\0')
			continue;
		if (!strcmp(names[i]->d_name, "."))
			continue;
		if (!strcmp(names[i]->d_name, ".."))
			continue;

		name = strdup(names[i]->d_name);
		if (name == NULL) {
			ret = FWTS_ERROR;
			fwts_uefi_free_variable_names(list);
			break;
                } else
			fwts_list_append(list, name);
        }

	/* Free dirent names */
 	for (i = 0; i < n; i++)
		free(names[i]);
        free(names);

        return ret;
}

static const uefistatus_info uefistatus_info_table[] = {
	{ EFI_SUCCESS,			"EFI_SUCCESS",			"The operation completed successfully." },
	{ EFI_LOAD_ERROR,		"EFI_LOAD_ERROR",		"The image failed to load." },
	{ EFI_INVALID_PARAMETER,	"EFI_INVALID_PARAMETER",	"A parameter was incorrect." },
	{ EFI_UNSUPPORTED,		"EFI_UNSUPPORTED", 		"The operation is not supported." },
	{ EFI_BAD_BUFFER_SIZE,		"EFI_BAD_BUFFER_SIZE",		"The buffer was not the proper size for the request." },
	{ EFI_BUFFER_TOO_SMALL,		"EFI_BUFFER_TOO_SMALL",		"The buffer is not large enough to hold the requested data. The required buffer size is returned in the appropriate parameter when this error occurs." },
	{ EFI_NOT_READY,		"EFI_NOT_READY", 		"There is no data pending upon return." },
	{ EFI_DEVICE_ERROR,		"EFI_DEVICE_ERROR", 		"The physical device reported an error while attempting the operation." },
	{ EFI_WRITE_PROTECTED,		"EFI_WRITE_PROTECTED", 		"The device cannot be written to." },
	{ EFI_OUT_OF_RESOURCES,		"EFI_OUT_OF_RESOURCES", 	"A resource has run out." },
	{ EFI_VOLUME_CORRUPTED,		"EFI_VOLUME_CORRUPTED", 	"An inconstancy was detected on the file system causing the operating to fail." },
	{ EFI_VOLUME_FULL,		"EFI_VOLUME_FULL",		"There is no more space on the file system." },
	{ EFI_NO_MEDIA,			"EFI_NO_MEDIA",			"The device does not contain any medium to perform the operation." },
	{ EFI_MEDIA_CHANGED,		"EFI_MEDIA_CHANGED",		"The medium in the device has changed since the last access." },
	{ EFI_NOT_FOUND,		"EFI_NOT_FOUND",		"The item was not found." },
	{ EFI_ACCESS_DENIED,		"EFI_ACCESS_DENIED",		"Access was denied." },
	{ EFI_NO_RESPONSE,		"EFI_NO_RESPONSE",		"The server was not found or did not respond to the request." },
	{ EFI_NO_MAPPING,		"EFI_NO_MAPPING",		"A mapping to a device does not exist." },
	{ EFI_TIMEOUT,			"EFI_TIMEOUT",			"The timeout time expired." },
	{ EFI_NOT_STARTED,		"EFI_NOT_STARTED",		"The protocol has not been started." },
	{ EFI_ALREADY_STARTED,		"EFI_ALREADY_STARTED",		"The protocol has already been started." },
	{ EFI_ABORTED,			"EFI_ABORTED",			"The operation was aborted." },
	{ EFI_ICMP_ERROR,		"EFI_ICMP_ERROR",		"An ICMP error occurred during the network operation." },
	{ EFI_TFTP_ERROR,		"EFI_TFTP_ERROR",		"A TFTP error occurred during the network operation." },
	{ EFI_PROTOCOL_ERROR,		"EFI_PROTOCOL_ERROR",		"A protocol error occurred during the network operation." },
	{ EFI_INCOMPATIBLE_VERSION,	"EFI_INCOMPATIBLE_VERSION",	"The function encountered an internal version that was incompatible with a version requested by the caller." },
	{ EFI_SECURITY_VIOLATION,	"EFI_SECURITY_VIOLATION",	"The function was not performed due to a security violation." },
	{ EFI_CRC_ERROR,		"EFI_CRC_ERROR",		"A CRC error was detected." },
	{ EFI_END_OF_MEDIA,		"EFI_END_OF_MEDIA",		"Beginning or end of media was reached." },
	{ EFI_END_OF_FILE,		"EFI_END_OF_FILE",		"The end of the file was reached." },
	{ EFI_INVALID_LANGUAGE,		"EFI_INVALID_LANGUAGE",		"The language specified was invalid." },
	{ EFI_COMPROMISED_DATA,		"EFI_COMPROMISED_DATA",		"The security status of the data is unknown or compromised and the data must be updated or replaced to restore a valid security status." },
	{ EFI_IP_ADDRESS_CONFLICT,	"EFI_IP_ADDRESS_CONFLICT",	"There is an address conflict address allocation." },
	{ EFI_HTTP_ERROR,		"EFI_HTTP_ERROR",		"A HTTP error occurred during the network operation." },
	{ ~0, NULL, NULL }
};

void fwts_uefi_print_status_info(fwts_framework *fw, const uint64_t status)
{

	const uefistatus_info *info;

	if (status == ~0ULL) {
		fwts_log_info(fw, "fwts test ioctl() failed, errno=%d (%s)", errno, strerror(errno));
		return;
	}

	for (info = uefistatus_info_table; info->mnemonic != NULL; info++) {
		if (status == info->statusvalue) {
			fwts_log_info(fw, "Return status: %s. %s", info->mnemonic, info->description);
			return;
		}
	}
	fwts_log_info(fw, "Cannot find the return status information, value = 0x%" PRIx64 ".", status);

}

/*
 *  fwts_uefi_attribute_info()
 *	convert attribute into a human readable form
 */
char *fwts_uefi_attribute_info(uint32_t attr)
{
	static char str[100];

	*str = 0;

	if (attr & FWTS_UEFI_VAR_NON_VOLATILE)
		strcat(str, "NonVolatile");

	if (attr & FWTS_UEFI_VAR_BOOTSERVICE_ACCESS) {
		if (*str)
			strcat(str, ",");
		strcat(str, "BootServ");
	}

	if (attr & FWTS_UEFI_VAR_RUNTIME_ACCESS) {
		if (*str)
			strcat(str, ",");
		strcat(str, "RunTime");
	}

	if (attr & FWTS_UEFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) {
		if (*str)
			strcat(str, ",");
		strcat(str, "AuthenticatedWrite");
	}

	if (attr & FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) {
		if (*str)
			strcat(str, ",");
		strcat(str, "TimeBasedAuthenticatedWrite");
	}

	return str;
}

/*
 *  fwts_uefi_efivars_fs_exist()
 *	check the efivar interface exist
 */
bool fwts_uefi_efivars_iface_exist(void)
{
	char *path;

	return (fwts_uefi_get_interface(&path) == UEFI_IFACE_EFIVARS);

}

/*
 *  fwts_uefi_rt_support_status_get()
 *	get the status of runtime service support.
 *
 *	Since UEFI 2.8 Errata A the EFI_RT_PROPERTIES_TABLE configuration table
 *	can be used to indicate which UEFI runtime services are not implemented
 *	via the bitmask RuntimeServicesSupported. If the table is not present,
 *	all runtime services are to be considered available. Since Linux 5.11
 *	this bitmask can be read via an IOCTL call. Before Linux 5.11 the value
 *	cannot be determined.
 */
void fwts_uefi_rt_support_status_get(int fd, uint32_t *rtservicessupported)
{
	long ioret;

	ioret = ioctl(fd, EFI_RUNTIME_GET_SUPPORTED_MASK, rtservicessupported);
	if (ioret == -1)
		*rtservicessupported = EFI_RT_SUPPORTED_ALL;

	return;
}
