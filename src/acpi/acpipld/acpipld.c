/*
 * Copyright (C) 2024-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include "fwts_acpi_object_eval.h"

/*
 *  acpipld_init()
 *	initialize ACPI
 */
static int acpipld_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  acpipld_deinit
 *	de-intialize ACPI
 */
static int acpipld_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

static int acpipld_test1(fwts_framework *fw)
{
	fwts_list_link	*item;
	fwts_list *objects;
	const size_t name_len = 4;
	bool failed = false;

	if ((objects = fwts_acpi_object_get_names()) == NULL) {
		fwts_log_info(fw, "Cannot find any ACPI objects");
		return FWTS_ERROR;
	}

	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char *, item);
		const size_t len = strlen(name);

		if (!strncmp("_PLD", name + len - name_len, name_len)) {
			fwts_list_link	*item_upc;
			char *name_cmp;
			name_cmp = strdup(name);
			strcpy(name_cmp + len - name_len, "_UPC");

			fwts_list_foreach(item_upc, objects) {
				char *name_upc = fwts_list_data(char *, item_upc);
				if (!strncmp(name_cmp, name_upc, len)) {
					ACPI_OBJECT_LIST arg_list;
					ACPI_BUFFER buf;
					ACPI_OBJECT *obj;
					int ret;

					arg_list.Count   = 0;
					arg_list.Pointer = NULL;
					ret = fwts_acpi_object_evaluate(fw, name_upc, &arg_list, &buf);
					if ((ACPI_FAILURE(ret) != AE_OK) || (buf.Pointer == NULL))
						continue;

					obj = buf.Pointer;
					if (obj->Package.Elements[0].Integer.Value == 0) {
						fwts_warning(fw, "The ACPI method %s exists on the port "
							"claimed not connectable by the %s.",
							name, name_upc);
							failed = true;
					}

					free(buf.Pointer);
				}
			}
			free(name_cmp);
		}
	}

	if (!failed)
		fwts_passed(fw, "All _PLD methods exist only on the port connectable.");

	return FWTS_OK;
}

static fwts_framework_minor_test acpipld_tests[] = {
	{ acpipld_test1, "Check the ACPI _PLD methods exist only on the port connectable." },
	{ NULL, NULL }
};

static fwts_framework_ops acpipld_ops = {
	.description = "Check if the ACPI _PLD methods exist only on the port connectable.",
	.init        = acpipld_init,
	.deinit      = acpipld_deinit,
	.minor_tests = acpipld_tests
};

FWTS_REGISTER("acpipld", &acpipld_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
