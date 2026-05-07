/*
 * Copyright (C) 2021-2026 Canonical
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

#include <sys/utsname.h>

#include "fwts.h"
#include "fwts_kernel.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

#define CONFIG_FILE_PREFIX	"/boot/config-"
#define CONFIG_FILE_PROC	"/proc/config.gz"

/*
 *  fwts_kernel_config_plain_set
 *	 check whether a plain-text kernel config
 */
bool fwts_kernel_config_plain_set(const char *config)
{
	const size_t config_str_len = strlen(config) + 3;
	char config_file[PATH_MAX];
	char config_str[255];
	size_t config_file_len;
	fwts_list* config_list;
	fwts_list_link *item;
	struct utsname buf;

	/* get path of config file, i.e. /boot/config-5.11.0-38-generic */
	uname(&buf);
	config_file_len = strlen(CONFIG_FILE_PREFIX) + strlen(buf.release) + 1;
	(void)strlcpy(config_file, CONFIG_FILE_PREFIX, config_file_len);
	(void)strlcat(config_file, buf.release, config_file_len);

	config_list = fwts_file_open_and_read(config_file);
	if (config_list == NULL)
		return false;

	fwts_list_foreach(item, config_list) {
		/* check built-in, i.e. =y */
		(void)strlcpy(config_str, config, config_str_len);
		(void)strlcat(config_str, "=y", config_str_len);
		if (!strncmp(fwts_text_list_text(item), config_str, strlen(config_str))) {
			fwts_list_free(config_list, free);
			return true;
		}

		/* check module, i.e. =m */
		config_str[strlen(config_str) - 1] = 'm';
		if (!strncmp(fwts_text_list_text(item), config_str, strlen(config_str))) {
			fwts_list_free(config_list, free);
			return true;
		}
	}

	fwts_list_free(config_list, free);
	return false;
}

/*
 *  fwts_kernel_config_gz_set
 *	 check whether a gz kernel config
 */
bool fwts_kernel_config_gz_set(const char *config)
{
	const size_t config_str_len = strlen(config) + 3;
	char config_str[255];
	fwts_list* config_list;
	fwts_list_link *item;

	config_list = fwts_gzfile_open_and_read(CONFIG_FILE_PROC);
	if (config_list == NULL)
		return false;

	fwts_list_foreach(item, config_list) {
		/* check built-in, i.e. =y */
		(void)strlcpy(config_str, config, config_str_len);
		(void)strlcat(config_str, "=y", config_str_len);
		if (!strncmp(fwts_text_list_text(item), config_str, strlen(config_str))) {
			fwts_list_free(config_list, free);
			return true;
		}

		/* check module, i.e. =m */
		config_str[strlen(config_str) - 1] = 'm';
		if (!strncmp(fwts_text_list_text(item), config_str, strlen(config_str))) {
			fwts_list_free(config_list, free);
			return true;
		}
	}

	fwts_list_free(config_list, free);
	return false;
}

/*
 *  fwts_kernel_config_set
 *	 check whether a kernel config is set, ex.
 *	 true if CONFIG_XYZ=y or CONFIG_XYZ=m
 */
bool fwts_kernel_config_set(const char *config)
{
	if (fwts_kernel_config_plain_set(config))
		return true;

	if (fwts_kernel_config_gz_set(config))
		return true;

	return false;
}

/*
 *  fwts_kernel_config_exist
 *	 check whether kernel config exist
 */
bool fwts_kernel_config_exist()
{
	char config_file[PATH_MAX];
	size_t config_file_len;
	struct utsname buf;

	uname(&buf);
	config_file_len = strlen(CONFIG_FILE_PREFIX) + strlen(buf.release) + 1;
	(void)strlcpy(config_file, CONFIG_FILE_PREFIX, config_file_len);
	(void)strlcat(config_file, buf.release, config_file_len);

	if ((access(config_file, F_OK) != 0) && (access(CONFIG_FILE_PROC, F_OK) != 0))
		return false;

	return true;
}
