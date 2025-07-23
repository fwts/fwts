/*
 * Copyright (C) 2010-2025 Canonical
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

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include "fwts_acpi_object_eval.h"

typedef enum {
	FWTS_WMI_EXPENSIVE 	= 0x00000001,
	FWTS_WMI_METHOD		= 0x00000002,
	FWTS_WMI_STRING		= 0x00000004,
	FWTS_WMI_EVENT		= 0x00000008
} fwts_wmi_flags;

typedef struct {
	const fwts_wmi_flags	flags;
	const char 		*name;
} fwts_wmi_flags_name;

typedef struct {
	const char *guid;	/* GUID string */
	const char *driver;	/* Kernel Driver name */
	const char *vendor;	/* Machine Vendor */
} fwts_wmi_known_guid;

/*
 *  Packed WMI WDG data
 */
typedef struct {
	uint8_t	guid[16];			/* GUID */
	union {
		uint8_t 	obj_id[2];	/* Object Identifier */
		struct {
			uint8_t	notify_id;	/* Notify Identifier */
			uint8_t	reserved;	/* Reserved */
		} notify;
	} id;
	uint8_t	instance;			/* Instance */
	uint8_t	flags;				/* fwts_wmi_flags */
} __attribute__ ((packed)) fwts_wdg_info;

/*
 *  Bunch of known WMI GUIDs in the kernel
 */
