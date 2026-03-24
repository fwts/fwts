/*
 * Copyright (C) 2016-2026 Canonical
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

static fwts_acpi_table_info *table;

static int asf_init(fwts_framework *fw)
{

	if (fwts_acpi_find_table(fw, "ASF!", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI tables.");
		return FWTS_ERROR;
	}
	if (table == NULL || (table && table->length == 0)) {
		fwts_log_error(fw, "ACPI ASF! table does not exist, skipping test");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

/*
 *  4.1.2.1 ASF_INFO
 */
static void asf_check_info(
	fwts_framework *fw,
	ssize_t length,
	uint8_t *data,
	bool *passed,
	bool *abort)
{
	fwts_acpi_table_asf_info *info = (fwts_acpi_table_asf_info *)data;

	if (length < (ssize_t)sizeof(fwts_acpi_table_asf_info)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!InfoRecordTooShort",
			"ASF! ASF_INFO Record too short, "
			"expecting %zu bytes, instead got %zu bytes",
			sizeof(fwts_acpi_table_asf_info), length);
		*passed = false;
		*abort = true;
		return;
	}

	fwts_log_info_verbatim(fw, "ASF! ASF_INFO Record:");
	fwts_log_info_verbatim(fw, "  Min Watchdog Reset Value: 0x%2.2" PRIx8, info->watchdog_reset_value);
	fwts_log_info_verbatim(fw, "  Min Poll Wait Time:       0x%2.2" PRIx8, info->min_sensor_poll_wait_time);
	fwts_log_info_verbatim(fw, "  System ID:                0x%4.4" PRIx16, info->id);
	fwts_log_info_verbatim(fw, "  IANA Manufacturer ID:     0x%8.8" PRIx32, info->iana_id);
	fwts_log_info_verbatim(fw, "  Feature Flags:            0x%2.2" PRIx8, info->flags);
	fwts_log_info_verbatim(fw, "  Reserved:                 0x%2.2" PRIx8, info->reserved1);
	fwts_log_info_verbatim(fw, "  Reserved:                 0x%2.2" PRIx8, info->reserved2);
	fwts_log_info_verbatim(fw, "  Reserved:                 0x%2.2" PRIx8, info->reserved3);

	if (info->min_sensor_poll_wait_time < 2) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!InfoMinPollWaitTimeInvalid",
			"ASF! ASF_INFO Minimum Poll Wait Time is 0x%" PRIx8
			" and must be in the range 0x02..0xff",
			info->min_sensor_poll_wait_time);
	}

	fwts_acpi_reserved_bits("ASF!", "ASF_INFO Feature Flags", info->flags, 1, 7, passed);
	fwts_acpi_reserved_zero("ASF!", "ASF_INFO Reserved1", info->reserved1, passed);
	fwts_acpi_reserved_zero("ASF!", "ASF_INFO Reserved2", info->reserved2, passed);
	fwts_acpi_reserved_zero("ASF!", "ASF_INFO Reserved3", info->reserved3, passed);

	if (*passed)
		fwts_passed(fw, "No issues found in ASF! ASF_INFO record.");
}


/*
 *  4.1.2.2 ASF_ALRT
 */
