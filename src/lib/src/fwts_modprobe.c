/*
 * Copyright (C) 2020-2026 Canonical
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
#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/utsname.h>

#include "fwts.h"

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

/*
 *  fwts_module_path()
 *	build a new path based on basename path and
 *	the new filename (or directory name)
 */
static void fwts_module_path(
	char *newpath,
	const size_t newpath_len,
	const char *basepath,
	const char *filename)
{
	char *ptr = newpath;
	size_t n = newpath_len, len;

	(void)strlcpy(ptr, basepath, n);
	len = strlen(basepath);
	ptr += len;
	n -= len;

	(void)strlcpy(ptr, "/", n);
	ptr++;
	n--;

	(void)strlcpy(ptr, filename, n);
}

/*
 *  fwts_module_find()
 *	recursively search for module from the basepath start
 *	and return false if not found, and true if found. If
 *	found, the full path of the module is set in the string
 *	path.
 */
static bool fwts_module_find(
	const char *module,
	const char *basepath,
	char *path,
	const size_t path_len)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(basepath);
	if (!dir)
		return false;

	while ((de = readdir(dir)) != NULL) {
		char newpath[PATH_MAX];

		if (de->d_name[0] == '.')
			continue;

		switch (de->d_type) {
		case DT_DIR:
			fwts_module_path(newpath, sizeof(newpath), basepath, de->d_name);
			if (fwts_module_find(module, newpath, path, path_len)) {
				(void)closedir(dir);
				return true;
			}
			break;
		case DT_REG:
			if (!strcmp(de->d_name, module)) {
				fwts_module_path(path, path_len, basepath, module);
				(void)closedir(dir);
				return true;
			}
			break;
		default:
			break;
		}
	}

	(void)closedir(dir);
	return false;
}

/*
 *  sys_finit_module()
 *	system call wrapper for finit_module
 */
static inline int sys_finit_module(
	int fd,
	const char *param_values,
	int flags)
{
	errno = 0;
	return syscall(__NR_finit_module, fd, param_values, flags);
}

/*
 *  sys_delete_module()
 *	system call wrapper for delete_module
 */
static inline int sys_delete_module(
	const char *name,
	int flags)
{
	errno = 0;
	return syscall(__NR_delete_module, name, flags);
}

/*
 *  fwts_module_loaded()
 *	check if module is loaded, the name (without .ko suffix) is
 *	provided in string module. Boolean loaded is set to true if
 *	the module is loaded, false otherwise. Returns FWTS_OK if
 *	all went OK, FWTS_ERROR if something went wrong.
 */
int fwts_module_loaded(fwts_framework *fw, const char *module, bool *loaded)
{
	FILE *fp;
	char buffer[1024];

	*loaded = false;
	fp = fopen("/proc/modules", "r");
	if (!fp) {
		fwts_log_error(fw, "Cannot open /proc/modules, errno=%d (%s)\n",
			errno, strerror(errno));
		return FWTS_ERROR;
	}

	(void)memset(buffer, 0, sizeof(buffer));
	while (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
		char *ptr = strchr(buffer, ' ');

		if (ptr)
			*ptr = '\0';

		if (!strcmp(buffer, module)) {
			*loaded = true;
			break;
		}
	}
	(void)fclose(fp);

	return FWTS_OK;
}

/*
 *  fwts_module_load()
 *	load a module. The module name (without the .ko) suffix
 *	is to provided in string module.  Returns FWTS_OK if
 *	succeeded (or the module is already loaded) and FWTS_ERROR
 *	if the load failed.
 */
int fwts_module_load(fwts_framework *fw, const char *module)
{
	struct utsname u;
	const size_t modlen = strlen(module);
	char module_ko[modlen + 4];
	char module_ko_zst[modlen + 8];
	char path[PATH_MAX];
	char modpath[PATH_MAX];
	bool loaded = false;
	int status = 0;
	char modeprobe_ko[64];
	/*
	 *  No need to unload if it's not already loaded
	 */
	if (fwts_module_loaded(fw, module, &loaded) == FWTS_OK) {
		if (loaded)
			return FWTS_OK;
	}

	/*
	 *  Set up module root path and try to find the named module
	 */
	if (uname(&u) < 0) {
		fwts_log_error(fw, "Call to uname failed, errno=%d (%s)\n",
			errno, strerror(errno));
		return FWTS_ERROR;
	}
	(void)snprintf(module_ko, sizeof(module_ko), "%s.ko", module);
	(void)snprintf(modpath, sizeof(modpath), "/lib/modules/%s", u.release);
	if (!fwts_module_find(module_ko, modpath, path, sizeof(path))) {
		(void)snprintf(module_ko_zst, sizeof(module_ko_zst), "%s.ko.zst", module);
		if (!fwts_module_find(module_ko_zst, modpath, path, sizeof(path))) {
			fwts_log_error(fw, "Cannot find module %s\n", module);
			return FWTS_ERROR;
		}
	}

	(void)snprintf(modeprobe_ko, sizeof("modprobe ") + modlen, "modprobe %s", module);
	(void)fwts_exec(modeprobe_ko, &status);
	if (status != FWTS_OK) {
		fwts_log_error(fw, "modprobe module %s failed\n", module);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  fwts_module_unload()
 *	unload a module. The module name (without the .ko) suffix
 *	is to provided in string module.  Returns FWTS_OK if
 *	succeeded (or the module is not previously loaded) and
 *	FWTS_ERROR if the unload failed.
 */
int fwts_module_unload(fwts_framework *fw, const char *module)
{
	bool loaded = false;
	char modeprobe_ko[64];
	const size_t modlen = strlen(module);
	int status = 0;

	/*
	 *  No need to unload if it's not already loaded
	 */
	if (fwts_module_loaded(fw, module, &loaded) == FWTS_OK) {
		if (!loaded)
			return FWTS_OK;
	}

	(void)snprintf(modeprobe_ko, sizeof("modprobe -r") + modlen, "modprobe -r %s", module);
	(void)fwts_exec(modeprobe_ko, &status);
	if (status != FWTS_OK) {
		fwts_log_error(fw, "Cannot unload module %s\n", module);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}