static fwts_wmi_known_guid fwts_wmi_known_guids[] = {
	{ "67C3371D-95A3-4C37-BB61-DD47B491DAAB", "acer-wmi",	"Acer" },
	{ "431F16ED-0C2B-444C-B267-27DEB140CF9C", "acer-wmi",	"Acer" },
	{ "6AF4F258-B401-42FD-BE91-3D4AC2D7C0D3", "acer-wmi",	"Acer" },
	{ "95764E09-FB56-4e83-B31A-37761F60994A", "acer-wmi",	"Acer" },
	{ "61EF69EA-865C-4BC3-A502-A0DEBA0CB531", "acer-wmi",	"Acer" },
	{ "676AA15E-6A47-4D9F-A2CC-1E6D18D14026", "acer-wmi",	"Acer" },
	{ "A90597CE-A997-11DA-B012-B622A1EF5492", "alienware-wmi", "Alienware" },
	{ "A80593CE-A997-11DA-B012-B622A1EF5492", "alienware-wmi", "Alienware" },
	{ "A70591CE-A997-11DA-B012-B622A1EF5492", "alienware-wmi", "Alienware" },
	{ "0B3CBB35-E3C2-45ED-91C2-4C5A6D195D1C", "asus-nb-wmi","Asus" },
	{ "97845ED0-4E6D-11DE-8A39-0800200C9A66", "asus-wmi",	"Asus" },
	{ "9DBB5994-A997-11DA-B012-B622A1EF5492", "dell-wmi-base", "Dell" },
	{ "8D9DDCBC-A997-11DA-B012-B622A1EF5492", "dell-wmi-descriptor", "Dell" },
	{ "284A0E6B-380E-472A-921F-E52786257FB4", "dell-wmi-aio","Dell" },
	{ "02314822-307C-4F66-BF0E-48AEAEB26CC8", "dell-wmi-aio","Dell" },
	{ "F6E4FE6E-909D-47cb-8BAB-C9F6F2F8D396", "dell-wmi-led","Dell" },
	{ "6932965F-1671-4CEB-B988-D3AB0A901919", "dell-wmi-privacy", "Dell" },
	{ "F1DDEE52-063C-4784-A11E-8A06684B9BF5", "dell-wmi-sysman", "Dell" },
	{ "F1DDEE52-063C-4784-A11E-8A06684B9BFA", "dell-wmi-sysman", "Dell" },
	{ "F1DDEE52-063C-4784-A11E-8A06684B9BF9", "dell-wmi-sysman", "Dell" },
	{ "0894B8D6-44A6-4719-97D7-6AD24108BFD4", "dell-wmi-sysman", "Dell" },
	{ "F1DDEE52-063C-4784-A11E-8A06684B9BF4", "dell-wmi-sysman", "Dell" },
	{ "70FE8229-D03B-4214-A1C6-1F884B1A892A", "dell-wmi-sysman", "Dell" },
	{ "8A42EA14-4F2A-FD45-6422-0087F7A7E608", "dell-wmi-ddv", "Dell" },
	{ "F1DDEE52-063C-4784-A11E-8A06684B9B01", "dell-smm-hwmon", "Dell" },
	{ "ABBC0F72-8EA1-11D1-00A0-C90629100000", "eeepc-wmi",	"Asus" },
	{ "97845ED0-4E6D-11DE-8A39-0800200C9A66", "eeepc-wmi",	"Asus" },
	{ "DEADBEEF-2001-0000-00A0-C90629100000", "gigabyte-wmi",	"Gigabyte" },
	{ "95F24279-4D7B-4334-9387-ACCDC67EF61C", "hp-wmi",	"HP"   },
	{ "5FB7F034-2C63-45e9-BE91-3D44E2C707E4", "hp-wmi",	"HP"   },
	{ "ABBC0F5B-8EA1-11D1-A000-C90629100000", "huawei-wmi", "Huawei" },
	{ "ABBC0F5C-8EA1-11D1-A000-C90629100000", "huawei-wmi", "Huawei" },
	{ "39142400-C6A3-40fa-BADB-8A2652834100", "huawei-wmi", "Huawei" },
	{ "59142400-C6A3-40fa-BADB-8A2652834100", "huawei-wmi", "Huawei" },
	{ "26CAB2E5-5CF1-46AE-AAC3-4A12B6BA50E6", "ideapad-laptop", "Ideapad"   },
	{ "56322276-8493-4CE8-A783-98C991274F5E", "ideapad-laptop", "Ideapad"   },
	{ "44FADEB1-B204-40F2-8581-394BBDC1B651", "intel-wmi-sbl-fw-update", "Intel" },
	{ "86CCFD48-205E-4A77-9C48-2021CBEDE341", "intel-wmi-thunderbolt", "Intel" },
	{ "E4FB94F9-7F2B-4173-AD1A-CD1D95086248", "lg-laptop",	"LG"   },
	{ "023B133E-49D1-4E10-B313-698220140DC2", "lg-laptop",	"LG"   },
	{ "37BE1AC0-C3F2-4B1F-BFBE-8FDEAF2814D6", "lg-laptop",	"LG"   },
	{ "911BAD44-7DF8-4FBB-9319-BABA1C4B293B", "lg-laptop",	"LG"   },
	{ "551A1F84-FBDD-4125-91DB-3EA8F44F1D45", "msi-wmi",	"MSI"  },
	{ "B6F3EEF2-3D2F-49DC-9DE3-85BCE18C62F2", "msi-wmi",	"MSI"  },
	{ "5B3CC38A-40D9-7245-8AE6-1145B751BE3F", "msi-wmi",	"MSI"  },
	{ "F6CB5C3C-9CAE-4EBD-B577-931EA32A2CC0", "mxm-wmi",	"MXM"  },
	{ "ABBC0F6F-8EA1-11D1-00A0-C90629100000", "peaq-wmi",	"PEAQ"  },
	{ "51F5230E-9677-46CD-A1CF-C0B23EE34DB7", "think-lmi", "Lenovo" },
	{ "98479A64-33F5-4E33-A707-8E251EBBC3A1", "think-lmi", "Lenovo" },
	{ "6A4B54EF-A5ED-4D33-9455-B0D9B48DF4B3", "think-lmi", "Lenovo" },
	{ "8ADB159E-1E32-455C-BC93-308A7ED98246", "think-lmi", "Lenovo" },
	{ "2651D9FD-911C-4B69-B94E-D0DED5963BD7", "think-lmi", "Lenovo" },
	{ "7364651A-132F-4FE7-ADAA-40C6C7EE2E3B", "think-lmi", "Lenovo" },
	{ "F7CC25EC-D20B-404C-8903-0ED4359C18AE", "surface3-wmi", "Surface3" },
	{ "C364AC71-36DB-495A-8494-B439D472A505", "tc110-wmi",	"HP Compaq" },
	{ "59142400-C6A3-40FA-BADB-8A2652834100", "toshiba_acpi", "Toshiba" },
	{ "05901221-D566-11D1-B2F0-00A0C9062910", "wmi-bmof", "Generic" },
	{ NULL, NULL, NULL }
};

