/*
 * Copyright (C) 2014-2026 Canonical
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
#include <string.h>

#include "fwts.h"

/*
 *  fwts_release_field_get()
 *	attempt to match a field (needle) in the given text and update field
 *	with any text after the delimiter, e.g. if text is "DISTRIB_ID=Ubuntu"
 *	and needle is "DISTRIB_ID" and delimiter is "=", then the field gets set to "Ubuntu".
 */
static void fwts_release_field_get(char *needle, char *delimiter, char *text, char **field)
{
	char *str = strstr(text, delimiter);

	if (str == NULL)
		return;
	if (strstr(text, needle) == NULL)
		return;
	str++;
	while (*str == ' ' || *str == '\t')
		str++;

	if (*str)
		*field = strdup(str);
}

/*
 *  fwts_release_get_debian()
 *	return release for debian based systems
 */
static void fwts_release_get_debian(fwts_list *list, fwts_release *release)
{
	fwts_list_link *item;

	fwts_list_foreach(item, list) {
		char *line = fwts_text_list_text(item);

		fwts_release_field_get("Distributor ID:", ":", line, &release->distributor);
		fwts_release_field_get("Release", ":", line, &release->description);
		fwts_release_field_get("Codename", ":", line, &release->release);
		fwts_release_field_get("Description", ":", line, &release->codename);
	}
}

/*
 *  fwts_release_field_null
 *	convert NULL fields to ""
 */
static void fwts_release_field_null_to_str(char **field)
{
	if (*field == NULL)
		*field = strdup("");
}

/*
 *  fwts_release_get()
 *	get release info
 */
fwts_release *fwts_release_get(void)
{
	fwts_list *list;
	fwts_release *release;
	int status;

	if ((release = calloc(1, sizeof(fwts_release))) == NULL)
		return NULL;

	/*
	 *  For the moment, check in /etc/lsb-release, we need to add in
	 *  support for different distros too.
	 */
	if (fwts_pipe_exec("lsb_release -a", &list, &status) != FWTS_OK) {
		free(release);
		return NULL;
	}
	if (list) {
		fwts_release_get_debian(list, release);
		fwts_list_free(list, free);
	}

	/* null fields to "" */
	fwts_release_field_null_to_str(&release->distributor);
	fwts_release_field_null_to_str(&release->description);
	fwts_release_field_null_to_str(&release->release);
	fwts_release_field_null_to_str(&release->codename);

	if ((release->distributor == NULL) ||
	    (release->description == NULL) ||
	    (release->release == NULL) ||
	    (release->codename == NULL)) {
		fwts_release_free(release);
		return NULL;
	}
	return release;
}

/*
 *  fwts_release_free()
 *	free release info
 */
void fwts_release_free(fwts_release *release)
{
	if (release) {
		free(release->distributor);
		free(release->description);
		free(release->release);
		free(release->codename);
		free(release);
	}
}
