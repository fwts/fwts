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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

static fwts_acpi_table_info *table;
acpi_table_init(DRTM, &table)

/*
 *  DRTM D-RTM Resources Table
 */
static int drtm_test1(fwts_framework *fw)
{
	fwts_acpi_table_drtm *drtm = (fwts_acpi_table_drtm*) table->data;
	fwts_acpi_table_drtm_vtl *drtm_vtl;
	fwts_acpi_table_drtm_rtl *drtm_rtl;
	fwts_acpi_table_drtm_dps *drtm_dps;
	bool passed = true;
	uint32_t offset;
	uint32_t i;

	fwts_log_info_verbatim(fw, "DRTM D-RTM Resources Table:");
	fwts_log_info_simp_int(fw, "  DL_Entry_Base:            ", drtm->entry_base_address);
	fwts_log_info_simp_int(fw, "  DL_Entry_Length:          ", drtm->entry_length);
	fwts_log_info_simp_int(fw, "  DL_Entry32:               ", drtm->entry_address32);
	fwts_log_info_simp_int(fw, "  DL_Entry64:               ", drtm->entry_address64);
	fwts_log_info_simp_int(fw, "  DLME_Exit:                ", drtm->exit_address);
	fwts_log_info_simp_int(fw, "  Log_Area_Start:           ", drtm->log_area_address);
	fwts_log_info_simp_int(fw, "  Log_Area_Length:          ", drtm->log_area_length);
	fwts_log_info_simp_int(fw, "  Architecture_Dependent:   ", drtm->arch_dependent_address);
	fwts_log_info_simp_int(fw, "  DRT_Flags:                ", drtm->flags);

	fwts_acpi_reserved_bits("DRTM", "DRT_Flags", drtm->flags, 4, 31, &passed);
	fwts_log_nl(fw);

	offset = sizeof(fwts_acpi_table_drtm);
	drtm_vtl = (fwts_acpi_table_drtm_vtl *) (table->data + offset);
	fwts_log_info_simp_int(fw, "  VTL_Length:               ", drtm_vtl->validated_table_count);
	offset += sizeof(drtm_vtl->validated_table_count);

	if (drtm->header.length < offset + sizeof(uint64_t) * drtm_vtl->validated_table_count) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"DRTMOutOfBound",
			"DRTM's length is too small to contain all fields");
		goto error;
	}

	for (i = 0; i < drtm_vtl->validated_table_count; i++) {
		fwts_log_info_simp_int(fw, "  Validated_Tables:         ", drtm_vtl->validated_tables[i]);
		offset += sizeof(drtm_vtl->validated_tables[i]);
	}

	fwts_log_nl(fw);

	drtm_rtl = (fwts_acpi_table_drtm_rtl *) (table->data + offset);
	fwts_log_info_simp_int(fw, "  RL_Length:                ", drtm_rtl->resource_count);
	offset += sizeof(drtm_rtl->resource_count);

	if (drtm->header.length < offset + sizeof(fwts_acpi_drtm_resource) * drtm_rtl->resource_count) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"DRTMOutOfBound",
			"DRTM's length is too small to contain all fields");
		goto error;
	}

	for (i = 0; i < drtm_rtl->resource_count; i++) {
		fwts_acpi_drtm_resource *resource = (fwts_acpi_drtm_resource *) (table->data + offset);
		uint64_t size;
		size = resource->size[0] + ((uint64_t) resource->size[1] << 8) +
					   ((uint64_t) resource->size[2] << 16) + ((uint64_t) resource->size[3] << 24) +
					   ((uint64_t) resource->size[4] << 32) + ((uint64_t) resource->size[5] << 40) +
					   ((uint64_t) resource->size[6] << 48);

		fwts_log_info_simp_int(fw, "  Resource Size:            ", size);
		fwts_log_info_simp_int(fw, "  Resource Type:            ", resource->type);
		fwts_log_info_simp_int(fw, "  Resource Address:         ", resource->address);

		if (resource->type & 0x7C) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"DRTMBadResourceType",
				"DRTM Resource Type Bits [6:2] are reserved, got 0x%2.2" PRIx8
				" instead",	resource->type);
		}

		offset += sizeof(fwts_acpi_drtm_resource);
		fwts_log_nl(fw);
	}

	drtm_dps = (fwts_acpi_table_drtm_dps *) (table->data + offset);
	fwts_log_info_simp_int(fw, "  DPS_Length:               ", drtm_dps->dps_id_length);

	if (drtm->header.length < offset + sizeof(fwts_acpi_table_drtm_dps)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"DRTMOutOfBound",
			"DRTM's length is too small to contain all fields");
		goto error;
	}

	for (i = 0; i < sizeof(drtm_dps->dps_id); i++) {
		fwts_log_info_simp_int(fw, "  DLME Platform Id:         ", drtm_dps->dps_id[i]);
	}

	fwts_log_nl(fw);

	if (passed)
		fwts_passed(fw, "No issues found in DRTM table.");

 error:
	return FWTS_OK;
}

static fwts_framework_minor_test drtm_tests[] = {
	{ drtm_test1, "DRTM D-RTM Resources Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops drtm_ops = {
	.description = "DRTM D-RTM Resources Table test.",
	.init        = DRTM_init,
	.minor_tests = drtm_tests
};

FWTS_REGISTER("drtm", &drtm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
