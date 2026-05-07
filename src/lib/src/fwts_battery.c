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
#include "fwts.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <inttypes.h>

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

static inline bool fwts_battery_match(
	const uint32_t index,
	const uint32_t loop_index)
{
	return (((fwts_battery_type)index == FWTS_BATTERY_ALL) | (index == loop_index));
}

static inline void fwts_battery_path(
	char *path,
	const size_t path_len,
	const char *battery_dir,
	const char *name,
	const char *str)
{
	(void)strlcpy(path, battery_dir, path_len);
	(void)strlcat(path, "/", path_len);
	(void)strlcat(path, name, path_len);
	(void)strlcat(path, "/", path_len);
	(void)strlcat(path, str, path_len);
}

static int fwts_battery_get_capacity_sys_fs(fwts_framework *fw,
	DIR 	*dir,
	const fwts_battery_type type,
	const uint32_t index,
	uint32_t *capacity_mAh,	/* charge */
	uint32_t *capacity_mWh, /* energy */
	uint32_t *count)
{
	struct dirent *entry;
	char *field_mAh;
	char *field_mWh;
	size_t field_mAh_len;
	size_t field_mWh_len;
	uint32_t i = 0;

	switch (type) {
	case FWTS_BATTERY_DESIGN_CAPACITY:
		field_mAh = "POWER_SUPPLY_CHARGE_FULL_DESIGN=";
		field_mWh = "POWER_SUPPLY_ENERGY_FULL_DESIGN=";
		break;
	case FWTS_BATTERY_REMAINING_CAPACITY:
		field_mAh = "POWER_SUPPLY_CHARGE_NOW=";
		field_mWh = "POWER_SUPPLY_ENERGY_NOW=";
		break;
	default:
		return FWTS_ERROR;
	}

	field_mAh_len = strlen(field_mAh);
	field_mWh_len = strlen(field_mWh);

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			char *data;
			int  val;
			FILE *fp;
			bool match;

			/* Check that type field matches the expected type */
			fwts_battery_path(path, sizeof(path), FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name, "type");
			if ((data = fwts_get(path)) != NULL) {
				bool mismatch = (strstr(data, "Battery") == NULL);
				free(data);
				if (mismatch)
					continue;	/* type don't match, skip this entry */
			} else
				continue;		/* can't check type, skip this entry */

			match = fwts_battery_match(index, i);
			i++;
			if (!match)
				continue;

			snprintf(path, sizeof(path), "%s/%s/uevent", FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name);
			if ((fp = fopen(path, "r")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[4096];
				while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					if (strstr(buffer, field_mAh) &&
					    strlen(buffer) > field_mAh_len) {
						sscanf(buffer+field_mAh_len, "%d", &val);
						*capacity_mAh += (val / 1000);
						(*count)++;
					}
					if (strstr(buffer, field_mWh) &&
					    strlen(buffer) > field_mWh_len) {
						sscanf(buffer+field_mWh_len, "%d", &val);
						*capacity_mWh += (val / 1000);
						(*count)++;
					}
				}
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static int fwts_battery_get_capacity_proc_fs(fwts_framework *fw,
	DIR 	*dir,
	const fwts_battery_type type,
	const uint32_t index,
	uint32_t *capacity_mAh,
	uint32_t *capacity_mWh,
	uint32_t *count)
{
	struct dirent *entry;
	char *file;
	char *field;
	uint32_t i = 0;

	switch (type) {
	case FWTS_BATTERY_DESIGN_CAPACITY:
		file = "info";
		field = "design capacity";
		break;
	case FWTS_BATTERY_REMAINING_CAPACITY:
		file = "state";
		field = "remaining capacity";
		break;
	default:
		return FWTS_ERROR;
	}

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			int  val;
			FILE *fp;
			bool match = fwts_battery_match(index, i);

			i++;
			if (!match)
				continue;

			fwts_battery_path(path, sizeof(path), FWTS_PROC_ACPI_BATTERY, entry->d_name, file);

			if ((fp = fopen(path, "r")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[4096];
				while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					if (strstr(buffer, field) &&
					    strlen(buffer) > 25) {
						char units[64];

						sscanf(buffer+25, "%d %63s", &val, units);
						if (strncmp(units, "mAh",3) == 0) {
							*capacity_mAh += val;
							(*count)++;
						}
						if (strncmp(units, "mWh",3) == 0) {
							*capacity_mWh += val;
							(*count)++;
						}
						break;
					}
				}
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static int fwts_battery_get_count_sys_fs(DIR *dir, uint32_t *count)
{
	struct dirent *entry;
	char path[PATH_MAX];
	char *data;

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			/* Check that type field matches the expected type */
			fwts_battery_path(path, sizeof(path), FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name, "type");
			if ((data = fwts_get(path)) != NULL) {
				if (strstr(data, "Battery") != NULL)
					(*count)++;
				free(data);
			}
		}
	} while (entry);
	return FWTS_OK;
}

static int fwts_battery_get_count_proc_fs(DIR *dir, uint32_t *count)
{
	struct dirent *entry;
	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2)
			(*count)++;
	} while (entry);
	return FWTS_OK;
}

