/*
 * Copyright (C) 2010-2026 Canonical
 * Some of this work - Copyright (C) 2016-2021 IBM
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

#define _GNU_SOURCE /* added for asprintf */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include "fwts.h"

#ifdef HAVE_MTD_MTD_ABI_H
#include <mtd/mtd-abi.h>
#endif

#ifdef HAVE_LIBFDT
#include <libfdt.h>
#endif

#define FDT_FLASH_PATH "/proc/device-tree/chosen/ibm,system-flash"
#define SYSFS_MTD_PATH "/sys/class/mtd"

static bool mtd_present(int fwts_mtd_flags, char *mtd_devnode)
{
	return !access(mtd_devnode, fwts_mtd_flags);
}

static int mtd_hdr_query(fwts_framework *fw, char *mtd_devnode) {

	/* snippet from skiboot libflash/ffs.h */
	struct ffs_hdr {
		char magic[8];
	};
	int fd = 0;
	ssize_t bytes_read = 0;
	struct ffs_hdr mtd_hdr;

	if ((fd = open(mtd_devnode, O_RDONLY)) < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL MTD Info",
			"Cannot get data from MTD device '%s'.",
			mtd_devnode);
		return FWTS_ERROR;
	}

	bytes_read = read(fd, &mtd_hdr, sizeof(mtd_hdr) );

	if (bytes_read >= 4) {
		/* FFS_MAGIC from skiboot libflash/ffs.h */
		if (strncmp(mtd_hdr.magic, "PART", 4) == 0) {
			fwts_log_info(fw,
				"MTD device '%s' header eye-catcher 'PART'"
				" verified.\n", mtd_devnode);
		} else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"OPAL MTD Info",
				"MTD device '%s' header eye-catcher 'PART'"
				" not able to be"
				" verified. Check the system setup.\n",
				mtd_devnode);
			(void)close(fd);
			return FWTS_ERROR;
		}
	} else {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL MTD Info",
			"MTD device '%s' header was unable to be read."
			" Cannot validate the integrity of the MTD."
			" Check the system setup.\n",
			mtd_devnode);
		(void)close(fd);
		return FWTS_ERROR;
	}

	(void)close(fd);
	return FWTS_OK;
}

static int mtd_dev_query(fwts_framework *fw, char *mtd_devnode)
{

#ifdef HAVE_MTD_MTD_ABI_H
	int fd = 0;
	int fwts_mtd_flags = 0;
	struct mtd_info_user mtd_info;
#endif

	if (!mtd_present(R_OK | W_OK, mtd_devnode)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL MTD Info",
			"Cannot read or write to MTD device '%s'"
			" check your user privileges.", mtd_devnode);
		return FWTS_ERROR;
	} else {
		fwts_log_info(fw, "MTD device '%s' is verified"
			" and %s is read/write in the file system, the"
			" MTD device itself will be checked later,"
			" see MTD Flags.",
			mtd_devnode, mtd_devnode);
	}

#ifdef HAVE_MTD_MTD_ABI_H
	if (strstr(mtd_devnode, "ro")) {
		fwts_mtd_flags = O_RDONLY;
	} else {
		fwts_mtd_flags = O_RDWR;
	}

	if ((fd = open(mtd_devnode, fwts_mtd_flags)) < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL MTD Info",
			"Cannot get data from '%s'"
			" device interface.", mtd_devnode);
		return FWTS_ERROR;
	}

	(void)memset(&mtd_info, 0, sizeof(mtd_info));
	if (ioctl(fd, MEMGETINFO, &mtd_info)) {
		(void)close(fd);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "OPAL MTD Info",
			"Cannot get data from '%s'"
			" device interface.", mtd_devnode);
		return FWTS_ERROR;
	} else {
		fwts_log_info(fw, "MTD device '%s' attributes follow:"
			" MTD Type=%u (3=MTD_NORFLASH),"
			" MTD Flags=%u (1024=MTD_WRITEABLE),"
			" MTD total size=%u bytes,"
			" MTD erase size=%u bytes,"
			" MTD write size=%u,"
			" MTD oob size=%u",
			mtd_devnode,
			mtd_info.type,
			mtd_info.flags,
			mtd_info.size,
			mtd_info.erasesize,
			mtd_info.writesize,
			mtd_info.oobsize);
		(void)close(fd);
		return FWTS_OK;
	}
}
#else
	fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MTD Info",
			"MTD Info unable to be retrieved"
			" for '%s' due to lack of "
			"mtd-abi.h, check your "
			"configuration and rebuild.",
			mtd_devnode);
	return FWTS_ERROR;
}
#endif