/*
 *  WMI flag to text mappings
 */
static const fwts_wmi_flags_name wmi_flags_name[] = {
	{ FWTS_WMI_EXPENSIVE,	"Expensive" },
	{ FWTS_WMI_METHOD,	"Method" },
	{ FWTS_WMI_STRING,	"String" },
	{ FWTS_WMI_EVENT,	"Event" },
	{ 0,			NULL }
};

static bool wmi_advice_given;

/*
 *  wmi_init()
 *	initialize ACPI
 */
static int wmi_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  wmi_deinit()
 *	de-intialize ACPI
 */
static int wmi_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

/*
 *  fwts_wmi_known_guid()
 *	find any known GUID driver info
 */
static fwts_wmi_known_guid *wmi_find_guid(char *guid)
{
	fwts_wmi_known_guid *info;

	for (info = fwts_wmi_known_guids; info->guid != NULL; info++)
		if (strcmp(info->guid, guid) == 0)
			return info;

	return NULL;
}

/*
 *  wmi_strncat()
 *	build up a description of flag settings
 */
static char *wmi_strncat(char *dst, const char *str, const size_t dst_len)
{
	if (*dst)
		strncat(dst, " | ", dst_len);

	return strncat(dst, str, dst_len);
}

/*
 *  wmi_wdg_flags_to_text()
 *	turn WDG flags into a description string
 */
static char *wmi_wdg_flags_to_text(const fwts_wmi_flags flags)
{
	static char buffer[1024];
	int i;

	*buffer = 0;

	for (i = 0; wmi_flags_name[i].flags; i++)
		if (flags & wmi_flags_name[i].flags)
			wmi_strncat(buffer, wmi_flags_name[i].name, sizeof(buffer) - 1);

	if (!*buffer)
		strncpy(buffer, "None", sizeof(buffer) - 1);

	return buffer;
}

/*
 * wmi_acpi_get_parent_name()
 *	get parent name of acpi object, the result needs to be freed
 */
static char *wmi_acpi_get_parent_name(const char *object_name)
{
	size_t parent_name_length;
	char *parent_name;
	char *separator;

	separator = strrchr(object_name, '.');
	if (!separator)
		return NULL;

	parent_name_length = separator - object_name;
	parent_name = malloc(parent_name_length + 1);
	if (!parent_name)
		return NULL;

	memcpy(parent_name, object_name, parent_name_length);
	parent_name[parent_name_length] = '\0';

	return parent_name;
}

/*
 * wmi_acpi_method_count_on_object()
 *	count occurrence of methods on acpi object
 */
static int wmi_acpi_method_count_on_object(const char *object_name, const char *method_name)
{
	const size_t object_name_length = strlen(object_name);
	const size_t method_name_length = strlen(method_name);
	fwts_list_link *item;
	fwts_list *objects;
	int count = 0;

	objects = fwts_acpi_object_get_names();
	if (!objects)
		return 0;

	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char *, item);
		const size_t name_length = strlen(name);

		if (name_length != (object_name_length + method_name_length + 1))
			continue;

		if (strncmp(object_name, name, object_name_length))
			continue;

		if (!strncmp(method_name, name + name_length - method_name_length, method_name_length))
			count++;
	}

	return count;
}

/*
 * wmi_block_query_exist_count()
 *	check if the mandatory block query method exists for the WDG object
 */
static void wmi_block_query_exist_count(
	fwts_framework *fw,
	const fwts_wdg_info *info,
	const char *object_name,
	const char *guid_str)
{
	char wm_name[5];
	int count;

	snprintf(wm_name, sizeof(wm_name), "WQ%c%c", info->id.obj_id[0], info->id.obj_id[1]);

	count = wmi_acpi_method_count_on_object(object_name, wm_name);
	switch (count) {
	case 0:
		fwts_failed(fw, LOG_LEVEL_LOW, "WMIMissingQueryMethod",
			"GUID %s should have an associated query method %s defined, "
			"however this does not seem to exist.",
			guid_str, wm_name);
		break;
	case 1:
		fwts_passed(fw, "%s has associated query method %s.%s",
			guid_str, object_name, wm_name);
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_LOW, "WMIMultipleQueryMethod",
			"GUID %s has multiple associated query methods %s defined, "
			"this is a firmware bug that leads to ambiguous behaviour.",
			guid_str, wm_name);
		break;
	}
}

