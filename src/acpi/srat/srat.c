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

static fwts_acpi_table_info *table;
acpi_table_init(SRAT, &table)

static void srat_check_local_apic_sapic_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_table_local_apic_sapic_affinity *affinity =
		(fwts_acpi_table_local_apic_sapic_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_local_apic_sapic_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATLocalApicSapicAffinityShort",
			"SRAT Local APIC/SPAIC Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_local_apic_sapic_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_local_apic_sapic_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT Local APIC/SAPIC Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Proximity Domain:   [7:0] ", affinity->proximity_domain_0);
	fwts_log_info_simp_int(fw, "  APIC ID:                  ", affinity->apic_id);
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Local SAPIC EID:          ", affinity->local_sapic_eid);
	fwts_log_info_simp_int(fw, "  Proximity Domain:  [8:15] ", affinity->proximity_domain_1);
	fwts_log_info_simp_int(fw, "  Proximity Domain: [16:23] ", affinity->proximity_domain_2);
	fwts_log_info_simp_int(fw, "  Proximity Domain: [23:31] ", affinity->proximity_domain_3);
	fwts_log_info_simp_int(fw, "  Clock Domain              ", affinity->clock_domain);
	fwts_log_nl(fw);

	fwts_acpi_reserved_bits("SRAT", "Local APIC/SPAIC Affinity Flags", affinity->flags, 1, 31, passed);

	/*
	 * Not clear of bits 0..7 of Proximity Domain are reserved or not
	 * so skip check for these
	 */

	/*
	 *  Clock domain probably needs deeper sanity checking, for now
	 *  skip this.
	 */
done:
	*length -= sizeof(fwts_acpi_table_local_apic_sapic_affinity);
	*data += sizeof(fwts_acpi_table_local_apic_sapic_affinity);
}

static void srat_check_memory_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_table_memory_affinity *affinity =
		(fwts_acpi_table_memory_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_memory_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATMemoryAffinityShort",
			"SRAT Memory Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_memory_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_memory_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT Memory Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved1);
	fwts_log_info_verbatim(fw, "  Base Address:             0x%8.8" PRIx32 "%8.8" PRIx32,
		affinity->base_addr_hi, affinity->base_addr_lo);
	fwts_log_info_verbatim(fw, "  Length:                   0x%8.8" PRIx32 "%8.8" PRIx32,
		affinity->length_hi, affinity->length_lo);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved2);
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved3);
	fwts_log_nl(fw);

	fwts_acpi_reserved_bits("SRAT", "Memory Affinity Flags", affinity->flags, 4, 31, passed);

done:
	*length -= sizeof(fwts_acpi_table_memory_affinity);
	*data += sizeof(fwts_acpi_table_memory_affinity);
}

static void srat_check_local_x2apic_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_table_local_x2apic_affinity *affinity =
		(fwts_acpi_table_local_x2apic_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_local_x2apic_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATLocalx2apicAffinityShort",
			"SRAT Local x2APIC Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_local_x2apic_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_local_x2apic_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT Local x2APIC Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved1);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);
	fwts_log_info_simp_int(fw, "  X2APIC ID:                ", affinity->x2apic_id);
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Clock Domain              ", affinity->clock_domain);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved2);
	fwts_log_nl(fw);

	/* Spec states 1st reserved field MUST be zero */
	if (affinity->reserved1) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATLocalx2apicReserved1",
			"SRAT Local x2APIC Affinity 1st Reserved field should be zero, got "
			"0x%" PRIx32,
			affinity->reserved1);
		*passed = false;
	}

	fwts_acpi_reserved_bits("SRAT", "Local x2APIC Affinity Flags", affinity->flags, 1, 31, passed);

	/*
	 *  Clock domain probably needs deeper sanity checking, for now
	 *  skip this.
	 */

done:
	*length -= sizeof(fwts_acpi_table_local_x2apic_affinity);
	*data += sizeof(fwts_acpi_table_local_x2apic_affinity);
}

static void srat_check_gicc_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_table_gicc_affinity *affinity =
		(fwts_acpi_table_gicc_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_gicc_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATGICCAffinityShort",
			"SRAT GICC Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_gicc_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_gicc_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT GICC Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);
	fwts_log_info_simp_int(fw, "  ACPI Processor UID:       ", affinity->acpi_processor_uid);
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Clock Domain              ", affinity->clock_domain);
	fwts_log_nl(fw);

	fwts_acpi_reserved_bits("SRAT", "GICC Affinity Flags", affinity->flags, 1, 31, passed);

	/*
	 *  Clock domain probably needs deeper sanity checking, for now
	 *  skip this.
	 */

done:
	*length -= sizeof(fwts_acpi_table_gicc_affinity);
	*data += sizeof(fwts_acpi_table_gicc_affinity);
}