static int mtd_info_test1(fwts_framework *fw)
{
	char fdt_node_path[PATH_MAX + 1];
	int count, i, fd;
	ssize_t bytes = 0, bytes_read = 0;
	struct dirent **namelist;

	fd = open(FDT_FLASH_PATH, O_RDONLY);
	if (fd < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MTD Info",
			"Failed to open the path %s."
			" Check the installation"
			" for the path %s.\n",
			FDT_FLASH_PATH,
			FDT_FLASH_PATH);
		return FWTS_ERROR;
	}
	bytes_read = read(fd, fdt_node_path, PATH_MAX);
	if (bytes_read < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MTD Info",
			"Failed to get the FDT info."
			" Check the installation "
			"for the path %s.\n",
			FDT_FLASH_PATH);
		(void)close(fd);
		return FWTS_ERROR;
	}
	(void)close(fd);
	fdt_node_path[PATH_MAX] = '\0';
	fwts_log_info(fw, "MTD Info validated FDT of '%s'.",
			fdt_node_path);

	count = scandir(SYSFS_MTD_PATH, &namelist, NULL, alphasort);
	if (count < 0) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"OPAL MTD Info",
			"Scan for MTD '%s' unable to find any "
			"candidates. Check the installation "
			"for the MTD device config.",
			SYSFS_MTD_PATH);
		return FWTS_ERROR;
	}

	bytes = 0;

	fwts_log_nl(fw);
	fwts_log_info(fw, "STARTING checks of MTD devices");
	fwts_log_nl(fw);

	for (i = 0; i < count; i++) {
		struct dirent *dirent;
		char *sys_device_path; /* /sys/class/device/mtdx */
		char *mtd_device_path; /* /dev/mtdx */
		char *driver_path;
		char fdt_node_path_tmp[PATH_MAX];
		char mtd_driver_path[PATH_MAX];

		memset(fdt_node_path_tmp, 0, sizeof(fdt_node_path_tmp));
		memset(mtd_driver_path, 0, sizeof(mtd_driver_path));

		dirent = namelist[i];

		if (dirent->d_name[0] == '.' || bytes ||
			asprintf(&sys_device_path,
				"%s/%s/device/of_node",
				SYSFS_MTD_PATH, dirent->d_name) < 0) {
			/* asprintf must be last condition so when it */
			/* evaluates sys_device_path gets allocated */
			free(namelist[i]);
			continue;
		}

		bytes = readlink(sys_device_path, fdt_node_path_tmp,
			sizeof(fdt_node_path_tmp) - 1);
		free(sys_device_path);
		if (bytes < 0) {
			/* if mtd system flash does not have an FDT node */
			/* just continue */
			free(namelist[i]);
			/* reset the bytes to continue */
			bytes = 0;
			continue;
		}
		fdt_node_path_tmp[bytes] = '\0';

		if (strstr(fdt_node_path_tmp, fdt_node_path)) {
			bytes = asprintf(&mtd_device_path, "/dev/%s",
				dirent->d_name);
			if (bytes < 0) {
				free(namelist[i]);
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"OPAL MTD Info",
					"Failed to get the device path."
					" Check the installation for the"
					" path '/dev/%s'.",
					dirent->d_name);
				continue;
			}
			mtd_device_path[bytes] = '\0';
			bytes = 0;
			if (asprintf(&driver_path,
				"%s/%s/device/driver",
				SYSFS_MTD_PATH, dirent->d_name) < 0) {
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"OPAL MTD Info",
					"Failed to get the MTD Path."
					" Check the installation "
					"for the path '%s/%s/device/driver'.",
					SYSFS_MTD_PATH,
					dirent->d_name);
				free(mtd_device_path);
				continue;
			}
			bytes = readlink(driver_path, mtd_driver_path,
				sizeof(mtd_driver_path) -1);
			if (bytes < 0) {
				fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"OPAL MTD Info",
					"Failed to get the MTD drive path."
					" Check the installation for the "
					"path %s.",
					driver_path);
				free(mtd_device_path);
				free(driver_path);
				continue;
			} else {
				mtd_driver_path[bytes] = '\0';
				bytes = 0;
				free(driver_path);
			}

			if (strstr(mtd_driver_path, "powernv_flash")) {
				if (!strstr(mtd_device_path, "ro")) {
					if (mtd_dev_query(fw,
						mtd_device_path)) {
						/* failures logged */
						/* in subroutine   */
						free(mtd_device_path);
						continue;
					}
					if (mtd_hdr_query(fw,
						mtd_device_path)) {
						/* failures logged */
						/* in subroutine   */
						free(mtd_device_path);
						continue;
					}
					fwts_log_nl(fw);
				}
			}
			free(mtd_device_path);
		}
		free(namelist[i]);
	}
	free(namelist);

	fwts_log_info(fw, "ENDING checks of MTD devices");
	fwts_log_nl(fw);

	fwts_passed(fw, "OPAL MTD info passed.");

	return FWTS_OK;
}

static int mtd_info_init(fwts_framework *fw)
{
	if (fw->fdt) {
#ifdef HAVE_LIBFDT
		int node;
		/* perform some FDT validation */
		node = fdt_path_offset(fw->fdt,
			"/ibm,opal/nvram");
		if (node >= 0) {
			if (!fdt_node_check_compatible(fw->fdt, node,
				"ibm,opal-nvram")) {
				fwts_log_info(fw,
					"MTD Info initialization validated"
					" FDT for 'ibm,opal-nvram'.");
				return FWTS_OK;
			} else {
				return FWTS_SKIP;
			}
		}
#endif
	} else {
		fwts_log_info(fw, "The OPAL MTD device tree node is not"
			" able to be detected so skipping the mtd_info"
			" test.  There may be tools missing such as"
			" libfdt-dev or dtc, check that the packages"
			" are installed and re-build if needed."
			" If this condition persists try running the"
			" dt_base test to further diagnose. If dt_base"
			" test is not available this is probably a"
			" setup problem.");
		return FWTS_SKIP;
	}

	/* only run test when fdt node is confirmed */
	return FWTS_SKIP;
}

static fwts_framework_minor_test mtd_info_tests[] = {
	{ mtd_info_test1, "OPAL MTD Info" },
	{ NULL, NULL }
};

static fwts_framework_ops mtd_info_ops = {
	.description = "OPAL MTD Info",
	.init        = mtd_info_init,
	.minor_tests = mtd_info_tests
};

FWTS_REGISTER_FEATURES("mtd_info", &mtd_info_ops, FWTS_TEST_ANYTIME,
		FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV,
		FWTS_FW_FEATURE_DEVICETREE);