/*
 *  wmi_method_exist_count()
 *	check if an associated method exists for the WDG object
 */
static void wmi_method_exist_count(
	fwts_framework *fw,
	const fwts_wdg_info *info,
	const char *object_name,
	const char *guid_str)
{
	char wm_name[5];
	int  count;

	snprintf(wm_name, sizeof(wm_name), "WM%c%c",
		info->id.obj_id[0], info->id.obj_id[1]);

	count = wmi_acpi_method_count_on_object(object_name, wm_name);
	if (count == 0) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"WMIMissingMethod",
			"GUID %s should have an associated method WM%c%c defined, "
			"however this does not seem to exist.",
			guid_str, info->id.obj_id[0], info->id.obj_id[1]);
	} else if (count > 1) {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"WMIMultipleMethod",
			"GUID %s has multiple associated methods WM%c%c defined, "
			"this is a firmware bug that leads to ambiguous behaviour.",
			guid_str, info->id.obj_id[0], info->id.obj_id[1]);
	} else
		fwts_passed(fw, "%s has associated method %s.%s",
			guid_str, object_name, wm_name);
}

/*
 *  wmi_no_known_driver()
 *	grumble that the kernel does not have a known handler for this GUID
 */
static void wmi_no_known_driver(
	fwts_framework *fw,
	const char *guid_str)
{

	fwts_failed(fw, LOG_LEVEL_MEDIUM,
		"WMIUnknownGUID",
		"GUID %s is unknown to the kernel, a driver may need to "
		"be implemented for this GUID.", guid_str);

	if (!wmi_advice_given) {
		wmi_advice_given = true;
		fwts_log_advice(fw,
			"A WMI driver probably needs to be written for this WMI event. "
			"https://docs.kernel.org/wmi/driver-development-guide.html describes how "
			"to write an appropriate WMI driver.");
	}
}

/*
 *   wmi_dump_object()
 *	dump out a WDG object
 */
static void wmi_dump_object(fwts_framework *fw, const fwts_wdg_info *info)
{
	fwts_log_info_verbatim(fw, "    Flags          : 0x%2.2" PRIx8 " (%s)",
		info->flags, wmi_wdg_flags_to_text(info->flags));
	fwts_log_info_verbatim(fw, "    Object ID      : %c%c",
		info->id.obj_id[0], info->id.obj_id[1]);
	fwts_log_info_verbatim(fw, "    Instance       : 0x%2.2" PRIx8,
		info->instance);
}

/*
 *  wmi_known_driver()
 *	report info about the supported kernel driver
 */
static void wmi_known_driver(
	fwts_framework *fw,
	const fwts_wmi_known_guid *known)
{
	/* If we recognise the GUID then we may as well report this info */
	if (known) {
		fwts_log_info_verbatim(fw, "    Driver         : %s (%s)",
			known->driver, known->vendor);
	}
}

/*
 *  wmi_parse_wdg_data()
 *	parse over raw _WDG data and dump + sanity check the objects
 */