static void asf_check_alrt(
	fwts_framework *fw,
	ssize_t record_length,
	ssize_t length,
	uint8_t *data,
	bool *passed,
	bool *abort)
{
	fwts_acpi_table_asf_alrt *alrt = (fwts_acpi_table_asf_alrt *)data;
	uint8_t i;
	ssize_t total_length;

	if (length < (ssize_t)sizeof(fwts_acpi_table_asf_alrt)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!AlrtRecordTooShort",
			"ASF! ASF_ALRT Record too short, "
			"expecting %zu bytes, instead got %zu bytes",
			sizeof(fwts_acpi_table_asf_alrt), length);
		*passed = false;
		*abort = true;
		return;
	}

	fwts_log_info_verbatim(fw, "ASF! ASF_ALRT Record:");
	fwts_log_info_verbatim(fw, "  Assertion Event Mask:     0x%2.2" PRIx8, alrt->assertion_mask);
	fwts_log_info_verbatim(fw, "  De-Assertion Event Mask:  0x%2.2" PRIx8, alrt->deassertion_mask);
	fwts_log_info_verbatim(fw, "  Number of Alerts:         0x%2.2" PRIx8, alrt->number_of_alerts);
	fwts_log_info_verbatim(fw, "  Array Element Length:     0x%2.2" PRIx8, alrt->array_length);

	if ((alrt->number_of_alerts < 1) ||
	    (alrt->number_of_alerts > 8)) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!AlrtNumOfAlertsInvalid",
			"ASF! ASF_ALRT Number of Alerts field is 0x%" PRIx8
			" and must be in the range 0x01..0x08",
			alrt->number_of_alerts);
		/* Don't trust the ALRT data, so abort */
		return;
	}
	if (alrt->array_length != sizeof(fwts_acpi_table_asf_alrt_element)) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!AlrtArrayElementLengthInvalid",
			"ASF! ASF_ALRT Array Element Length is 0x%" PRIx8
			" and must be instead 0x%" PRIx8,
			alrt->array_length,
			(uint8_t)sizeof(fwts_acpi_table_asf_alrt_element));
	}

	total_length = sizeof(fwts_acpi_table_asf_alrt) +
		(alrt->number_of_alerts * sizeof(fwts_acpi_table_asf_alrt_element));
	if (total_length > record_length) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!AlrtArrayElementLengthInvalid",
			"ASF! ASF_ALRT Array Element Length makes the "
			"total ASF_ALRT record size to be %zu bytes, however the "
			"table is only %zu bytes long",
			total_length, record_length);
		*passed = false;
		*abort = true;
		return;
	}

	data += sizeof(fwts_acpi_table_asf_alrt);
	for (i = 0; i < alrt->number_of_alerts; i++) {
		fwts_acpi_table_asf_alrt_element *element =
			(fwts_acpi_table_asf_alrt_element *)data;

		fwts_log_info_verbatim(fw, "ASF! ASF_ALRT Element %" PRIu8 ":", i);
		fwts_log_info_verbatim(fw, "  Device Address:           0x%2.2" PRIx8, element->device_addr);
		fwts_log_info_verbatim(fw, "  Alert Command:            0x%2.2" PRIx8, element->command);
		fwts_log_info_verbatim(fw, "  Alert Data Mask:          0x%2.2" PRIx8, element->data_mask);
		fwts_log_info_verbatim(fw, "  Alert Compare Value:      0x%2.2" PRIx8, element->compare_value);
		fwts_log_info_verbatim(fw, "  Alert Event Sensor Type:  0x%2.2" PRIx8, element->sensor_type);
		fwts_log_info_verbatim(fw, "  Alert Event Type:         0x%2.2" PRIx8, element->event_type);
		fwts_log_info_verbatim(fw, "  Alert Event Offset:       0x%2.2" PRIx8, element->event_offset);
		fwts_log_info_verbatim(fw, "  Alert Source Type:        0x%2.2" PRIx8, element->event_source_type);
		fwts_log_info_verbatim(fw, "  Alert Event Severity:     0x%2.2" PRIx8, element->event_severity);
		fwts_log_info_verbatim(fw, "  Alert Sensor Number:      0x%2.2" PRIx8, element->sensor_number);
		fwts_log_info_verbatim(fw, "  Alert Entity:             0x%2.2" PRIx8, element->entity);
		fwts_log_info_verbatim(fw, "  Alert Entity Instance:    0x%2.2" PRIx8, element->entity_instance);

		if (element->event_offset & 0x80) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASF!AlrtEventOffsetBit7Set",
				"ASF! ASF_ALRT Array Element %" PRIu8 " Event Offset Bit 7 is 1, "
				" and should be 0", i);
		}
		data += sizeof(fwts_acpi_table_asf_alrt_element);
	}
	if (*passed)
		fwts_passed(fw, "No issues found in ASF! ASF_ALRT record.");
}

/*
 *  4.1.2.4 ASF_RCTL
 */
