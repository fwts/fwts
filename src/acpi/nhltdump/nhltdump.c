/*
 * Copyright (C) 2022-2026 Canonical
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

static fwts_acpi_table_info *table;
acpi_table_init(NHLT, &table)

static void nhltdump_data_hexdump(fwts_framework *fw, uint8_t *data, size_t size)
{
	size_t i;

	for (i = 0; i < size; i += 16) {
		char buffer[128];
		size_t left = size - i;

		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw,  "    %s", buffer + 2);
	}
}

static char *nhltdump_linktype_to_string(const uint8_t linktype)
{
	char *str;

	switch (linktype) {
	case 0:
		str = "HD-Audio";
		break;
	case 1:
		str = "DSP";
		break;
	case 2:
		str = "PDM";
		break;
	case 3:
		str = "SSP";
		break;
	case 4:
		str = "Slimbus";
		break;
	case 5:
		str = "SoundWire";
		break;
	case 6:
		str = "USB Audio Offload";
		break;
	default:
		str = "Reserved";
		break;
	}

	return str;
}

static char *nhltdump_devtype_to_string(const uint8_t linktype, const uint8_t devtype)
{
	char *str;

	if (linktype == 2) {
		switch (devtype) {
		case 0:
			str = "PDM";
			break;
		case 1:
			str = "PDM (Skylake-only)";
			break;
		default:
			str = "Reserved";
			break;
		}
	} else if (linktype == 3) {
		switch (devtype) {
		case 0:
			str = "Bluetooth A2DP/HFP";
			break;
		case 1:
			str = "Frequency Modulation (Radio)";
			break;
		case 2:
			str = "Modem";
			break;
		case 3:
			str = "Bluetooth LE";
			break;
		case 4:
			str = "Analog Codec";
			break;
		default:
			str = "Reserved";
			break;
		}
	} else
		str = "Reserved";

	return str;
}

static char *nhltdump_direction_to_string(const uint8_t direction)
{
	char *str;

	switch (direction) {
	case 0:
		str = "Render";
		break;
	case 1:
		str = "Capture";
		break;
	default:
		str = "Reserved";
		break;
	}

	return str;
}

static bool nhltdump_ensure_space(
	fwts_framework *fw,
	uint32_t offset,
	uint32_t limit,
	size_t need,
	const char *context)
{
	size_t remaining;

	if (offset > limit)
		remaining = 0;
	else
		remaining = limit - offset;

	if (remaining < need) {
		fwts_log_info_verbatim(fw,
			"    %s truncated: need %zu bytes, only %zu remain.",
			context, need, remaining);
		return false;
	}

	return true;
}

static void nhltdump_dump_config(
	fwts_framework *fw,
	const char *indent,
	uint8_t *table_data,
	uint32_t *offset,
	uint32_t limit)
{
	uint32_t cap_size;
	size_t remaining;

	if (!nhltdump_ensure_space(fw, *offset, limit, sizeof(cap_size),
		"Configuration header")) {
		*offset = limit;
		return;
	}

	memcpy(&cap_size, table_data + *offset, sizeof(cap_size));
	*offset += sizeof(cap_size);

	fwts_log_info_verbatim(fw, "%sCapabilitiesSize: %" PRIu32, indent, cap_size);

	remaining = (size_t)(limit - *offset);
	if (cap_size > remaining) {
		fwts_log_info_verbatim(fw,
			"%sCapabilities truncated to %zu bytes (table end reached).",
			indent, remaining);
		cap_size = remaining;
	}

	fwts_log_info_verbatim(fw, "%sCapabilities:", indent);
	nhltdump_data_hexdump(fw, table_data + *offset, cap_size);
	*offset += cap_size;
}

static void nhltdump_dump_devices_info(
	fwts_framework *fw,
	uint8_t *table_data,
	uint32_t *offset,
	uint32_t limit)
{
	fwts_acpi_table_nhlt_devices_info *devices_info;
	uint8_t count, i;

	if (*offset >= limit)
		return;

	if (!nhltdump_ensure_space(fw, *offset, limit, sizeof(uint8_t),
		"DevicesInfo header")) {
		*offset = limit;
		return;
	}

	devices_info = (fwts_acpi_table_nhlt_devices_info *)(table_data + *offset);
	count = devices_info->devices_count;
	*offset += sizeof(devices_info->devices_count);

	fwts_log_info_verbatim(fw, "    DevicesInfo");
	fwts_log_info_verbatim(fw, "      DevicesCount: %" PRIu8, count);

		for (i = 0; i < count; i++) {
			fwts_acpi_table_nhlt_device_info *device_info;

		if (!nhltdump_ensure_space(fw, *offset, limit,
			sizeof(fwts_acpi_table_nhlt_device_info), "DeviceInfo")) {
			*offset = limit;
			return;
		}

		device_info = (fwts_acpi_table_nhlt_device_info *)(table_data + *offset);

		fwts_log_info_verbatim(fw, "      Device %" PRIu8, (uint8_t)(i + 1));
		fwts_log_info_verbatim(fw, "        ID:");
		nhltdump_data_hexdump(fw, device_info->id, sizeof(device_info->id));
		fwts_log_info_verbatim(fw, "        Instance ID: %" PRIu8, device_info->instance_id);
		fwts_log_info_verbatim(fw, "        Port ID: %" PRIu8, device_info->port_id);

		*offset += sizeof(fwts_acpi_table_nhlt_device_info);
	}
}

/*
 *  NHLT Table
 *   see 5.2.27 Non HDAudio Link Table (NHLT)
 */