static int fwts_battery_get_name_sys_fs(
	DIR *dir,
	const uint32_t index,
	char *name,
	const size_t name_len)
{
	struct dirent *entry;
	char path[PATH_MAX];
	char *data;
	uint32_t i = 0;

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			bool match;
			/* Check that type field matches the expected type */
			fwts_battery_path(path, sizeof(path), FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name, "type");
			if ((data = fwts_get(path)) != NULL) {
				bool mismatch = (strstr(data, "Battery") == NULL);
				free(data);
				if (mismatch)
					continue;	/* type don't match, skip this entry */
			} else
				continue;		/* can't check type, skip this entry */

			match = fwts_battery_match(index, i);
			i++;
			if (!match)
				continue;

			strlcpy(name, entry->d_name, name_len);
			return FWTS_OK;
		}
	} while (entry);

	return FWTS_ERROR;
}

static int fwts_battery_get_name_proc_fs(
	DIR *dir,
	const uint32_t index,
	char *name,
	const size_t name_len)
{
	struct dirent *entry;
	uint32_t i = 0;

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			bool match = fwts_battery_match(index, i);
			i++;
			if (!match)
				continue;

			strlcpy(name, entry->d_name, name_len);
			return FWTS_OK;
		}
	} while (entry);

	return FWTS_ERROR;
}

static int fwts_battery_get_cycle_count_sys_fs(
	fwts_framework *fw,
	DIR *dir,
	const uint32_t index,
	uint32_t *cycle_count)
{
	struct dirent *entry;
	char *field_cycle_count;
	size_t field_cycle_count_len;
	uint32_t i = 0;

	*cycle_count = 0;
	field_cycle_count = "POWER_SUPPLY_CYCLE_COUNT=";
	field_cycle_count_len = strlen(field_cycle_count);

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			char *data;
			int  val;
			FILE *fp;
			bool match;

			/* Check that type field matches the expected type */
			fwts_battery_path(path, sizeof(path), FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name, "type");
			if ((data = fwts_get(path)) != NULL) {
				bool mismatch = (strstr(data, "Battery") == NULL);
				free(data);
				if (mismatch)
					continue;	/* type don't match, skip this entry */
			} else
				continue;		/* can't check type, skip this entry */
			match = fwts_battery_match(index, i);
			i++;
			if (!match)
				continue;

			snprintf(path, sizeof(path), "%s/%s/uevent", FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name);
			if ((fp = fopen(path, "r")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[4096];
				while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					if (strstr(buffer, field_cycle_count) &&
					    strlen(buffer) > field_cycle_count_len) {
						sscanf(buffer+field_cycle_count_len, "%d", &val);
						*cycle_count = val;
					}
				}
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static int fwts_battery_get_cycle_count_proc_fs(
	fwts_framework *fw,
	DIR *dir,
	const uint32_t index,
	uint32_t *cycle_count)
{
	struct dirent *entry;
	char *file;
	char *field;
	uint32_t i = 0;

	*cycle_count = 0;
	file = "info";
	field = "cycle count";

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			int  val;
			FILE *fp;
			bool match = fwts_battery_match(index, i);

			i++;
			if (!match)
				continue;

			fwts_battery_path(path, sizeof(path), FWTS_PROC_ACPI_BATTERY, entry->d_name, file);

			if ((fp = fopen(path, "r")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[4096];
				while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					if (strstr(buffer, field) &&
					    strlen(buffer) > 25) {
						sscanf(buffer+25, "%d", &val);
						*cycle_count = val;
						break;
					}
				}
				(void)fclose(fp);
			}
		}
	} while (entry);
	return FWTS_OK;
}