static void asf_check_rctl(
	fwts_framework *fw,
	ssize_t record_length,
	ssize_t length,
	uint8_t *data,
	bool *passed,
	bool *abort)
{
	fwts_acpi_table_asf_rctl *rctl = (fwts_acpi_table_asf_rctl *)data;
	uint8_t i;
	ssize_t total_length;

	if (length < (ssize_t)sizeof(fwts_acpi_table_asf_rctl)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!RctlRecordTooShort",
			"ASF! ASF_RCTL Record too short, "
			"expecting %zu bytes, instead got %zu bytes",
			sizeof(fwts_acpi_table_asf_rctl), length);
		*passed = false;
		*abort = true;
		return;
	}
	fwts_log_info_verbatim(fw, "ASF! ASF_RCTL Record:");
	fwts_log_info_verbatim(fw, "  Number of Controls:       0x%2.2" PRIx8, rctl->number_of_controls);
	fwts_log_info_verbatim(fw, "  Array Element Length:     0x%2.2" PRIx8, rctl->array_element_length);
	fwts_log_info_verbatim(fw, "  Reserved:                 0x%4.4" PRIx16, rctl->array_element_length);

	if (rctl->array_element_length != sizeof(fwts_acpi_table_asf_rctl_element)) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!RctlArrayElementLengthInvalid",
			"ASF! ASF_RCTL Array Element Length is 0x%" PRIx8
			" and must be instead 0x%" PRIx8,
			rctl->array_element_length,
			(uint8_t)sizeof(fwts_acpi_table_asf_rctl_element));
	}

	total_length = sizeof(fwts_acpi_table_asf_rctl) +
		(rctl->number_of_controls * sizeof(fwts_acpi_table_asf_rctl_element));
	if (total_length > record_length) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!RctlArrayElementLengthInvalid",
			"ASF! ASF_RCTL Array Element Length makes the "
			"total ASF_RCTL record size to be %zu bytes, however the "
			"table is only %zu bytes long",
			total_length, record_length);
		*passed = false;
		*abort = true;
		return;
	}

	data += sizeof(fwts_acpi_table_asf_rctl);
	for (i = 0; i < rctl->number_of_controls; i++) {
		fwts_acpi_table_asf_rctl_element *element =
			(fwts_acpi_table_asf_rctl_element *)data;

		fwts_log_info_verbatim(fw, "ASF! ASF_RCTL Element %" PRIu8 ":", i);
		fwts_log_info_verbatim(fw, "  Control Function:         0x%2.2" PRIx8, element->control_function);
		fwts_log_info_verbatim(fw, "  Control Device Address:   0x%2.2" PRIx8, element->control_device_addr);
		fwts_log_info_verbatim(fw, "  Control Command:          0x%2.2" PRIx8, element->control_command);
		fwts_log_info_verbatim(fw, "  Control Value:            0x%2.2" PRIx8, element->control_value);

		if (element->control_function > 0x03) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASF!RctlCtrlFuncInvalid",
				"ASF! ASF_RCTL Control Function is 0x%" PRIx8
				" and must be in the range 0x00..0x03",
				element->control_function);
		}
		data += sizeof(fwts_acpi_table_asf_rctl_element);
	}
	if (*passed)
		fwts_passed(fw, "No issues found in ASF! ASF_RCTL record.");
}

/*
 *  4.1.2.6 ASF_RMCP
 */
