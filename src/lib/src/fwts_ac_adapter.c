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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "fwts.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#define FWTS_AC_ADAPTER_ANY	(0x0)
#define FWTS_AC_ADAPTER_ONLINE	(0x1)
#define FWTS_AC_ADAPTER_OFFLINE	(0x2)

#define SYS_INTERFACE 		(0x0)
#define PROC_INTERFACE		(0x1)

typedef struct {
	const char *path;	/* Path name of interface */
	const char *state;	/* Name of online/offline status */
	const char *offline;	/* Contents of state when offline */
	const char *online;	/* Contents of state when online */
	const char *type;	/* /sys/class type to indicate Mains power, NULL if not used */
} fwts_ac_interface_info;

static const fwts_ac_interface_info fwts_ac_interfaces[] = {
	{
		FWTS_SYS_CLASS_POWER_SUPPLY,
		"online",
		"0",
		"1",
		"Mains"
	},
	{
		FWTS_PROC_ACPI_AC_ADAPTER,
		"state",
		"off-line",
		"on-line",
		NULL
	}
};

/*
 *  fwts_ac_adapter_get_state()
 *	fetch count of matching and non-matching instances of
 *	AC adapter state, state can be:
 *	  FWTS_AC_ADAPTER_ONLINE  - connected to AC adapter
 *	  FWTS_AC_ADAPTER_OFFLINE - not connected to AC adapter
 *	  FWTS_AC_ADAPTER_ANY     - either of above
 * 	matching is incremented for each AC adapter instance that matches
 *	not_matching is incremented for each AC adapter instance that does
 *	not match.
 */
int fwts_ac_adapter_get_state(const int state, int *matching, int *not_matching)
{
	DIR *ac_power_dir;
	struct dirent *entry;
	fwts_ac_interface_info const *ac_interface;

	/* Try to user newer /sys interface first */
	if ((ac_power_dir = opendir(FWTS_SYS_CLASS_POWER_SUPPLY))) {
		int count = 0;

		while (readdir(ac_power_dir) != NULL)
			count++;

		/* no power supply dir exists */
		if (count == 2) {
			(void)closedir(ac_power_dir);
			return FWTS_ERROR;
		}
		rewinddir(ac_power_dir);

		ac_interface = &fwts_ac_interfaces[SYS_INTERFACE];
	/* then try older /proc interface  */
	} else if ((ac_power_dir = opendir(FWTS_PROC_ACPI_AC_ADAPTER))) {
		ac_interface = &fwts_ac_interfaces[PROC_INTERFACE];
	} else {
		return FWTS_ERROR;
	}

	do {
		entry = readdir(ac_power_dir);
		if (entry && entry->d_name[0] != '.') {
			char path[PATH_MAX];
			char *data;

			/* Check that type field matches the expected type */
			if (ac_interface->type != NULL) {
				snprintf(path, sizeof(path), "%s/%s/type", ac_interface->path, entry->d_name);
				if ((data = fwts_get(path)) != NULL) {
					bool mismatch = (strstr(ac_interface->type, data) != NULL);
					free(data);
					if (mismatch)
						continue;	/* type don't match, skip this entry */
				} else
					continue;		/* can't check type, skip this entry */
			}

			snprintf(path, sizeof(path), "%s/%s/%s", ac_interface->path, entry->d_name, ac_interface->state);
			if ((data = fwts_get(path)) != NULL) {
				const char *state_text = "";

				switch (state) {
				case FWTS_AC_ADAPTER_ANY:
					(*matching)++;
					free(data);
					continue;
				case FWTS_AC_ADAPTER_ONLINE:
					state_text = ac_interface->online;
					break;
				case FWTS_AC_ADAPTER_OFFLINE:
					state_text = ac_interface->offline;
					break;
				}
				if (strstr(data, state_text) != NULL)
					(*matching)++;
				else
					(*not_matching)++;
			}
			free(data);
		}
	} while (entry);

	(void)closedir(ac_power_dir);

	return FWTS_OK;
}
