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
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include "fwts_acpi_object_eval.h"

/*
 *  plddump_init()
 *	initialize ACPI
 */
static int plddump_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  plddump_deinit
 *	de-intialize ACPI
 */
static int plddump_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

static inline uint64_t get_bit(
	const uint8_t *data,
	const uint32_t offset)
{
	return (data[offset / 8] >> (offset % 8)) & 1;
}

static uint64_t get_bits(
	const uint8_t *data,
	const uint32_t offset,
	const uint32_t nbits)
{
	uint64_t val = 0;
	uint32_t i;

	for (i = 0; i < nbits; i++)
		val |= (get_bit(data, offset + i) << i);

	return val;
}

static const char *yesno[] = {
	"No",
	"Yes"
};

static const char *panel_surface[] = {
	"Top",
	"Bottom",
	"Left",
	"Right",
	"Front",
	"Back",
	"Unknown",
	"Out of specification"
};

static const char *vertical_position[] = {
	"Upper",
	"Center",
	"Lower",
	"Unknown"
};

static const char *horizontal_position[] = {
	"Left",
	"Center",
	"Right",
	"Unknown"
};

static const char *shape[] = {
	"Round",
	"Oval",
	"Square",
	"Vertical Rectangle",
	"Horizontal Rectangle",
	"Vertical Trapezoid",
	"Horizontal Trapezoid",
	"Unknown",
	"Chamfered",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};

static void plddump_buffer(
	fwts_framework *fw,
	const char *name,
	const void *data,
	const uint32_t length)
{
	fwts_log_nl(fw);
	fwts_log_info_verbatim(fw, "Name: %s", name);
	fwts_log_info_verbatim(fw, "  Revision:            %" PRIu64,
		get_bits(data, 0, 7));
	fwts_log_info_verbatim(fw, "  Ignore Color:        %" PRIu64 " (%s)",
		get_bits(data, 7, 1),
		yesno[get_bits(data, 7, 1)]);
	if (!get_bits(data, 7, 1)) {
		fwts_log_info_verbatim(fw, "  Red:                 0x%2.2" PRIx64,
			get_bits(data, 8, 8));
		fwts_log_info_verbatim(fw, "  Green:               0x%2.2" PRIx64,
			get_bits(data, 16, 8));
		fwts_log_info_verbatim(fw, "  Blue:                0x%2.2" PRIx64,
			get_bits(data, 24, 8));
	}
	fwts_log_info_verbatim(fw, "  Width:               %" PRIu64 " mm",
		get_bits(data, 32, 16));
	fwts_log_info_verbatim(fw, "  Height:              %" PRIu64 " mm",
		get_bits(data, 48, 16));
	fwts_log_info_verbatim(fw, "  User Visible:        %" PRIu64 " (%s)",
		get_bits(data, 64, 1),
		yesno[get_bits(data, 64, 1)]);
	fwts_log_info_verbatim(fw, "  Dock:                %" PRIu64 " (%s)",
		get_bits(data, 65, 1),
		yesno[get_bits(data, 65, 1)]);
	fwts_log_info_verbatim(fw, "  Lid:                 %" PRIu64 " (%s)",
		get_bits(data, 66, 1),
		yesno[get_bits(data, 66, 1)]);
	fwts_log_info_verbatim(fw, "  Panel Surface:       %" PRIu64 " (%s)",
		get_bits(data, 67, 3),
		panel_surface[get_bits(data, 67, 3)]);
	if (get_bits(data, 67, 3) < 6) {
		fwts_log_info_verbatim(fw, "  Vertical Position:   %" PRIu64 " (%s)",
			get_bits(data, 70, 2),
			vertical_position[get_bits(data, 70, 2)]);
		fwts_log_info_verbatim(fw, "  Horizontal Position: %" PRIu64 " (%s)",
			get_bits(data, 72, 2),
			horizontal_position[get_bits(data, 72, 2)]);
	}
	fwts_log_info_verbatim(fw, "  Shape:               %" PRIu64 " (%s)",
		get_bits(data, 74, 4),
		shape[get_bits(data, 74, 4)]);
	fwts_log_info_verbatim(fw, "  Group Orientation:   %" PRIu64,
		get_bits(data, 78, 1));
	fwts_log_info_verbatim(fw, "  Group Token:         %" PRIu64,
		get_bits(data, 79, 8));
	fwts_log_info_verbatim(fw, "  Group Position:      %" PRIu64,
		get_bits(data, 87, 8));
	fwts_log_info_verbatim(fw, "  Bay:                 %" PRIu64 " (%s)",
		get_bits(data, 95, 1),
		yesno[get_bits(data, 95, 1)]);
	fwts_log_info_verbatim(fw, "  Ejectable:           %" PRIu64 " (%s)",
		get_bits(data, 96, 1),
		yesno[get_bits(data, 96, 1)]);
	fwts_log_info_verbatim(fw, "  OSPM Ejection Rqd:   %" PRIu64 " (%s)",
		get_bits(data, 97, 1),
		yesno[get_bits(data, 97, 1)]);
	fwts_log_info_verbatim(fw, "  Cabinet Number:      %" PRIu64,
		get_bits(data, 98, 8));
	fwts_log_info_verbatim(fw, "  Card Cage Number:    %" PRIu64,
		get_bits(data, 106, 8));
	fwts_log_info_verbatim(fw, "  Reference:           %" PRIu64 " (%s)",
		get_bits(data, 114, 1),
		yesno[get_bits(data, 114, 1)]);
	fwts_log_info_verbatim(fw, "  Rotation:            %" PRIu64 " (%" PRIu64 " degrees)",
		get_bits(data, 115, 3),
		get_bits(data, 115, 3) * 45);
	fwts_log_info_verbatim(fw, "  Order:               %" PRIu64,
		get_bits(data, 119, 5));
	if (length >= 20) {
		fwts_log_info_verbatim(fw, "  Vertical Offset:     %" PRIu64 " mm",
			get_bits(data, 128, 16));
		fwts_log_info_verbatim(fw, "  Horizontal Offset:   %" PRIu64 " mm",
			get_bits(data, 144, 16));
	}
}

