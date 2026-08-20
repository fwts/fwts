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

static fwts_acpi_table_info *table;
acpi_table_init(CSRT, &table)

/*
 *  CSRT Core System Resource Table
 *	https://acpica.org/sites/acpica/files/CSRT.doc
 */
static int csrt_test1(fwts_framework *fw)
{
	size_t rg_offset;
	fwts_acpi_table_csrt_resource_group *rg;
	bool passed = true;
	int rg_count = 0;

	/* Note: CSRT header is actually just an ACPI header */
	if (table->length < sizeof(fwts_acpi_table_csrt)) {
		passed = false;
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"CSRTToosShort",
			"CSRT is only %zu bytes long, expecting at least "
			"%zu bytes", table->length,
			sizeof(fwts_acpi_table_csrt));
		goto done;
	}

	rg_offset = sizeof(fwts_acpi_table_csrt);
	rg = (fwts_acpi_table_csrt_resource_group *)
		(table->data + sizeof(fwts_acpi_table_csrt));

	while (rg_offset < table->length) {
		size_t rg_length = table->length - rg_offset;

		/* Sanity check size of what we've got left */
		if (rg_length < sizeof(fwts_acpi_table_csrt_resource_group)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"CSRTResourceGroupTooShort",
				"CSRT Resource Group %d is only %zu bytes long, "
				"expecting at least %zu bytes",
				rg_count, rg_length,
				sizeof(fwts_acpi_table_csrt_resource_group));
			goto done; /* Bad size, bail out */
		}
		/* Sanity check resource group header size */
		if (rg->length < sizeof(fwts_acpi_table_csrt_resource_group)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"CSRTResourceGroupLengthFieldTooShort",
				"CSRT Resource Group %d Length Field is %"
				PRIu32 " and was expecting at least %zu ",
				rg_count, rg->length,
				sizeof(fwts_acpi_table_csrt_resource_group));
			goto done; /* Bad size, bail out */
		}

		/*
		 *  Enough space for a header, so we can now check the
		 *  if we have a shared info following the header and before the
		 *  resource descriptor
		 */
		if (rg->length < (sizeof(fwts_acpi_table_csrt_resource_group) +
				  rg->shared_info_length)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"CSRTResourceGroupLengthFieldTooShort",
				"CSRT Resource Group %d Length Field is %"
				PRIu32 " and was expecting at least %zu ",
				rg_count, rg->length,
				sizeof(fwts_acpi_table_csrt_resource_group) +
				rg->shared_info_length);
			goto done; /* Bad size, bail out */
		}
		/* Reserved must be zero */
		if (rg->reserved != 0) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"CSRTResourceGroupReservedFieldNonZero",
				"CSRT Resource Group %d Reserved Field is 0x%4.4" PRIx16
				" and was expecting a zero value",
				rg_count, rg->reserved);
		}

		/* Do we have enough space for a resource descriptor? */
		if (rg_length > sizeof(fwts_acpi_table_csrt_resource_descriptor)) {
			size_t rd_offset = sizeof(fwts_acpi_table_csrt_resource_group) +
					   rg->shared_info_length;
			int rd_count = 0;
			fwts_acpi_table_csrt_resource_descriptor *rd =
				(fwts_acpi_table_csrt_resource_descriptor *)
				((uint8_t *)rg + sizeof(fwts_acpi_table_csrt_resource_group) +
				 rg->shared_info_length);

			/* Handle 1 or more resource descriptors */
			while (rd_offset < rg->length) {
				/* Advertised length must be sane */
				if (rd->length < sizeof(fwts_acpi_table_csrt_resource_descriptor)) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"CSRTResourceDescriptorLengthFieldTooShort",
						"CSRT Resource Group %d Descriptor %d "
						"Length Field is %" PRIu32 " and was "
						"expecting at least %zu",
						rg_count, rd_count, rd->length,
						sizeof(fwts_acpi_table_csrt_resource_descriptor));
					goto next; /* Bad size, skip to next */
				}
				/* Sanity check type and subtype */
				switch (rd->type) {
				case 0x0000:
				case 0x0800 ... 0xffff:
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"CSRTResourceDescriptorResourceTypeInvalid",
						"CSRT Resource Group %d Descriptor %d "
						"Resource Type is 0x%4.4" PRIx16 " which is a "
						"reserved type",
						rg_count, rd_count, rd->type);
					break;
				case 0x0001: /* Interrupt */
					if (rd->subtype > 1) {
						passed = false;
						fwts_failed(fw, LOG_LEVEL_HIGH,
							"CSRTResourceDescriptorResourceSubTypeInvalid",
							"CSRT Resource Group %d Descriptor %d "
							"Resource Subtype is 0x%4.4" PRIx16 " which is a "
							"reserved type, expecting 0x0000 (Interrupt "
							"Line) or 0x0001 (Interrupt Controller)",
							rg_count, rd_count, rd->subtype);
					}
					break;
				case 0x0002: /* Timer */
					if (rd->subtype > 0) {
						passed = false;
						fwts_failed(fw, LOG_LEVEL_HIGH,
							"CSRTResourceDescriptorResourceSubTypeInvalid",
							"CSRT Resource Group %d Descriptor %d "
							"Resource Subtype is 0x%4.4" PRIx16 " which is a "
							"reserved type, expecting 0x0000 (Timer)",
							rg_count, rd_count, rd->subtype);
					}
					break;
				case 0x0003: /* DMA */
					if (rd->subtype > 1) {
						passed = false;
						fwts_failed(fw, LOG_LEVEL_HIGH,
							"CSRTResourceDescriptorResourceSubTypeInvalid",
							"CSRT Resource Group %d Descriptor %d "
							"Resource Subtype is 0x%4.4" PRIx16 " which is a "
							"reserved type, expecting 0x0000 (DMA channel) "
							"or 0x0001 (DMA controller)",
							rg_count, rd_count, rd->subtype);
					}
					break;
				default:     /* For future use */
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"CSRTResourceDescriptorResourceTypeInvalid",
						"CSRT Resource Group %d Descriptor %d "
						"Resource Type is 0x%4.4" PRIx16 " which is a "
						"reserved type for future use only",
						rg_count, rd_count, rd->type);
					break;
				}
				/* Check for reserved UID */
				if (rd->uid == 0xffffffff) {
					passed = false;
					fwts_failed(fw, LOG_LEVEL_HIGH,
						"CSRTResourceDescriptorReservedUID",
						"CSRT Resource Group %d Descriptor %d "
						"UID Field is 0x%8.8" PRIx32 " and this is "
						"a reserved UID value",
						rg_count, rd_count, rd->uid);
				}
				/* the silicon vendor info is not specified, can't check */

				rd_offset += rd->length;
				rd_count++;
				rd = (fwts_acpi_table_csrt_resource_descriptor *)
					((uint8_t *)rd + rd->length);
			}
		}
next:
		rg_offset += rg->length;
		rg_count++;
		rg = (fwts_acpi_table_csrt_resource_group *)
			((uint8_t*)rg + rg->length);
	}
done:
	if (passed)
		fwts_passed(fw, "No issues found in CSRT table.");

	return FWTS_OK;
}

static fwts_framework_minor_test csrt_tests[] = {
	{ csrt_test1, "CSRT Core System Resource Table test." },
	{ NULL, NULL }
};

static fwts_framework_ops csrt_ops = {
	.description = "CSRT Core System Resource Table test.",
	.init        = CSRT_init,
	.minor_tests = csrt_tests
};

FWTS_REGISTER("csrt", &csrt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
