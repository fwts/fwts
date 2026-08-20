/*
 * Copyright (C) 2025-2026 Canonical
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

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

/*
 * ACPI 6.6 Section 5.2.27 Non HDAudio Link Table (NHLT)
 */

static fwts_acpi_table_info *table;
acpi_table_init(NHLT, &table)

static bool nhlt_validate_format_configs(
	fwts_framework *fw,
	uint8_t *table_data,
	uint32_t *offset,
	uint32_t limit,
	bool *passed)
{
	fwts_acpi_table_nhlt_formats_configuration *formats_conf;
	uint8_t i;
	uint32_t config_size;

	if (*offset > limit || sizeof(uint8_t) > (limit - *offset)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTFieldTooShort",
			"NHLT FormatsConfig header exceeds table boundary.");
		*passed = false;
		return false;
	}

	formats_conf = (fwts_acpi_table_nhlt_formats_configuration *)(table_data + *offset);
	*offset += sizeof(formats_conf->formats_count);

	for (i = 0; i < formats_conf->formats_count; i++) {
		if (*offset > limit ||
		    sizeof(fwts_acpi_table_nhlt_wave_format_ext) > (limit - *offset)) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"NHLTFieldTooShort",
				"NHLT Format structure exceeds table boundary.");
			*passed = false;
			return false;
		}

		*offset += sizeof(fwts_acpi_table_nhlt_wave_format_ext);

		if (*offset > limit || sizeof(uint32_t) > (limit - *offset)) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"NHLTFieldTooShort",
				"NHLT Format Config header exceeds table boundary.");
			*passed = false;
			return false;
		}

		memcpy(&config_size, table_data + *offset, sizeof(config_size));
		*offset += sizeof(uint32_t);

		if (*offset > limit || config_size > (limit - *offset)) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"NHLTFieldTooShort",
				"NHLT Format Config exceeds table boundary.");
			*passed = false;
			return false;
		}

		*offset += config_size;
	}

	return true;
}

static bool nhlt_validate_endpoint(
	fwts_framework *fw,
	uint8_t index,
	uint8_t *table_data,
	uint32_t *endpoint_offset,
	uint32_t table_length,
	bool *passed)
{
	fwts_acpi_table_nhlt_ep_descriptor *ep;
	uint32_t endpoint_end, offset, device_config_size;

	if (*endpoint_offset > table_length ||
	    sizeof(fwts_acpi_table_nhlt_ep_descriptor) > (table_length - *endpoint_offset)) {
		return false;
	}

	ep = (fwts_acpi_table_nhlt_ep_descriptor *)(table_data + *endpoint_offset);

	if (ep->len < sizeof(fwts_acpi_table_nhlt_ep_descriptor)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTShortEndpoint",
			"NHLT endpoint %" PRIu8 " has length %" PRIu32
			" which is shorter than the descriptor header.", index, ep->len);
		*passed = false;
		return false;
	}

	if (*endpoint_offset > table_length || ep->len > (table_length - *endpoint_offset)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTFieldTooShort",
			"NHLT endpoint payload exceeds table boundary.");
		*passed = false;
		return false;
	}

	if (ep->link_type > 6) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTBadLinkType",
			"NHLT endpoint %" PRIu8 " uses reserved Link Type %" PRIu8 ".",
			index, ep->link_type);
		*passed = false;
	}

	if (ep->direction > 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTBadDirection",
			"NHLT endpoint %" PRIu8 " has invalid Direction %" PRIu8
			", must be 0 (Render) or 1 (Capture).",
			index, ep->direction);
		*passed = false;
	}

	if (ep->link_type == 2 && ep->device_type > 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTBadDeviceType",
			"NHLT endpoint %" PRIu8 " uses reserved PDM Device Type %" PRIu8 ".",
			index, ep->device_type);
		*passed = false;
	}

	if (ep->link_type == 3 && ep->device_type > 4) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTBadDeviceType",
			"NHLT endpoint %" PRIu8 " uses reserved SSP Device Type %" PRIu8 ".",
			index, ep->device_type);
		*passed = false;
	}

	endpoint_end = *endpoint_offset + ep->len;
	offset = *endpoint_offset + sizeof(fwts_acpi_table_nhlt_ep_descriptor);

	if (offset > endpoint_end || sizeof(uint32_t) > (endpoint_end - offset)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTFieldTooShort",
			"NHLT Device Config header exceeds endpoint boundary.");
		*passed = false;
		return false;
	}

	memcpy(&device_config_size, table_data + offset, sizeof(device_config_size));
	offset += sizeof(uint32_t);

	if (offset > endpoint_end || device_config_size > (endpoint_end - offset)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTFieldTooShort",
			"NHLT Device Config exceeds endpoint boundary.");
		*passed = false;
		return false;
	}

	offset += device_config_size;

	if (!nhlt_validate_format_configs(fw, table_data, &offset, endpoint_end, passed))
		return false;

	if (offset < endpoint_end) {
		uint8_t count, i;

		if (sizeof(uint8_t) > (endpoint_end - offset)) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"NHLTFieldTooShort",
				"NHLT DevicesInfo header exceeds endpoint boundary.");
			*passed = false;
			return false;
		}

		count = *(table_data + offset);
		offset += sizeof(uint8_t);

		for (i = 0; i < count; i++) {
			if (sizeof(fwts_acpi_table_nhlt_device_info) > (endpoint_end - offset)) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
					"NHLTFieldTooShort",
					"NHLT DeviceInfo exceeds endpoint boundary.");
				*passed = false;
				return false;
			}

			offset += sizeof(fwts_acpi_table_nhlt_device_info);
		}
	}

	offset = endpoint_end;

	*endpoint_offset = endpoint_end;

	return true;
}

static int nhlt_test1(fwts_framework *fw)
{
	fwts_acpi_table_nhlt *nhlt = (fwts_acpi_table_nhlt *)table->data;
	uint32_t offset, table_length = table->length;
	bool passed = true;
	uint8_t i;

	if (table_length < sizeof(fwts_acpi_table_nhlt)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"NHLTTableTooSmall",
			"NHLT table is shorter than the header.");
		return FWTS_OK;
	}

	offset = sizeof(fwts_acpi_table_nhlt);

	for (i = 0; i < nhlt->ep_descriptor_count; i++) {
		if (!nhlt_validate_endpoint(fw, i + 1, (uint8_t *)table->data, &offset,
			table_length, &passed))
			return FWTS_OK;
	}

	if (offset < table_length) {
		uint32_t config_size;

		if (sizeof(uint32_t) > (table_length - offset)) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"NHLTFieldTooShort",
				"NHLT OED Config header exceeds table boundary.");
			passed = false;
			return FWTS_OK;
		}

		memcpy(&config_size, (uint8_t *)table->data + offset, sizeof(config_size));
		offset += sizeof(uint32_t);

		if (config_size > (table_length - offset)) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"NHLTFieldTooShort",
				"NHLT OED Config exceeds table boundary.");
			passed = false;
			return FWTS_OK;
		}

		offset += config_size;
	}

	if (passed)
		fwts_passed(fw, "NHLT table complies with ACPI 6.6 Section 5.2.27.");

	return FWTS_OK;
}

static fwts_framework_minor_test nhlt_tests[] = {
	{ nhlt_test1, "NHLT table validation test." },
	{ NULL, NULL }
};

static fwts_framework_ops nhlt_ops = {
	.description = "NHLT Non HDAudio Link Table test.",
	.init        = NHLT_init,
	.minor_tests = nhlt_tests
};

FWTS_REGISTER("nhlt", &nhlt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