static int plddump_test1(fwts_framework *fw)
{
	fwts_list_link	*item;
	fwts_list *objects;
	const size_t name_len = 4;
	bool found = false;

	if ((objects = fwts_acpi_object_get_names()) == NULL) {
		fwts_log_info(fw, "Cannot find any ACPI objects");
		return FWTS_ERROR;
	}

	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char *, item);
		const size_t len = strlen(name);

		if (!strncmp("_PLD", name + len - name_len, name_len)) {
			ACPI_OBJECT_LIST arg_list;
			ACPI_BUFFER buf;
			ACPI_OBJECT *obj;
			int ret;

			arg_list.Count   = 0;
			arg_list.Pointer = NULL;

			ret = fwts_acpi_object_evaluate(fw, name, &arg_list, &buf);
			if ((ACPI_FAILURE(ret) != AE_OK) || (buf.Pointer == NULL))
				continue;

			/*  Do we have a valid buffer in the package to dump? */
			obj = buf.Pointer;
			if ((obj->Type == ACPI_TYPE_PACKAGE) &&
			    (obj->Package.Count) &&
			    (obj->Package.Elements[0].Type == ACPI_TYPE_BUFFER) &&
			    (obj->Package.Elements[0].Buffer.Pointer != NULL) &&
			    (obj->Package.Elements[0].Buffer.Length >= 16)) {
				plddump_buffer(fw,
					name,
					obj->Package.Elements[0].Buffer.Pointer,
					obj->Package.Elements[0].Buffer.Length);
				found = true;
			}
			free(buf.Pointer);
		}
	}

	if (!found)
		fwts_log_info_verbatim(fw, "No _PLD objects found.");

	return FWTS_OK;
}

static fwts_framework_minor_test plddump_tests[] = {
	{ plddump_test1, "Dump ACPI _PLD (Physical Device Location)." },
	{ NULL, NULL }
};

static fwts_framework_ops plddump_ops = {
	.description = "Dump ACPI _PLD (Physical Device Location).",
	.init        = plddump_init,
	.deinit      = plddump_deinit,
	.minor_tests = plddump_tests
};

FWTS_REGISTER("plddump", &plddump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)

#endif
