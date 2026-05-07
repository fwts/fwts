/*
 * Copyright (C) 2015-2026 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

#include "fwts_acpi_object_eval.h"

typedef struct {
	char *hid_name;
	ACPI_OBJECT *hid_obj;	/* this can be _HID or _CID */
	ACPI_OBJECT *uid_obj;
} acpi_ids;

static fwts_list *hid_list;

static int uniqueid_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	if ((hid_list = fwts_list_new()) == NULL)
		return FWTS_ERROR;

	return FWTS_OK;
}

static int uniqueid_deinit(fwts_framework *fw)
{
	fwts_keymap_free(hid_list);
	return fwts_acpi_deinit(fw);
}

static int uniqueid_evaluate_method(fwts_framework *fw,
	char *name,
	ACPI_OBJECT *args,
	int num_args,
	fwts_method_return check_func,
	void *private)
{
	fwts_list *methods;
	const size_t name_len = strlen(name);

	if ((methods = fwts_acpi_object_get_names()) != NULL) {
		fwts_list_link	*item;

		fwts_list_foreach(item, methods) {
			char *method_name = fwts_list_data(char*, item);
			ACPI_HANDLE method_handle;
			ACPI_OBJECT_TYPE type;
			ACPI_STATUS status;

			size_t len = strlen(method_name);
			if (strncmp(name, method_name + len - name_len, name_len) == 0) {
				ACPI_OBJECT_LIST  arg_list;

				status = AcpiGetHandle (NULL, method_name, &method_handle);
				if (ACPI_FAILURE(status)) {
					fwts_warning(fw, "Failed to get handle for object %s.", name);
					continue;
				}
				status = AcpiGetType(method_handle, &type);
				if (ACPI_FAILURE(status)) {
					fwts_warning(fw, "Failed to get object type for %s.",name);
					continue;
				}

				if (type == ACPI_TYPE_LOCAL_SCOPE)
					continue;

				arg_list.Count   = num_args;
				arg_list.Pointer = args;
				method_evaluate_found_method(fw, method_name,
					check_func, private, &arg_list);
			}
		}
	}

	return FWTS_OK;
}

static bool is_uniqueid_equal(fwts_framework *fw, acpi_ids *obj1, acpi_ids *obj2)
{
	bool hid_match = false;

	if (obj1 == NULL || obj2 == NULL)
		return false;

	/* _HID and _CID are in the same device */
	if (!strncmp(obj1->hid_name, obj2->hid_name, strlen(obj1->hid_name) - 4))
		return false;

	if ((obj1->hid_obj->Type != obj2->hid_obj->Type) || (obj1->uid_obj->Type != obj2->uid_obj->Type))
		return false;

	switch (obj1->hid_obj->Type) {
	case ACPI_TYPE_STRING:
		if (strcmp(obj1->hid_obj->String.Pointer, obj2->hid_obj->String.Pointer))
			return false;
		hid_match = true;
		break;
	case ACPI_TYPE_INTEGER:
		if (obj1->hid_obj->Integer.Value != obj2->hid_obj->Integer.Value)
			return false;
		hid_match = true;
		break;
	default:
		/*
		 * For _CID, it may be of Package type.
		 * Compare all items in the package, and if any item matches,
		 * it is considered a match (as the OS might use the same one).
		 * Then, further verify the _UID value.
		 */
		if ((strstr(obj1->hid_name, "_CID") != NULL) &&
				obj1->hid_obj->Type == ACPI_TYPE_PACKAGE) {
			for (int i = 0; i < obj1->hid_obj->Package.Count; i++) {
				for (int j = 0; j < obj2->hid_obj->Package.Count; j++) {
					switch (obj1->hid_obj->Package.Elements[i].Type) {
					case ACPI_TYPE_STRING:
						if (!strcmp(obj1->hid_obj->Package.Elements[i].String.Pointer,
							obj2->hid_obj->Package.Elements[j].String.Pointer))
							hid_match = true;
						break;
					case ACPI_TYPE_INTEGER:
						if (obj1->hid_obj->Package.Elements[i].Integer.Value ==
							obj2->hid_obj->Package.Elements[j].Integer.Value)
							hid_match = true;
						break;
					}
					if (hid_match)
						break;
				}
				if (hid_match)
					break;
			}
			if (!hid_match)
				return false;
			break;
		}

		fwts_log_error(fw, "Unknow type for _HID or _CID.");
		return true;
	}

	switch (obj1->uid_obj->Type) {
	case ACPI_TYPE_STRING:
		if (strcmp(obj1->uid_obj->String.Pointer, obj2->uid_obj->String.Pointer) && hid_match)
			return false;
		break;
	case ACPI_TYPE_INTEGER:
		if (obj1->uid_obj->Integer.Value != obj2->uid_obj->Integer.Value && hid_match)
			return false;
		break;
	}

	return true;
}

static void unique_HID_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	const size_t name_len = strlen(name) + 1;
	char uid_name[name_len];
	ACPI_OBJECT *hid_obj, *uid_obj;
	ACPI_BUFFER hid_buf, uid_buf;
	fwts_list_link *item;
	bool passed = true;
	ACPI_STATUS status;
	acpi_ids *obj1;

	FWTS_UNUSED(buf);
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	strlcpy(uid_name, name, name_len);
	uid_name[name_len - 4] = 'U';

	status = fwts_acpi_object_evaluate(fw, name, NULL, &hid_buf);
	if (ACPI_FAILURE(status))
		return;
	hid_obj = hid_buf.Pointer;

	status = fwts_acpi_object_evaluate(fw, uid_name, NULL, &uid_buf);
	if (ACPI_FAILURE(status))
		return;
	uid_obj = uid_buf.Pointer;

	obj1 = (acpi_ids *)calloc(1, sizeof(acpi_ids));
	if (!obj1)
		return;

	obj1->hid_name = name;
	obj1->hid_obj = hid_obj;
	obj1->uid_obj = uid_obj;

	fwts_list_foreach(item, hid_list) {
		acpi_ids *obj2 = fwts_list_data(acpi_ids*, item);
		if (is_uniqueid_equal(fw, obj1, obj2)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "HardwareIDNotUnique",
				"%s/_UID conflict with %s/_UID", name, obj2->hid_name);
			break;
		}
	}

	if (passed) {
		fwts_list_append(hid_list, obj1);
		fwts_passed(fw, "%s/_UID is unique.", name);
	} else {
		free(obj1);
	}
}

static int uniqueid_test1(fwts_framework *fw)
{
	return uniqueid_evaluate_method(fw, "_HID", NULL, 0, unique_HID_return, NULL);
}

static int uniqueid_test2(fwts_framework *fw)
{
	return uniqueid_evaluate_method(fw, "_CID", NULL, 0, unique_HID_return, NULL);
}

static fwts_framework_minor_test uniqueid_tests[] = {
	{ uniqueid_test1, "ACPI _HID unique ID test." },
	{ uniqueid_test2, "ACPI _CID unique ID test." },
	{ NULL, NULL }
};

static fwts_framework_ops uniqueid_ops = {
	.description = "ACPI Unique IDs test.",
	.init        = uniqueid_init,
	.deinit      = uniqueid_deinit,
	.minor_tests = uniqueid_tests
};

FWTS_REGISTER("uniqueid", &uniqueid_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)

#endif
