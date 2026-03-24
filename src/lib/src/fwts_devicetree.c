/*
 * Copyright (C) 2016-2021 IBM Corporation
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

#include <stdio.h>
#include <ctype.h>

#include "fwts.h"

#include <libfdt.h>

proc_gen_t proc_gen;

int fwts_devicetree_read(fwts_framework *fwts)
{
	char *command, *data = NULL;
	int fd, rc, status;
	ssize_t len;
	pid_t pid;

	if (!fwts_firmware_has_features(FWTS_FW_FEATURE_DEVICETREE))
		return FWTS_OK;

	rc = asprintf(&command, "dtc -I fs -O dtb %s", DT_FS_PATH);
	if (rc < 0)
		return FWTS_ERROR;

	rc = fwts_pipe_open_ro(command, &pid, &fd);
	if (rc < 0) {
		free(command);
		return FWTS_ERROR;
	}
	free(command);

	rc = fwts_pipe_read(fd, &data, &len);
	if (rc) {
		fwts_pipe_close(fd, pid);
		return FWTS_ERROR;
	}

	status = fwts_pipe_close(fd, pid);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || len == 0) {
		fprintf(stderr, "Cannot read devicetree data: dtc failed\n");
		free(data);
		return FWTS_ERROR;
	}

	fwts->fdt = data;

	return FWTS_OK;
}

bool check_status_property_okay(fwts_framework *fw,
	const char *my_path,
	const char *my_prop_string,
	const char *property)
{
	const char *prop_string = strstr(my_path, my_prop_string);

	if (prop_string) {
		int prop_len = 0;
		int node = fdt_path_offset(fw->fdt, prop_string);

		if (node >= 0) {
			const char *prop_buf;

			prop_buf = fdt_getprop(fw->fdt, node,
					property,
					&prop_len);
			if (prop_buf && (prop_len > 0)) {
				if ((!strcmp(prop_buf, "okay")) ||
					(!strcmp(prop_buf, "ok"))) {
					return true;
				}
			}
		}
	}
	return false;
}

int check_property_printable(fwts_framework *fw,
	const char *name,
	const char *buf,
	const size_t len)
{
	bool printable = true;
	unsigned int i;

	/* we need at least one character plus a nul */
	if (len < 2) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"DTPrintablePropertyShort",
			"property \"%s\" is too short", name);
		return FWTS_ERROR;
	}

	/* check all characters are printable */
	for (i = 0; i < len - 1; i++) {
		printable = printable && isprint(buf[i]);
		if (!printable)
			break;
	}

	if (!printable) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"DTPrintablePropertyInvalid",
			"property \"%s\" contains unprintable characters",
			name);
		return FWTS_ERROR;
	}

	/* check for a trailing nul */
	if (buf[len - 1] != '\0') {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"DTPrintablePropertyNoNul",
			"property \"%s\" isn't nul-terminated", name);
		return FWTS_ERROR;
	}

	fwts_log_info_verbatim(fw,
		"DTPrintableProperty \"%s\" with a string"
		" value of \"%s\" passed",
		name,
		buf);

	fwts_passed(fw,
		"DTPrintableProperty \"%s\" passed",
		name);

	return FWTS_OK;
}

/* hidewhitespace (char *name)                    */
/*                                                */
/* Caller must pass in modifiable memory for name */
/* Caller must save original memory ptr to free   */
/* the original allocated memory if needed        */

char *hidewhitespace(char *name)
{
	char *end;

	while (isspace(*name))
		name++;

	if (!*name)
		return name;

	end = name + strlen(name) - 1;
	while (end > name && isspace(*end))
		end--;

	*(end + 1) = '\0';

	return name;

}

/*
 * fwts_dt_property_read_u32 This function reads one u32 DT property
 * 	Returns FWTS_OK on success:	*value will contain one int
 * 	FWTS_ERROR on error:		*value will contain error code
 */

int fwts_dt_property_read_u32(
	void *fdt,
	const int offset,
	const char *pname,
	int *value)
{
	int len = 0;
	const int *buf;

	buf = fdt_getprop(fdt, offset, pname, &len);
	if (buf == NULL) {
		*value = len;
		return FWTS_ERROR;
	}
	*value = be32toh(*buf);
	return FWTS_OK;
}