static int nhltdump_test1(fwts_framework *fw)
{
	fwts_acpi_table_nhlt *nhlt = (fwts_acpi_table_nhlt *)table->data;
	uint32_t i, j, table_length = table->length;
	fwts_acpi_table_nhlt_ep_descriptor *ep_descriptor = NULL;
	fwts_acpi_table_nhlt_formats_configuration *formats_conf = NULL;
	fwts_acpi_table_nhlt_wave_format_ext *format = NULL;
	char guid_str[37];
	uint8_t *table_data = (uint8_t *)table->data;
	uint32_t offset = sizeof(fwts_acpi_table_nhlt);
	uint32_t ep_end;
	char *str_info;

	fwts_log_info_simp_int(fw, "TableLength: ", table_length);
	if (offset > table_length) {
		fwts_log_info_verbatim(fw, "Table too short, skip dumping remaining data.");
		return FWTS_OK;
	}
	fwts_log_info_verbatim(fw, "NHLT Table:");
	fwts_log_info_verbatim(fw, "  EndpointDescriptorCount: %" PRIu8, nhlt->ep_descriptor_count);

	for (i = 0; i < nhlt->ep_descriptor_count; i++) {
		if (!nhltdump_ensure_space(fw, offset, table_length,
			sizeof(fwts_acpi_table_nhlt_ep_descriptor), "EndpointDescriptor header"))
			return FWTS_OK;

		ep_descriptor = (fwts_acpi_table_nhlt_ep_descriptor *)(table_data + offset);

		if (ep_descriptor->len < sizeof(fwts_acpi_table_nhlt_ep_descriptor)) {
			fwts_log_info_verbatim(fw,
				"EndpointDescriptorLength (%" PRIu32 ") smaller than header size.",
				ep_descriptor->len);
			return FWTS_OK;
		}

		if (ep_descriptor->len > table_length - offset) {
			fwts_log_info_verbatim(fw,
				"EndpointDescriptor %" PRIu32 " exceeds table length, truncating.",
				i + 1);
			ep_end = table_length;
		} else {
			ep_end = offset + ep_descriptor->len;
		}

		fwts_log_info_verbatim(fw, "  EndpointDescriptor %" PRIu32, i + 1);
		fwts_log_info_simp_int(fw, "    EndpointDescriptorLength: ", ep_descriptor->len);
		str_info = nhltdump_linktype_to_string(ep_descriptor->link_type);
		fwts_log_info_verbatim(fw, "    LinkType: %" PRIu8 " (%s)", ep_descriptor->link_type, str_info);
		fwts_log_info_simp_int(fw, "    Instance ID: ", ep_descriptor->instance_id);
		fwts_log_info_simp_int(fw, "    Vendor ID: ", ep_descriptor->vendor_id);
		fwts_log_info_simp_int(fw, "    Device ID: ", ep_descriptor->device_id);
		fwts_log_info_simp_int(fw, "    Revision ID: ", ep_descriptor->revision_id);
		fwts_log_info_simp_int(fw, "    Subsystem ID: ", ep_descriptor->subsystem_id);
		str_info = nhltdump_devtype_to_string(ep_descriptor->link_type, ep_descriptor->device_type);
		fwts_log_info_verbatim(fw, "    Device Type: %" PRIu8 " (%s)", ep_descriptor->device_type, str_info);
		str_info = nhltdump_direction_to_string(ep_descriptor->direction);
		fwts_log_info_verbatim(fw, "    Direction: %" PRIu8 " (%s)", ep_descriptor->direction, str_info);
		fwts_log_info_simp_int(fw, "    Virtual Bus ID: ", ep_descriptor->virtual_bus_id);

		offset += sizeof(fwts_acpi_table_nhlt_ep_descriptor);
		if (offset > ep_end) {
			fwts_log_info_verbatim(fw, "EndpointDescriptor %" PRIu32 " payload truncated.", i + 1);
			return FWTS_OK;
		}

		fwts_log_info_verbatim(fw, "    DeviceConfig");
		nhltdump_dump_config(fw, "      ", table_data, &offset, ep_end);

		fwts_log_info_verbatim(fw, "    FormatsConfig");
		if (!nhltdump_ensure_space(fw, offset, ep_end, sizeof(uint8_t),
			"FormatsConfig header")) {
			offset = ep_end;
		} else {
			formats_conf = (fwts_acpi_table_nhlt_formats_configuration *)(table_data + offset);
			fwts_log_info_verbatim(fw, "      FormatsCount: %" PRIu8, formats_conf->formats_count);
			offset += sizeof(formats_conf->formats_count);
			for (j = 0; j < formats_conf->formats_count; j++) {
				if (!nhltdump_ensure_space(fw, offset, ep_end,
					sizeof(*format), "FormatConfig header")) {
					offset = ep_end;
					break;
				}

				format = (fwts_acpi_table_nhlt_wave_format_ext *)(table_data + offset);

				fwts_log_info_verbatim(fw, "      Format %" PRIu32, j + 1);
				fwts_log_info_simp_int(fw, "        FormatTag: ", format->format_tag);
				fwts_log_info_simp_int(fw, "        Channels: ", format->channels);
				fwts_log_info_simp_int(fw, "        SamplesPerSec: ", format->samples_per_sec);
				fwts_log_info_simp_int(fw, "        AvgBytesPerSec: ", format->avg_bytes_per_sec);
				fwts_log_info_simp_int(fw, "        BlockAlign: ", format->block_align);
				fwts_log_info_simp_int(fw, "        BitsPerSample: ", format->bits_per_sample);
				fwts_log_info_simp_int(fw, "        Size: ", format->size);
				fwts_log_info_simp_int(fw, "        ValidBitsPerSample: ", format->valid_bits_per_sample);
				fwts_log_info_simp_int(fw, "        ChannelMask: ", format->channel_mask);
				fwts_guid_buf_to_str(format->subformat, guid_str, sizeof(guid_str));
				fwts_log_info_verbatim(fw, "        SubFormat:  %s", guid_str);

				offset += sizeof(*format);

				fwts_log_info_verbatim(fw, "        Config");
				nhltdump_dump_config(fw, "          ", table_data, &offset, ep_end);
			}
		}

		if (offset < ep_end)
			nhltdump_dump_devices_info(fw, table_data, &offset, ep_end);

		offset = ep_end;
	}

if (offset < table_length) {
	fwts_log_info_verbatim(fw, "  OEDConfig");
	nhltdump_dump_config(fw, "    ", table_data, &offset, table_length);
}

	return FWTS_OK;
}

static fwts_framework_minor_test nhltdump_tests[] = {
	{ nhltdump_test1, "Dump non-HD Audio endpoints configuration performed via ACPI(NHLT)." },
	{ NULL, NULL }
};

static fwts_framework_ops nhltdump_ops = {
	.description = "Dump configurations performed via NHLT.",
	.init        = NHLT_init,
	.minor_tests = nhltdump_tests
};

FWTS_REGISTER("nhltdump", &nhltdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS)

#endif
