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

#include "brightness-helper.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

/*
 *  Brightness tests helper functions
 */

static DIR *brightness_dir;
static const char *brightness_path;

/*
 *  brightness_get_dir()
 * 	return /sys interface DIR handle opened by brightness_init()
 */
DIR *brightness_get_dir(void)
{
	return brightness_dir;
}

/*
 *  brightness_get_path()
 *
 */
const char *brightness_get_path(void)
{
	return brightness_path;
}

/*
 *  brightness_init()
 *	generic brightness test init, if successful
 *	it opens a directory for the /sys interface
 */
int brightness_init(fwts_framework *fw)
{
	int i;
	static const char *sys_path[] = {
		"/sys/class/backlight",
		"/sys/devices/virtual/backlight",
		NULL
	};

	brightness_path = NULL;
	brightness_dir = NULL;

	for (i = 0; sys_path[i]; i++) {
		brightness_dir = opendir(sys_path[i]);
		if (brightness_dir) {
			brightness_path = sys_path[i];
			return FWTS_OK;
		}
	}

	fwts_failed(fw, LOG_LEVEL_LOW,
		"BacklightNoPath",
		"No sysfs backlight directory available: cannot test.");

	return FWTS_ERROR;
}

/*
 *  brightness_deinit()
 */
int brightness_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	if (brightness_dir)
		(void)closedir(brightness_dir);

	brightness_path = NULL;
	brightness_dir = NULL;

	return FWTS_OK;
}

/*
 *  brightness_get_setting()
 *	get a brightness setting
 */
int brightness_get_setting(const char *entry_name, const char *setting, int *value)
{
	char path[PATH_MAX];
	FILE *fp;

	snprintf(path, sizeof(path), "%s/%s/%s", brightness_path, entry_name, setting);
	if ((fp = fopen(path, "r")) == NULL)
		return FWTS_ERROR;

	if (fscanf(fp, "%d", value) != 1) {
		(void)fclose(fp);
		return FWTS_ERROR;
	}

	(void)fclose(fp);

	return FWTS_OK;
}

/*
 *  brightness_set_setting()
 *	set a brightness setting
 */
int brightness_set_setting(const char *entry_name, const char *setting, const int value)
{
	char path[PATH_MAX];
	FILE *fp;

	snprintf(path, sizeof(path), "%s/%s/%s", brightness_path, entry_name, setting);
	if ((fp = fopen(path, "w")) == NULL)
		return FWTS_ERROR;

	if (fprintf(fp, "%d", value) < 1) {
		(void)fclose(fp);
		return FWTS_ERROR;
	}

	(void)fclose(fp);

	return FWTS_OK;
}