static int fwts_battery_set_trip_point_sys_fs(
	fwts_framework *fw,
	DIR *dir,
	const uint32_t index,
	const uint32_t trip_point)
{
	struct dirent *entry;
	uint32_t i = 0;

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			char *data;
			FILE *fp;
			bool match;

			/* Check that type field matches the expected type */
			fwts_battery_path(path, sizeof(path), FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name, "type");
			if ((data = fwts_get(path)) != NULL) {
				bool mismatch = (strstr(data, "Battery") == NULL);
				free(data);
				if (mismatch)
					continue;	/* type don't match, skip this entry */
			} else
				continue;		/* can't check type, skip this entry */
			match = fwts_battery_match(index, i);
			i++;
			if (!match)
				continue;

			snprintf(path, sizeof(path), "%s/%s/alarm", FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name);
			if ((fp = fopen(path, "rw+")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[512];
				sprintf(buffer, "%" PRIu32, trip_point * 1000);
				fputs(buffer, fp);
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static int fwts_battery_get_trip_point_sys_fs(
	fwts_framework *fw,
	DIR *dir,
	const uint32_t index,
	uint32_t *trip_point)
{
	struct dirent *entry;
	uint32_t i = 0;

	*trip_point = 0;
	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			char *data;
			int  val;
			FILE *fp;
			bool match;

			/* Check that type field matches the expected type */
			fwts_battery_path(path, sizeof(path), FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name, "type");
			if ((data = fwts_get(path)) != NULL) {
				bool mismatch = (strstr(data, "Battery") == NULL);
				free(data);
				if (mismatch)
					continue;	/* type don't match, skip this entry */
			} else
				continue;		/* can't check type, skip this entry */
			match = fwts_battery_match(index, i);
			i++;
			if (!match)
				continue;

			snprintf(path, sizeof(path), "%s/%s/alarm", FWTS_SYS_CLASS_POWER_SUPPLY, entry->d_name);
			if ((fp = fopen(path, "r")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[4096];
				while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					sscanf(buffer, "%d", &val);
					*trip_point = val / 1000;
				}
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static int fwts_battery_set_trip_point_proc_fs(
	fwts_framework *fw,
	DIR *dir,
	const uint32_t index,
	const uint32_t trip_point)
{
	struct dirent *entry;
	uint32_t i = 0;

	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			FILE *fp;
			bool match = fwts_battery_match(index, i);

			i++;
			if (!match)
				continue;

			fwts_battery_path(path, sizeof(path), FWTS_PROC_ACPI_BATTERY, entry->d_name, "alarm");
			if ((fp = fopen(path, "rw+")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[512];
				sprintf(buffer, "%" PRIu32 , trip_point);
				fputs(buffer, fp);
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

static int fwts_battery_get_trip_point_proc_fs(
	fwts_framework *fw,
	DIR *dir,
	const uint32_t index,
	uint32_t *trip_point)
{
	struct dirent *entry;
	uint32_t i = 0;

	*trip_point = 0;
	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			int  val;
			FILE *fp;
			bool match = fwts_battery_match(index, i);

			i++;
			if (!match)
				continue;

			fwts_battery_path(path, sizeof(path), FWTS_PROC_ACPI_BATTERY, entry->d_name, "alarm");
			if ((fp = fopen(path, "r")) == NULL) {
				fwts_log_info(fw, "Battery %s present but undersupported - no state present.", entry->d_name);
			} else {
				char buffer[4096];
				while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					if (strstr(buffer, "alarm:") &&
					    strlen(buffer) > 25) {
						sscanf(buffer + 25, "%" SCNu32, &val);
						*trip_point = val;
						break;
					}
				}
				(void)fclose(fp);
			}
		}
	} while (entry);

	return FWTS_OK;
}

int fwts_battery_set_trip_point(
	fwts_framework *fw,
	const uint32_t index,
	const uint32_t trip_point)
{
	int ret;
	DIR *dir;

	if ((dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY)) != NULL) {
		ret = fwts_battery_set_trip_point_sys_fs(fw, dir, index, trip_point);
		(void)closedir(dir);
	} else if ((dir = opendir(FWTS_PROC_ACPI_BATTERY)) != NULL) {
		ret = fwts_battery_set_trip_point_proc_fs(fw, dir, index, trip_point);
		(void)closedir(dir);
	} else {
		return FWTS_ERROR;
	}

	return ret;
}

int fwts_battery_get_trip_point(
	fwts_framework *fw,
	const uint32_t index,
	uint32_t *trip_point)
{
	int ret;
	DIR *dir;

	if ((dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY)) != NULL) {
		ret = fwts_battery_get_trip_point_sys_fs(fw, dir, index, trip_point);
		(void)closedir(dir);
	} else if ((dir = opendir(FWTS_PROC_ACPI_BATTERY)) != NULL) {
		ret = fwts_battery_get_trip_point_proc_fs(fw, dir, index, trip_point);
		(void)closedir(dir);
	} else {
		return FWTS_ERROR;
	}

	return ret;
}

bool fwts_battery_check_trip_point_support(
	fwts_framework *fw,
	const uint32_t index)
{
	uint32_t trip_point;

	if (!(fwts_battery_get_trip_point(fw, index, &trip_point) == FWTS_OK))
		return false;

	if (trip_point == 0)
		return false;

	return true;
}

int fwts_battery_get_cycle_count(
	fwts_framework *fw,
	const uint32_t index,
	uint32_t *cycle_count)
{
	int ret;
	DIR *dir;

	if ((dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY)) != NULL) {
		ret = fwts_battery_get_cycle_count_sys_fs(fw, dir, index, cycle_count);
		(void)closedir(dir);
	} else if ((dir = opendir(FWTS_PROC_ACPI_BATTERY)) != NULL) {
		ret = fwts_battery_get_cycle_count_proc_fs(fw, dir, index, cycle_count);
		(void)closedir(dir);
	} else {
		return FWTS_ERROR;
	}

	return ret;
}

int fwts_battery_get_name(
	fwts_framework *fw,
	const uint32_t index,
	char *name,
	const size_t name_len)
{
	int ret;
	DIR *dir;

	FWTS_UNUSED(fw);

	(void)memset(name, 0, name_len);

	if ((dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY)) != NULL) {
		ret = fwts_battery_get_name_sys_fs(dir, index, name, name_len);
		(void)closedir(dir);
	} else if ((dir = opendir(FWTS_PROC_ACPI_BATTERY)) != NULL) {
		ret = fwts_battery_get_name_proc_fs(dir, index, name, name_len);
		(void)closedir(dir);
	} else {
		return FWTS_ERROR;
	}
	return ret;
}

int fwts_battery_get_count(fwts_framework *fw, uint32_t *count)
{
	*count = 0;
	int ret;
	DIR *dir;

	FWTS_UNUSED(fw);

	if ((dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY)) != NULL) {
		ret = fwts_battery_get_count_sys_fs(dir, count);
		(void)closedir(dir);
	} else if ((dir = opendir(FWTS_PROC_ACPI_BATTERY)) != NULL) {
		ret = fwts_battery_get_count_proc_fs(dir, count);
		(void)closedir(dir);
	} else {
		return FWTS_ERROR;
	}
	return ret;
}

int fwts_battery_get_capacity(fwts_framework *fw,
	const fwts_battery_type type,
	const uint32_t index,
	uint32_t *capacity_mAh,
	uint32_t *capacity_mWh)
{
	int ret;
	DIR *dir;
	uint32_t n = 0;

	*capacity_mAh = 0;
	*capacity_mWh = 0;

	if ((dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY)) != NULL) {
		ret = fwts_battery_get_capacity_sys_fs(fw, dir, type, index, capacity_mAh, capacity_mWh, &n);
		(void)closedir(dir);
	} else if ((dir = opendir(FWTS_PROC_ACPI_BATTERY)) != NULL) {
		ret = fwts_battery_get_capacity_proc_fs(fw, dir, type, index, capacity_mAh, capacity_mWh, &n);
		(void)closedir(dir);
	} else {
		fwts_log_info(fw, "No battery information present: cannot test.");
		return FWTS_ERROR;
	}

	if ((ret == FWTS_ERROR) || (n == 0)) {
		fwts_log_info(fw, "No valid battery information present: cannot test.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}