/*
 * This function reads DT property array of u32's
 * 	Return FWTS_OK on success:	*value contain full array of int's
 *  	FWTS_ERROR on error:		*value will contain error code which
 *  						comes from *len
 */

int fwts_dt_property_read_u32_arr(
	void *fdt,
	const int offset,
	const char *pname,
	int *value,
	int *len)
{
	int i;
	const int *buf;

	buf = fdt_getprop(fdt, offset, pname, len);
	if (buf == NULL) {
		*value = *len;
		return FWTS_ERROR;
	}

	*len = *len / sizeof(int);
	for (i = 0; i < *len; i++)
		value[i] = be32toh(buf[i]);
	return FWTS_OK;
}

/*
 * This function reads DT property array of u64's
 * 	Return FWTS_OK on success:	*value contain full array of u64's
 * 	FWTS_ERROR on error:		*value will contain error code which
 * 						comes from *len
 */

int fwts_dt_property_read_u64_arr(
	void *fdt,
	const int offset,
	const char *pname,
	uint64_t *value,
	int *len)
{
	int i;
	const int *buf;

	buf = fdt_getprop(fdt, offset, pname, len);
	if (buf == NULL) {
		*value = *len;
		return FWTS_ERROR;
	}

	*len = *len / sizeof(uint64_t);
	for (i = 0; i < *len; i++)
		value[i] = be64toh(buf[i]);
	return FWTS_OK;
}

/* Gets the length of DT property string list */

int fwts_dt_stringlist_count(
	fwts_framework *fw,
	const void *fdt,
	const int nodeoffset,
	const char *property)
{
	const char *list, *end;
	int length = 0, count = 0;

	list = fdt_getprop(fdt, nodeoffset, property, &length);
	if (!list) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PropertyNotFound",
			"Failed to get property %s rc %d", property, length);
		return FWTS_ERROR;
	}

	end = list + length;

	while (list < end) {
		length = strnlen(list, end - list) + 1;

		/* Check if the last string isn't properly NUL-terminated. */
		if (list + length > end) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "NotNULLTerminated",
				"Last string is not properly NULL terminated");
			return FWTS_ERROR;
		}

		list += length;
		count++;
	}

	return count;
}

static int get_cpu_version(fwts_framework *fw, int *value)
{
	const char *cpus_path = "/cpus/";
	int offset;
	int cpu_version; int ret;

	if (!fw->fdt) {
		fwts_skipped(fw, "Device tree not found");
		return FWTS_SKIP;
	}

	offset = fdt_path_offset(fw->fdt, cpus_path);
	if (offset < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
			"/cpus node is missing");
		return FWTS_ERROR;
	}

	offset = fdt_node_offset_by_prop_value(fw->fdt, -1, "device_type",
				"cpu", sizeof("cpu"));
	if (offset < 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTNodeMissing",
			"cpu node is missing");
		return FWTS_ERROR;
	}

	ret = fwts_dt_property_read_u32(fw->fdt, offset,
			"cpu-version", &cpu_version);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "DTPropertyReadError",
			"Failed to read property cpu-version %s",
			fdt_strerror(cpu_version));
		return FWTS_ERROR;
	}
	*value = cpu_version;
	return FWTS_OK;
}

int get_proc_gen(fwts_framework *fw)
{
	int version; int ret;
	const int mask = 0xFFFF0000;
	int pvr;

	ret = get_cpu_version(fw, &version);
	if (ret != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "DTNoCPUVersion",
			"Not able to get the CPU version");
		return FWTS_ERROR;
	}

	pvr = (mask & version) >> 16;
	switch (pvr) {
	/* Get CPU family and other flags based on PVR */
	case PVR_TYPE_P7:
	case PVR_TYPE_P7P:
		proc_gen = proc_gen_p7;
		break;
	case PVR_TYPE_P8E:
	case PVR_TYPE_P8:
		proc_gen = proc_gen_p8;
		break;
	case PVR_TYPE_P8NVL:
		proc_gen = proc_gen_p8;
		break;
	case PVR_TYPE_P9:
		proc_gen = proc_gen_p9;
		break;
	default:
		proc_gen = proc_gen_unknown;
	}

	return FWTS_OK;
}
