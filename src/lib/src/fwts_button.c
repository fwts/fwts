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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

#define FWTS_PROC_ACPI_BUTTON	"/proc/acpi/button"

static inline void fwts_button_path(
	char *path,
	const size_t path_len,
	const char *button_dir,
	const char *name,
	const char *str)
{
	(void)strlcpy(path, button_dir, path_len);
	(void)strlcat(path, "/", path_len);
	(void)strlcat(path, name, path_len);
	(void)strlcat(path, "/", path_len);
	(void)strlcat(path, str, path_len);
}

/*
 *  fwts_button_match_state_proc()
 *	find matching button state and keep count of matching
 *	any non-matching states found, via proc interface
 */
static int fwts_button_match_state_proc(
	const uint32_t button,
	uint32_t *matched,
	uint32_t *not_matched)
{
	DIR *dir;
	struct dirent *entry;
	char *acpi_button_lid   = FWTS_PROC_ACPI_BUTTON "/lid";
	char *acpi_button_power = FWTS_PROC_ACPI_BUTTON "/power";
	char *button_dir;
	char *field;
	char *match;

	switch (button) {
	case FWTS_BUTTON_LID_ANY:
		button_dir = acpi_button_lid;
		field  = "state";
		match  = "";
		break;
	case FWTS_BUTTON_LID_OPENED:
		button_dir = acpi_button_lid;
		field  = "state";
		match  = "open";
		break;
	case FWTS_BUTTON_LID_CLOSED:
		button_dir = acpi_button_lid;
		field  = "state";
		match  = "close";
		break;
	case FWTS_BUTTON_POWER_EXISTS:
		button_dir = acpi_button_power;
		field  = "info";
		match  = "Power Button";
		break;
	default:
		return FWTS_ERROR;
	}

	if ((dir = opendir(button_dir)) == NULL)
		return FWTS_ERROR;
	do {
		entry = readdir(dir);
		if (entry && strlen(entry->d_name) > 2) {
			char path[PATH_MAX];
			char *data;

			fwts_button_path(path, sizeof(path), button_dir, entry->d_name, field);

			if ((data = fwts_get(path)) != NULL) {
				if (strstr(data, match))
					(*matched)++;
				else
					(*not_matched)++;
				free(data);
			}
		}
	} while (entry);

	(void)closedir(dir);

	return FWTS_OK;
}

/*
 *  fwts_button_match_state()
 *	find matching button state and keep count of matching
 *	any non-matching states found
 *
 *  The button state can be:
 *	FWTS_BUTTON_LID_ANY      - match any LID button state
 * 	FWTS_BUTTON_LID_OPENED   - match any LID buttons that are open
 * 	FWTS_BUTTON_LID_CLOSED   - match any LID buttons that are closed
 *	FWTS_BUTTON_POWER_EXISTS - match any power putton states
 *
 * 	matched is a count of any button states that match, and not_matched
 *	is a count of any button states that don't match.
 */
int fwts_button_match_state(
	fwts_framework *fw,
	const uint32_t button,
	uint32_t *matched,
	uint32_t *not_matched)
{
	*matched = 0;
	*not_matched = 0;

	FWTS_UNUSED(fw);

	if (access(FWTS_PROC_ACPI_BUTTON, R_OK) == 0)
		return fwts_button_match_state_proc(button, matched, not_matched);

	return FWTS_ERROR;
}