static void srat_check_its_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_table_its_affinity *affinity =
		(fwts_acpi_table_its_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_its_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATITSAffinityShort",
			"SRAT ITS Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_its_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_its_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT ITS Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved);
	fwts_log_info_simp_int(fw, "  ITS ID                    ", affinity->its_id);
	fwts_log_nl(fw);

	fwts_acpi_reserved_zero("SRAT", "ITS Affinity Reserved", affinity->reserved, passed);

done:
	*length -= sizeof(fwts_acpi_table_its_affinity);
	*data += sizeof(fwts_acpi_table_its_affinity);
}


static void srat_check_initiator_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	uint32_t h_reserved = 0;

	fwts_acpi_table_initiator_affinity *affinity =
		(fwts_acpi_table_initiator_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_initiator_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATInitiatorAffinityShort",
			"SRAT Initiator Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_initiator_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_initiator_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT Initiator Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved1);
	fwts_log_info_simp_int(fw, "  Device Handle Type:       ", affinity->device_handle_type);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);

	fwts_acpi_reserved_zero("SRAT", "Initiator Affinity Reserved", affinity->reserved1, passed);
	fwts_acpi_reserved_bits("SRAT", "Initiator Affinity Device Handle Type", affinity->device_handle_type, 1, 7, passed);

	fwts_log_info_verbatim(fw, "  Device Handle:");
	if (affinity->device_handle_type == 0) {
		fwts_log_info_simp_int(fw, "    ACPI _HID:              ", *(uint64_t *)affinity->device_handle);
		fwts_log_info_simp_int(fw, "    ACPI _UID:              ", *(uint32_t *)(affinity->device_handle + 8));
		h_reserved = *(uint32_t *)(affinity->device_handle + 12);
		fwts_log_info_simp_int(fw, "    Reserved:               ", h_reserved);
		fwts_acpi_reserved_zero("SRAT", "Initiator Affinity Device Handle Reserved", h_reserved, passed);
	} else if (affinity->device_handle_type == 1) {
		fwts_log_info_simp_int(fw, "    PCI Segment:            ", *(uint16_t *)affinity->device_handle);
		fwts_log_info_simp_int(fw, "    PCI BDF Number:         ", *(uint16_t *)(affinity->device_handle + 2));
		fwts_log_info_verbatim(fw, "    Reserved:");
		fwts_hexdump_data_prefix_all(fw, affinity->device_handle + 4, "      ", 12);
		fwts_acpi_reserved_zero_array(fw, "SRAT", "  Initiator Affinity Device Handle Reserved", affinity->device_handle + 4, 12, passed);
	}
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved2);

	fwts_acpi_reserved_bits("SRAT", "Initiator Affinity Flags", affinity->flags, 2, 31, passed);
	fwts_acpi_reserved_zero("SRAT", "Initiator Affinity Reserved", affinity->reserved2, passed);

done:
	*length -= sizeof(fwts_acpi_table_initiator_affinity);
	*data += sizeof(fwts_acpi_table_initiator_affinity);
}

static void srat_check_port_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	uint32_t h_reserved = 0;

	fwts_acpi_table_port_affinity *affinity =
		(fwts_acpi_table_port_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_port_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATPortAffinityShort",
			"SRAT Port Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_port_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_port_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT Port Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved1);
	fwts_log_info_simp_int(fw, "  Device Handle Type:       ", affinity->device_handle_type);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);

	fwts_acpi_reserved_zero("SRAT", "Port Affinity Reserved", affinity->reserved1, passed);
	fwts_acpi_reserved_bits("SRAT", "Port Affinity Device Handle Type", affinity->device_handle_type, 1, 7, passed);

	fwts_log_info_verbatim(fw, "  Device Handle:");
	if (affinity->device_handle_type == 0) {
		fwts_log_info_simp_int(fw, "    ACPI _HID:              ", *(uint64_t *)(affinity->device_handle));
		fwts_log_info_simp_int(fw, "    ACPI _UID:              ", *(uint32_t *)(affinity->device_handle + 8));
		h_reserved = *(uint32_t *)(affinity->device_handle + 12);
		fwts_log_info_simp_int(fw, "    Reserved:               ", *(uint32_t *)(affinity->device_handle + 12));
		fwts_acpi_reserved_zero("SRAT", "Port Affinity Device Handle Reserved", h_reserved, passed);
	} else if (affinity->device_handle_type == 1) {
		fwts_log_info_simp_int(fw, "    PCI Segment:            ", *(uint16_t *)(affinity->device_handle));
		fwts_log_info_simp_int(fw, "    PCI BDF Number:         ", *(uint16_t *)(affinity->device_handle + 2));
		fwts_log_info_verbatim(fw, "    Reserved:");
		fwts_hexdump_data_prefix_all(fw, affinity->device_handle + 4, "      ", 12);
		fwts_acpi_reserved_zero_array(fw, "SRAT", "  Port Affinity Device Handle Reserved", affinity->device_handle + 4, 12, passed);
	}
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved2);
	fwts_log_nl(fw);

	fwts_acpi_reserved_bits("SRAT", "Port Affinity Flags", affinity->flags, 2, 31, passed);
	fwts_acpi_reserved_zero("SRAT", "Port Affinity Reserved", affinity->reserved2, passed);