static void asf_check_rmcp(
	fwts_framework *fw,
	ssize_t length,
	uint8_t *data,
	bool *passed,
	bool *abort)
{
	fwts_acpi_table_asf_rmcp *rmcp = (fwts_acpi_table_asf_rmcp *)data;

	if (length < (ssize_t)sizeof(fwts_acpi_table_asf_rmcp)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!RmcpRecordTooShort",
			"ASF! ASF_RMCP Record too short, "
			"expecting %zu bytes, instead got %zu bytes",
			sizeof(fwts_acpi_table_asf_rmcp), length);
		*passed = false;
		*abort = true;
		return;
	}

	fwts_log_info_verbatim(fw, "ASF! ASF_RMCP Record:");
	fwts_log_info_verbatim(fw, "  Remote Control Cap.:      "
		"0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8 " "
		"0x%2.2" PRIx8 " 0x%2.2" PRIx8 " 0x%2.2" PRIx8,
		rmcp->remote_control_capabilities[0],
		rmcp->remote_control_capabilities[1],
		rmcp->remote_control_capabilities[2],
		rmcp->remote_control_capabilities[3],
		rmcp->remote_control_capabilities[4],
		rmcp->remote_control_capabilities[5],
		rmcp->remote_control_capabilities[6]);
	fwts_log_info_verbatim(fw, "  Boot Opt. Completion Code:0x%2.2" PRIx8, rmcp->completion_code);
	fwts_log_info_verbatim(fw, "  IANA Enterprise ID:       0x%8.8" PRIx32, rmcp->iana);
	fwts_log_info_verbatim(fw, "  Special Command:          0x%2.2" PRIx8, rmcp->special_command);
	fwts_log_info_verbatim(fw, "  Special Command Parameter:0x%4.4" PRIx16, rmcp->special_command_param);
	fwts_log_info_verbatim(fw, "  Boot Options:             0x%2.2" PRIx8 " 0x%2.2" PRIx8,
		rmcp->boot_options[0], rmcp->boot_options[1]);
	fwts_log_info_verbatim(fw, "  OEM Parameters:           0x%4.4" PRIx16, rmcp->oem_parameters);

	/* Specification, page 33-34 */
	if (rmcp->iana == 0x4542) {
		/* Values 0x00..0x05 and 0xc0..0xff are allowed */
		if ((rmcp->special_command > 0x05) &&
		    (rmcp->special_command < 0xc0)) {
			*passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASF!RmcpSpecialCommandInvalid",
				"ASF! ASF_RMCP Special Command is 0x%" PRIx8
				"and should be 0x00..0x05 or 0xc0..0xff",
				rmcp->special_command);
		}
	}
	if (*passed)
		fwts_passed(fw, "No issues found in ASF! ASF_RMCP record.");
}

/*
 *  4.1.2.7 ASF_ADDR
 */
static void asf_check_addr(
	fwts_framework *fw,
	ssize_t record_length,
	ssize_t length,
	uint8_t *data,
	bool *passed,
	bool *abort)
{
	ssize_t total_length;
	fwts_acpi_table_asf_addr *addr = (fwts_acpi_table_asf_addr *)data;
	uint8_t i;
	if (length < (ssize_t)sizeof(fwts_acpi_table_asf_addr)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!AddrRecordTooShort",
			"ASF! ASF_ADDR Record too short, "
			"expecting %zu bytes, instead got %zu bytes",
			sizeof(fwts_acpi_table_asf_addr), length);
		*passed = false;
		*abort = true;
		return;
	}

	fwts_log_info_verbatim(fw, "ASF! ASF_ADDR Record:");
	fwts_log_info_verbatim(fw, "  SEEPROM Address:          0x%2.2" PRIx8, addr->seeprom_addr);
	fwts_log_info_verbatim(fw, "  Number of Devices:        0x%2.2" PRIx8, addr->number_of_devices);

	total_length = sizeof(fwts_acpi_table_asf_addr) +
		(addr->number_of_devices * sizeof(fwts_acpi_table_asf_addr_element));
	if (total_length > record_length) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"ASF!AddrArrayElementLengthInvalid",
			"ASF! ASF_ADDR Number of Devices makes the "
			"total ASF_ADDR record size to be %zu bytes, however the "
			"table is only %zu bytes long",
			total_length, record_length);
		*passed = false;
		*abort = true;
		return;
	}

	data += sizeof(fwts_acpi_table_asf_addr);
	for (i = 0; i < addr->number_of_devices; i++) {
		fwts_acpi_table_asf_addr_element *element =
			(fwts_acpi_table_asf_addr_element *)data;
		fwts_log_info_verbatim(fw, "  Fixed SMBus Address       0x%2.2" PRIx8, element->fixed_smbus_addr);
		data += sizeof(fwts_acpi_table_asf_addr_element);
	}

	if (*passed)
		fwts_passed(fw, "No issues found in ASF! ASF_ADDR record.");
}

