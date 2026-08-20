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
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

/* PNP BIOS header, see spec www.osdever.net/documents/PNPBIOSSpecification-v1.0a.pdf */

#define PNP_REGION_START	(0x000f0000)
#define PNP_REGION_END  	(0x000fffff)
#define PNP_REGION_SIZE		(PNP_REGION_END - PNP_REGION_START)

#define PNP_CONTROL_FIELD_POLLING 	(0x1)
#define PNP_VERSION_1			(0x10)	/* In BCD */
#define PNP_SIGNATURE			"$PnP"

typedef struct {
	uint8_t		signature[4];
	uint8_t		version;
	uint8_t		length;
	uint16_t	control_field;
	uint8_t		checksum;
	uint32_t	event_notification_addr;
	uint16_t	RM_offset_entry;
	uint16_t	RM_code_segment_addr;
	uint16_t	PM_offset_entry;
	uint32_t	PM_code_segment_addr;
	uint32_t	OEM_device_ID;
	uint16_t	RM_data_addr;
	uint32_t	PM_data_addr;
}  __attribute__ ((packed)) pnp_header;

static char *oem_device_id(const uint32_t id)
{
	static char buf[12];

	if (id == 0)
		return " (undefined)";

	/* Encoded like EISA product identifiers, icky */
	snprintf(buf, sizeof(buf), " (%c%c%c%02X%02X)",
		0x40 + ((id >> 2) & 0x1f),
		0x40 + ((id & 0x3) << 3) + ((id >> 13) & 0x7),
		0x40 + ((id >> 8) & 0x1f),
                (id >> 16) & 0xff, (id >> 24) & 0xff);

	return buf;
}

static int pnp_test1(fwts_framework *fw)
{
	uint8_t *mem;
	int i;
	int found = 0;

	static const char *pnp_control_field[] = {
		"Not supported",
		"Handled by polling",
		"Asynchronous",
		"Invalid"
	};

	fwts_log_info(fw,
		"This test tries to find and sanity check the "
		"Plug and Play BIOS Support Installation Check structure.");

        if ((mem = fwts_mmap(PNP_REGION_START,
			PNP_REGION_SIZE)) == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap firmware region.");
		return FWTS_ERROR;
	}

	fwts_log_nl(fw);

	for (i = 0; i < PNP_REGION_SIZE; i+= 16) {
		pnp_header *pnp = (pnp_header*)(mem+i);

		/* Skip regions that are not readable */
		if (fwts_safe_memread(pnp, sizeof(pnp_header)) != FWTS_OK)
			continue;
		if ((memcmp(pnp->signature, PNP_SIGNATURE, 4) == 0) &&
		    (fwts_checksum(mem+i, sizeof(pnp_header)) == 0)) {
			fwts_log_info(fw, "Found PnP Installation Check structure at 0x%8.8x", PNP_REGION_START+i);
			fwts_log_info_verbatim(fw, "  Signature                          : %4.4s",
				pnp->signature);
			fwts_log_info_verbatim(fw, "  Version                            : 0x%2.2x (%d.%d)",
				pnp->version, pnp->version >> 4, pnp->version & 0xf);
			fwts_log_info_verbatim(fw, "  Length                             : 0x%4.4x bytes",
				pnp->length);
			fwts_log_info_verbatim(fw, "  Control Field                      : 0x%4.4x (%s)",
				pnp->control_field, pnp_control_field[pnp->control_field & 0x3]);
			fwts_log_info_verbatim(fw, "  Event Notification Flag Address    : 0x%8.8x%s",
				pnp->event_notification_addr,
				pnp->event_notification_addr ? "" : " (undefined)");
			fwts_log_info_verbatim(fw, "  Real Mode 16 bit Code Address      : 0x%4.4x:%4.4x",
				pnp->RM_code_segment_addr, pnp->RM_offset_entry);
			fwts_log_info_verbatim(fw, "  Real Mode 16 bit Data Address      : 0x%4.4x:%4.4x",
				pnp->RM_data_addr, 0);
			fwts_log_info_verbatim(fw, "  16 bit Protected Mode Code Address : 0x%8.8x",
				pnp->PM_code_segment_addr + pnp->PM_offset_entry);
			fwts_log_info_verbatim(fw, "  16 bit Protected Mode Data Address : 0x%8.8x",
				pnp->PM_data_addr);

			fwts_log_info_verbatim(fw, "  OEM Device Identifier              : 0x%8.8x%s",
				pnp->OEM_device_ID, oem_device_id(pnp->OEM_device_ID));
			fwts_log_nl(fw);
			found++;

			/* Very simple sanity checks */

			if (pnp->version == PNP_VERSION_1)
				fwts_passed(fw,
					"Version %u.%u detected.",
					PNP_VERSION_1 >> 4, PNP_VERSION_1 & 0xf);
			else
				fwts_failed(fw, LOG_LEVEL_LOW, "PNPStructBadVersion",
					"Version %u.%u detected, expecting version %u.%u",
					pnp->version >> 4, pnp->version & 0xf,
					PNP_VERSION_1 >> 4, PNP_VERSION_1 & 0xf);


			if (pnp->length == sizeof(pnp_header))
				fwts_passed(fw,
					"PnP Installation Check structure is the "
					"correct length of %d bytes.",
					(int)sizeof(pnp_header));
			else
				fwts_failed(fw, LOG_LEVEL_LOW, "PNPStructBadLength",
					"PnP Installation Check structure is the "
					"wrong length,  got %d bytes, expected %d.",
					(int)pnp->length, (int)sizeof(pnp_header));

			if ((pnp->control_field & 3) == PNP_CONTROL_FIELD_POLLING) {
				if (pnp->event_notification_addr != 0)
					fwts_passed(fw,
						"The control field indicates that "
						"polling is being used and the "
						"notification flag address is defined.");
				else
					fwts_failed(fw, LOG_LEVEL_LOW,
						"PNPStructBadFlagAddr",
						"The control field indicates that "
						"event notification is handled by "
						"polling the notification flag address "
						"however this address is not defined.");
			}
		}
	}

	if (found == 0)
		fwts_log_info(fw,
			"Could not find PnP BIOS Support Installation Check structure. "
			"This is not necessarily a failure.");
	else if (found > 1)
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"PNPMultipleTables",
			"Found %d instances of PCI Routing Tables, there should only be 1.", found);

        (void)fwts_munmap(mem, PNP_REGION_SIZE);

	return FWTS_OK;
}

static fwts_framework_minor_test pnp_tests[] = {
	{ pnp_test1, "PnP BIOS Support Installation structure test." },
	{ NULL, NULL }
};

static fwts_framework_ops pnp_ops = {
	.description = "BIOS Support Installation structure test.",
	.minor_tests = pnp_tests
};

FWTS_REGISTER("pnp", &pnp_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