done:
	*length -= sizeof(fwts_acpi_table_port_affinity);
	*data += sizeof(fwts_acpi_table_port_affinity);
}

static void srat_check_rintc_affinity(
	fwts_framework *fw,
	ssize_t		*length,
	uint8_t		**data,
	bool		*passed)
{
	fwts_acpi_table_rintc_affinity *affinity =
		(fwts_acpi_table_rintc_affinity *)*data;

	if ((ssize_t)sizeof(fwts_acpi_table_rintc_affinity) > *length) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"SRATRintcAffinityShort",
			"SRAT RINTC Affinity structure too short, got "
			"%zu bytes, expecting %zu bytes",
			*length, sizeof(fwts_acpi_table_rintc_affinity));
		*passed = false;
		goto done;
	}

	if (!fwts_acpi_structure_length(fw, "SRAT", affinity->type,
	    affinity->length, sizeof(fwts_acpi_table_rintc_affinity))) {
		*passed = false;
		goto done;
	}

	fwts_log_info_verbatim(fw, "SRAT RINTC Affinity Structure:");
	fwts_log_info_simp_int(fw, "  Type:                     ", affinity->type);
	fwts_log_info_simp_int(fw, "  Length:                   ", affinity->length);
	fwts_log_info_simp_int(fw, "  Reserved:                 ", affinity->reserved);
	fwts_log_info_simp_int(fw, "  Proximity Domain:         ", affinity->proximity_domain);
	fwts_log_info_simp_int(fw, "  ACPI Processor UID:       ", affinity->acpi_processor_uid);
	fwts_log_info_simp_int(fw, "  Flags:                    ", affinity->flags);
	fwts_log_info_simp_int(fw, "  Clock Domain              ", affinity->clock_domain);
	fwts_log_nl(fw);

	fwts_acpi_reserved_zero("SRAT", "RINTC Affinity Reserved", affinity->reserved, passed);
	fwts_acpi_reserved_bits("SRAT", "RINTC Affinity Flags", affinity->flags, 1, 31, passed);

done:
	*length -= sizeof(fwts_acpi_table_rintc_affinity);
	*data += sizeof(fwts_acpi_table_rintc_affinity);
}


/*
 *  See ACPI 6.0, Section 5.2.16
 */
static int srat_test1(fwts_framework *fw)
{
	const fwts_acpi_table_srat *srat = (const fwts_acpi_table_srat *)table->data;
	uint8_t *data = (uint8_t *)table->data;
	bool passed = true;
	ssize_t length = (ssize_t)table->length;

	fwts_acpi_fixed_value(fw, LOG_LEVEL_MEDIUM, "SRAT", "Revision1", srat->reserved1, 1, &passed);

	data += sizeof(fwts_acpi_table_srat);
	length -= sizeof(fwts_acpi_table_srat);

	while (length > 0) {
		switch (*data) {
		case 0x00:
			srat_check_local_apic_sapic_affinity(fw, &length, &data, &passed);
			break;
		case 0x01:
			srat_check_memory_affinity(fw, &length, &data, &passed);
			break;
		case 0x02:
			srat_check_local_x2apic_affinity(fw, &length, &data, &passed);
			break;
		case 0x03:
			srat_check_gicc_affinity(fw, &length, &data, &passed);
			break;
		case 0x04:
			srat_check_its_affinity(fw, &length, &data, &passed);
			break;
		case 0x05:
			srat_check_initiator_affinity(fw, &length, &data, &passed);
			break;
		case 0x06:
			srat_check_port_affinity(fw, &length, &data, &passed);
			break;
		case 0x07:
			srat_check_rintc_affinity(fw, &length, &data, &passed);
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"SRATInvalidType",
				"SRAT Affinity Structure Type 0x%" PRIx8
				" is an invalid type, expecting 0x00..0x07",
				*data);
			passed = false;
			length = 0;
			break;
		}
	}

	if (passed)
		fwts_passed(fw, "No issues found in SRAT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test srat_tests[] = {
	{ srat_test1, "SRAT System Resource Affinity Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops srat_ops = {
	.description = "SRAT System Resource Affinity Table test.",
	.init        = SRAT_init,
	.minor_tests = srat_tests
};

FWTS_REGISTER("srat", &srat_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
