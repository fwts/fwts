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

static int rsdp_init(fwts_framework *fw)
{
	if (fwts_acpi_find_table(fw, "RSDP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI tables.");
		return FWTS_ERROR;
	}
	if (!table) {
		/*
		 * Really old x86 machines may not have an RSDP, but
		 * ia64 and arm64 are both required to be UEFI, so
		 * therefore must have an RSDP.
		 */
		if (fw->target_arch == FWTS_ARCH_X86)
			return FWTS_SKIP;
		else {
			fwts_log_error(fw,
				       "ACPI RSDP is required for the "
				       "%s target architecture.",
				       fwts_arch_get_name(fw->target_arch));
			return FWTS_ERROR;
		}
	}

	/* We know there is an RSDP now, so do a quick sanity check */
	if (table->length == 0) {
		fwts_log_error(fw,
			       "ACPI RSDP table has zero length");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  RSDP Root System Description Pointer
 */
static int rsdp_test1(fwts_framework *fw)
{
	fwts_acpi_table_rsdp *rsdp = (fwts_acpi_table_rsdp *)table->data;
	bool passed = true;
	size_t i;
	int value;
	uint8_t *ptr;
	uint8_t checksum;

	/* verify first checksum */
	checksum = fwts_checksum(table->data, 20);
	if (checksum == 0)
		fwts_passed(fw, "RSDP first checksum is correct");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "RSDPBadFirstChecksum",
			    "RSDP first checksum is incorrect: 0x%x", checksum);

	/* ensure oem_id only contains printable characters */
	for (i = 0; i < 6; i++) {
		if (!isprint(rsdp->oem_id[i])) {
			passed = false;
			break;
		}
	}
	if (passed)
		fwts_passed(fw,
			    "RSDP: oem_id contains only printable "
			    "characters.");
	else {
		fwts_failed(fw, LOG_LEVEL_LOW,
			"RSDPBadOEMId",
			"RSDP: oem_id contains non-printable "
			"characters.");
		fwts_advice(fw,
			"The RSDP OEM Id is non-conforming, but this "
			"will not affect the system behaviour. However "
			"this should be fixed if possible to make the "
			"firmware ACPI complaint.");
	}

	/* check if revision number is valid. note: revision field for
	 * ACPI 1.0 RSDP is 0, not 1, as per ACPI specification version
	 * 6.2, p.122. */
	if (rsdp->revision == 0 || rsdp->revision == 2) {
		if (fw->target_arch == FWTS_ARCH_RISCV64 && rsdp->revision == 0)
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
					"RSDPBadRevisionId",
					"RSDP: revision 0 is too old for RISC-V");
		else
			fwts_passed(fw,
					"RSDP: revision is %" PRIu8 ".", rsdp->revision);
	} else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"RSDPBadRevisionId",
			"RSDP: revision is %" PRIu8 ", expected "
			"value to be 0 or 2.", rsdp->revision);

	/* check if RSDP follows ACPI 1.0 - if so, make sure rsdt_address
	 * is not null. */
	if (rsdp->revision == 0) {
		if (rsdp->rsdt_address != 0)
			fwts_passed(fw,
					"RSDP: RSDP follows ACPI 1.0 "
					"and RsdtAddress is set.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"RSDPNullRSDTPointer",
					"RSDP: RSDP follows ACPI 1.0, but "
					"RsdtAddress isn't set.");

		/* all proceeding tests involve fields which are only
		 * defined in ACPI specifications 2.0 and greater, skip
		 * if ACPI version is 1.0 */
		return FWTS_OK;
	}

	/* whether RSDT or XSDT depends arch */
	if (rsdp->rsdt_address == 0 && rsdp->xsdt_address == 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "RSDPNoAddressesFound",
			    "RSDP: either the RsdtAddress must be non-zero "
			    "or the XsdtAddress must be non-zero.  Both "
			    "fields are zero.");
	else
		fwts_passed(fw,
			    "RSDP: at least one of RsdtAddress or XsdtAddress "
			    "is non-zero.");

	if (rsdp->rsdt_address && rsdp->xsdt_address) {
		fwts_log_warning(fw, "NOTE: The RSDT and XSDT are both "
			"defined. An operating system supporting ACPI "
			"1.0 should use the RSDT, otherwise it will "
			"use the XSDT. RSDT = 0x%" PRIu32 ", "
			"XSDT = 0x%" PRIu64,
			rsdp->rsdt_address, rsdp->xsdt_address);
	}

	passed = false;
	switch (fw->target_arch) {
	case FWTS_ARCH_X86:
		if (rsdp->rsdt_address != 0 || rsdp->xsdt_address != 0)
			passed = true;
		break;

	case FWTS_ARCH_ARM64:
		if (rsdp->xsdt_address != 0)
			passed = true;
		break;

	case FWTS_ARCH_RISCV64:
		passed = true;
		if (fw->flags & FWTS_FLAG_BRSI) {
			if (rsdp->rsdt_address != 0 || rsdp->xsdt_address == 0)
				passed = false;
		}
		break;

	default:
		passed = true;
		fwts_log_advice(fw,
				"No clear rule for RSDT/XSDT address usage "
				"is provided for the %s architecture.",
				fwts_arch_get_name(fw->target_arch));
	}
	if (passed)
		fwts_passed(fw,
			    "RSDP: the correct RSDT/XSDT address is being "
			    "used.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "RSDPBadAddresses",
			    "RSDP: the wrong RSDT/XSDT address is being "
			    "used for the %s architecture.",
			    fwts_arch_get_name(fw->target_arch));

	/* check the length field */
	if (rsdp->length == 36)
		fwts_passed(fw, "RSDP: the table is the correct length.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "RSDPBadLength",
			    "RSDP: length is %d bytes but should be 36.",
			    rsdp->length);

	/* verify the extended checksum */
	checksum = fwts_checksum(table->data, 36);
	if (checksum == 0)
		fwts_passed(fw, "RSDP second checksum is correct");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "RSDPBadSecondChecksum",
			    "RSDP second checksum incorrect: 0x%x", checksum);

	/* the reserved field should be zero */
	value = 0;
	for (ptr = (uint8_t *)rsdp->reserved, i = 0; i < 3; i++)
		value += *ptr++;

	passed = true;
	fwts_acpi_reserved_zero("RSDP", "Reserved", value, &passed);
	if (passed)
		fwts_passed(fw, "RSDP: the reserved field is zero.");

	return FWTS_OK;
}

static fwts_framework_minor_test rsdp_tests[] = {
	{ rsdp_test1, "RSDP Root System Description Pointer test." },
	{ NULL, NULL }
};

static fwts_framework_ops rsdp_ops = {
	.description = "RSDP Root System Description Pointer test.",
	.init        = rsdp_init,
	.minor_tests = rsdp_tests
};

FWTS_REGISTER("rsdp", &rsdp_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH |
	      FWTS_FLAG_ACPI | FWTS_FLAG_COMPLIANCE_ACPI | FWTS_FLAG_BRSI)

#endif