/*
 *  ASF! Hardware Error Source Table test
 *     http://www.dmtf.org/sites/default/files/standards/documents/DSP0136.pdf
 */
static int asf_test1(fwts_framework *fw)
{
	bool passed = true;
	bool abort = false;
	fwts_acpi_table_header *hdr = (fwts_acpi_table_header *)table->data;
        uint8_t *data = (uint8_t *)table->data;
        ssize_t length = (ssize_t)hdr->length;

	fwts_log_info_verbatim(fw, "ASF! Hardware Error Source Table");

        data += sizeof(fwts_acpi_table_header);
        length -= sizeof(fwts_acpi_table_header);

	while (!abort && (length > 0)) {
		bool asf_passed = true;

		fwts_acpi_table_asf_header *asf_hdr =
			(fwts_acpi_table_asf_header *)data;

		/* Must have enough for a info header */
		if (length < (ssize_t)sizeof(fwts_acpi_table_asf_header)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASF!TooShort",
				"ASF! table too short, expecting at least %zu bytes "
				"for an ASF! information record header, "
				"instead got %zu bytes",
				sizeof(fwts_acpi_table_asf_header), table->length);
			break;
		}

		fwts_log_info_verbatim(fw, "Type:                       0x%2.2" PRIx8, asf_hdr->type);
		fwts_log_info_verbatim(fw, "Reserved:                   0x%2.2" PRIx8, asf_hdr->reserved);
		fwts_log_info_verbatim(fw, "Length:                     0x%4.4" PRIx16, asf_hdr->length);

		fwts_acpi_reserved_zero("ASF!", "Information Record Reserved", asf_hdr->reserved, &passed);

		if (asf_hdr->length > (uint32_t)length) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASF!InfoRecordLengthTooLong",
				"ASF! Information Record Reserved length is %" PRIu32
				" and this is too long for the size given by "
				"the ASF! table. Expected at most %zu bytes.",
				asf_hdr->length, length);
			/* Since we can't trust the table, abort */
			break;
		}

		switch (asf_hdr->type) {
		case 0x00:
		case 0x80:
			asf_check_info(fw, length - sizeof(*asf_hdr), data + sizeof(*asf_hdr), &asf_passed, &abort);
			data += asf_hdr->length;
			length -= asf_hdr->length;
			break;
		case 0x01:
		case 0x81:
			asf_check_alrt(fw, asf_hdr->length,
				length - sizeof(*asf_hdr), data + sizeof(*asf_hdr), &asf_passed, &abort);
			data += asf_hdr->length;
			length -= asf_hdr->length;
			break;
		case 0x02:
		case 0x82:
			asf_check_rctl(fw, asf_hdr->length,
				length - sizeof(*asf_hdr), data + sizeof(*asf_hdr), &asf_passed, &abort);
			data += asf_hdr->length;
			length -= asf_hdr->length;
			break;
		case 0x03:
		case 0x83:
			asf_check_rmcp(fw, length - sizeof(*asf_hdr), data + sizeof(*asf_hdr), &asf_passed, &abort);
			data += asf_hdr->length;
			length -= asf_hdr->length;
			break;
		case 0x04:
		case 0x84:
			asf_check_addr(fw, asf_hdr->length,
				length - sizeof(*asf_hdr), data + sizeof(*asf_hdr), &asf_passed, &abort);
			data += asf_hdr->length;
			length -= asf_hdr->length;
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"ASF!InvalidType",
				"ASF! Information Record Type 0x%4.4" PRIx16 " is invalid, aborting check",
				asf_hdr->type);
			passed = false;
			length = 0;
			break;
		}
		passed &= asf_passed;
		fwts_log_nl(fw);
	}
	fwts_log_nl(fw);

	if (passed)
		fwts_passed(fw, "No issues found in ASF! table.");

	return FWTS_OK;
}

static fwts_framework_minor_test asf_tests[] = {
	{ asf_test1, "ASF! Alert Standard Format Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops asf_ops = {
	.description = "ASF! Alert Standard Format Table test.",
	.init        = asf_init,
	.minor_tests = asf_tests
};

FWTS_REGISTER("asf", &asf_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