static void wmi_parse_wdg_data(
	fwts_framework *fw,
	const char *name,
	const size_t size,
	const uint8_t *wdg_data)
{
	size_t i;
	const fwts_wdg_info *info = (const fwts_wdg_info *)wdg_data;
	bool all_events_known = true;
	bool events = false;
	char *acpi_object_name;

	acpi_object_name = wmi_acpi_get_parent_name(name);
	if (!acpi_object_name) {
		fwts_log_info(fw, "Unable to the get parent name of %s", name);
		return;
	}


	for (i = 0; i < (size / sizeof(fwts_wdg_info)); i++, info++) {
		const uint8_t *guid = info->guid;
		char guid_str[37];
		const fwts_wmi_known_guid *known;

		fwts_guid_buf_to_str(guid, guid_str, sizeof(guid_str));
		fwts_log_nl(fw);
		fwts_log_info_verbatim(fw, "%s (%zd of %zd)",
			name, i + 1, size / sizeof(fwts_wdg_info));
		fwts_log_info_verbatim(fw, "  GUID: %s", guid_str);
		known = wmi_find_guid(guid_str);

		if (info->flags & FWTS_WMI_METHOD) {
			fwts_log_info_verbatim(fw, "  WMI Method:");
			wmi_dump_object(fw, info);
			wmi_known_driver(fw, known);
			wmi_method_exist_count(fw, info, acpi_object_name, guid_str);
		} else if (info->flags & FWTS_WMI_EVENT) {
			events = true;
			fwts_log_info_verbatim(fw, "  WMI Event:");
			fwts_log_info_verbatim(fw, "    Flags          : 0x%2.2" PRIx8 " (%s)",
				info->flags, wmi_wdg_flags_to_text(info->flags));
			fwts_log_info_verbatim(fw, "    Notification ID: 0x%2.2" PRIx8,
				info->id.notify.notify_id);
			fwts_log_info_verbatim(fw, "    Reserved       : 0x%2.2" PRIx8,
				info->id.notify.reserved);
			fwts_log_info_verbatim(fw, "    Instance       : 0x%2.2" PRIx8,
				info->instance);
			wmi_known_driver(fw, known);

			/* To handle events we really need a custom kernel driver */
			if (!known) {
				wmi_no_known_driver(fw, guid_str);
				all_events_known = false;
			}
		} else {
			fwts_log_info_verbatim(fw, "  WMI Block:");
			wmi_dump_object(fw, info);
			wmi_known_driver(fw, known);
			wmi_block_query_exist_count(fw, info, acpi_object_name, guid_str);
		}


		if (info->instance == 0)
			fwts_failed(fw, LOG_LEVEL_LOW, "WMIZeroInstance",
				    "GUID %s has zero instances", guid_str);
		else
			fwts_passed(fw, "%s has more than zero instances", guid_str);
	}

	if (events && all_events_known)
		fwts_passed(fw, "All events associated with %s are handled by a kernel driver.", name);

	free(acpi_object_name);
}

static int wmi_test1(fwts_framework *fw)
{
	fwts_list_link	*item;
	fwts_list *objects;
	const size_t name_len = 4;
	bool wdg_found = false;

	if ((objects = fwts_acpi_object_get_names()) == NULL) {
		fwts_log_info(fw, "Cannot find any ACPI objects");
		return FWTS_ERROR;
	}

	wmi_advice_given = false;

	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char*, item);
		const size_t len = strlen(name);

		if (strncmp("_WDG", name + len - name_len, name_len) == 0) {
			ACPI_OBJECT_LIST arg_list;
			ACPI_BUFFER buf;
			ACPI_OBJECT *obj;
			int ret;

			arg_list.Count   = 0;
			arg_list.Pointer = NULL;

			ret = fwts_acpi_object_evaluate(fw, name, &arg_list, &buf);
			if ((ACPI_FAILURE(ret) != AE_OK) || (buf.Pointer == NULL))
				continue;

			/*  Do we have a valid buffer to dump? */
			obj = buf.Pointer;
			if ((obj->Type == ACPI_TYPE_BUFFER) &&
			    (obj->Buffer.Pointer != NULL) &&
			    (obj->Buffer.Length > 0)) {
				wmi_parse_wdg_data(fw, name,
					obj->Buffer.Length,
					(uint8_t*)obj->Buffer.Pointer);
				wdg_found = true;
			}
			free(buf.Pointer);
		}
	}

	if (!wdg_found) {
		fwts_log_info(fw, "No ACPI _WDG WMI data found.");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

static fwts_framework_minor_test wmi_tests[] = {
	{ wmi_test1, "Windows Management Instrumentation test." },
	{ NULL, NULL }
};

static fwts_framework_ops wmi_ops = {
	.description = "Extract and analyse Windows Management "
		       "Instrumentation (WMI).",
	.init	     = wmi_init,
	.deinit	     = wmi_deinit,
	.minor_tests = wmi_tests
};

FWTS_REGISTER("wmi", &wmi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
