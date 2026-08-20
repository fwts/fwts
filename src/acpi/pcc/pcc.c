/*
 * Copyright (C) 2010-2026 Canonical
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

#ifdef FWTS_ARCH_INTEL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "fwts_acpi_object_eval.h"

/*
 *  This test does some sanity checking on the PCC interface,
 *  see http://acpica.org/download/Processor-Clocking-Control-v1p0.pdf
 */

#define PCC_HDR_SIGNATURE	0x24504343	/* $PCC */

/*
 * For the moment, we turn this off as I am concerned that reads of this region
 * may cause issues.
 */
#define CHECK_PCC_HDR		0

typedef struct {
	uint8_t  descriptor;
	uint8_t  length;
	uint8_t  space_id;
	uint8_t  resource_usage;
	uint8_t  type_specific;
	uint64_t granularity;
	uint64_t minimum;
	uint64_t maximum;
	uint64_t translation_offset;
	uint64_t address_length;
} __attribute__ ((packed)) fwts_pcc_memory_resource;

typedef struct  {
	uint8_t  descriptor;
	uint16_t length;
	uint8_t  space_id;
	uint8_t  bit_width;
	uint8_t  bit_offset;
	uint8_t  access_size;
	uint64_t address;
} __attribute__ ((packed)) fwts_pcc_register_resource;

typedef struct {
	uint32_t signature;
	uint16_t length;
	uint8_t  major;
	uint8_t  minor;
	uint32_t features;
	uint16_t command;
	uint16_t status;
	uint32_t latency;
	uint32_t minimum_time;
	uint32_t maximum_time;
	uint32_t nominal;
	uint32_t throttled_frequency;
	uint32_t minimum_frequency;
} __attribute__ ((packed)) fwts_pcc_header;

/*
 *  pcc_init()
 *	initialize ACPI
 */
static int pcc_init(fwts_framework *fw)
{
	if (fwts_acpi_init(fw) != FWTS_OK)
		return FWTS_ERROR;

	return FWTS_OK;
}

/*
 *  pcc_deinit
 *	de-intialize ACPI
 */
static int pcc_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

#if CHECK_PCC_HDR
static void pcc_check_pcc_header(
	fwts_framework *fw,
	uint64_t addr,
	uint64_t length,
	bool *failed)
{
	fwts_pcc_header *hdr;

	hdr = (fwts_pcc_header *)fwts_mmap((off_t)addr, (size_t)length);
	if (hdr == FWTS_MAP_FAILED) {
		fwts_log_info(fw, "Failed to memory map PCC header 0x%" PRIx64
			"..0x%" PRIx64 ".", addr, addr + length);
		return;
	}
	/* Ensure header can be read */
	if (fwts_safe_memread(hdr, length) != FWTS_OK) {
		fwts_log_info(fw, "Cannot read PCC header 0x%" PRIx64
			 "..0x%" PRIx64 ".", addr, addr + length);
		fwts_munmap(hdr, (size_t)length);
		return;
	}

	fwts_log_info_simp_int(fw, "PCC header at ", addr);
	fwts_log_info_simp_int(fw, "  Signature:          ", hdr->signature);
	fwts_log_info_simp_int(fw, "  Length:             ", hdr->length);
	fwts_log_info_simp_int(fw, "  Major:              ", hdr->major);
	fwts_log_info_simp_int(fw, "  Minor:              ", hdr->minor);
	fwts_log_info_simp_int(fw, "  Features:           ", hdr->features);
	fwts_log_info_simp_int(fw, "  Command:            ", hdr->command);
	fwts_log_info_simp_int(fw, "  Status:             ", hdr->status);
	fwts_log_info_simp_int(fw, "  Latency:            ", hdr->latency);
	fwts_log_info_simp_int(fw, "  Minimum Time:       ", hdr->minimum_time);
	fwts_log_info_simp_int(fw, "  Maximum Time:       ", hdr->maximum_time);
	fwts_log_info_simp_int(fw, "  Nominal:            ", hdr->nominal);
	fwts_log_info_simp_int(fw, "  Throttled Freq.:    ", hdr->throttled_frequency);
	fwts_log_info_simp_int(fw, "  Minimum Freq.:      ", hdr->minimum_frequency);

	fwts_munmap(hdr, (size_t)length);
	fwts_log_nl(fw);

	if (hdr->signature != PCC_HDR_SIGNATURE) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHdrSignatureError",
			"The PCC Header Signature is not a valid PCC signature, was expecting "
			"0x%" PRIx32 " ($PCC), got instead 0x%" PRIx32,
			PCC_HDR_SIGNATURE, hdr->signature);
		*failed = true;
	}
}
#endif

static void pcc_check_shared_memory_region(
	fwts_framework *fw,
	const char *name,
	ACPI_OBJECT *pcc_obj,
	bool *failed)
{
	fwts_pcc_memory_resource *pcc_mr;

	if (pcc_obj->Type != ACPI_TYPE_BUFFER) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHElementZeroNotBuffer",
			"PCCH object %s returned a package with element zero "
			"was not an ACPI_BUFFER. This does not conform to the "
			"PCC specification.", name);
		*failed = true;
		return;
	}

	if (pcc_obj->Buffer.Pointer == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHElementZeroBufferNull",
			"PCCH object %s returned a package with element zero "
			"which was an ACPI_BUFFER, however, the buffer pointer "
			"was NULL. This does not conform to the PCC "
			"specification.", name);
		*failed = true;
		return;
	}

	if (pcc_obj->Buffer.Length < sizeof(fwts_pcc_memory_resource)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHMemoryResourceIllegalSize",
			"PCCH object %s returned a PCC memory resource buffer "
			"which was the wrong size. Got %" PRIu32 " bytes, "
			"expected %zu bytes.", name,
			pcc_obj->Buffer.Length, sizeof(fwts_pcc_memory_resource));
		*failed = true;
		return;
	}

	pcc_mr = (fwts_pcc_memory_resource *)pcc_obj->Buffer.Pointer;

	fwts_log_info_verbatim(fw, "PCC Memory Resource (Shared Memory Region) for %s:", name);
	fwts_log_info_simp_int(fw, "  Descriptor:         ", pcc_mr->descriptor);
	fwts_log_info_simp_int(fw, "  Length:             ", pcc_mr->length);
	fwts_log_info_simp_int(fw, "  Space ID:           ", pcc_mr->space_id);
	fwts_log_info_simp_int(fw, "  Resource Usage:     ", pcc_mr->resource_usage);
	fwts_log_info_simp_int(fw, "  Type Specific:      ", pcc_mr->type_specific);
	fwts_log_info_simp_int(fw, "  Minimum:            ", pcc_mr->minimum);
	fwts_log_info_simp_int(fw, "  Maximum:            ", pcc_mr->maximum);
	fwts_log_info_simp_int(fw, "  Translation Offset: ", pcc_mr->translation_offset);
	fwts_log_info_simp_int(fw, "  Address Length:     ", pcc_mr->address_length);

	if (pcc_mr->space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCMemoryResourceSpaceIdWrongType",
			"PCC Memory Resource Space ID is of the wrong type, got 0x%" PRIx8
			", expected to get type 0x%" PRIx8 " (ACPI_ADR_SPACE_SYSTEM_MEMORY).",
			pcc_mr->space_id, ACPI_ADR_SPACE_SYSTEM_MEMORY);
		*failed = true;
	}

	if (pcc_mr->length == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCMemoryResourceAddrLength",
			"PCC Memory Resource Address Length is zero, this is clearly incorrect.");
		*failed = true;
	}

	if (pcc_mr->minimum & 4095) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCMemoryNotPageAligned",
			"PCC Memory Resource Minimum Address is not page aligned. It must "
			"start on a 4K page boundary.");
		*failed = true;
	}

	/* TODO: We should also check if the region is in the e820 region too */

	if (pcc_mr->minimum == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCMemoryResourceMinAddr",
			"PCC Memory Resource Minimum Address is zero, this is clearly incorrect.");
		*failed = true;
	}

	if (pcc_mr->maximum == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCMemoryResourceMaxAddr",
			"PCC Memory Resource Maximum Address is zero, this is clearly incorrect.");
		*failed = true;
	}

	if (pcc_mr->minimum >= pcc_mr->maximum) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCMemoryResourceMinMaxAddrError",
			"PCC Memory Resource Minimum Address should be less than "
			"the Maximum Address: Min: 0x%" PRIx64 ", Max: 0x%" PRIx64,
			pcc_mr->minimum, pcc_mr->maximum);
		*failed = true;
	}

	fwts_log_nl(fw);

#if CHECK_PCC_HDR
	pcc_check_pcc_header(fw, pcc_mr->minimum, pcc_mr->length, failed);
#endif
}

static void pcc_check_doorbell_address(
	fwts_framework *fw,
	const char *name,
	ACPI_OBJECT *pcc_obj,
	bool *failed)
{
	fwts_pcc_register_resource *pcc_rr;

	if (pcc_obj->Type != ACPI_TYPE_BUFFER) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHElementOneNotBuffer",
			"PCCH object %s returned a package with element zero "
			"was not an ACPI_BUFFER. This does not conform to the "
			"PCC specification.", name);
		*failed = true;
		return;
	}

	if (pcc_obj->Buffer.Pointer == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHElementOneBufferNull",
			"PCCH object %s returned a package with element one "
			"which was an ACPI_BUFFER, however, the buffer pointer "
			"was NULL. This does not conform to the PCC "
			"specification.", name);
		*failed = true;
		return;
	}

	if (pcc_obj->Buffer.Length < sizeof(fwts_pcc_register_resource)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHRegisterResourceIllegalSize",
			"PCCH object %s returned a PCC register resource buffer "
			"which was the wrong size. Got %" PRIu32 " bytes, "
			"expected %zu bytes.", name,
			pcc_obj->Buffer.Length, sizeof(fwts_pcc_register_resource));
		*failed = true;
		return;
	}

	pcc_rr = (fwts_pcc_register_resource *)pcc_obj->Buffer.Pointer;

	fwts_log_info_verbatim(fw, "PCC Register Resource (Doorbell) for %s:", name);
	fwts_log_info_simp_int(fw, "  Descriptor:         ", pcc_rr->descriptor);
	fwts_log_info_simp_int(fw, "  Length:             ", pcc_rr->length);
	fwts_log_info_simp_int(fw, "  Space ID:           ", pcc_rr->space_id);
	fwts_log_info_simp_int(fw, "  Bit Width:          ", pcc_rr->bit_width);
	fwts_log_info_simp_int(fw, "  Bit Offset:         ", pcc_rr->bit_offset);
	fwts_log_info_simp_int(fw, "  Access Size:        ", pcc_rr->access_size);
	fwts_log_info_simp_int(fw, "  Address:            ", pcc_rr->address);

	if (pcc_rr->space_id != ACPI_ADR_SPACE_SYSTEM_IO) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCRegisterResourceSpaceIdWrongType",
			"PCC Register Resource Space ID is of the wrong type, got 0x%" PRIx8
			", expected to get type 0x%" PRIx8 " (ACPI_ADR_SPACE_SYSTEM_IO).",
			pcc_rr->space_id, ACPI_ADR_SPACE_SYSTEM_IO);
		*failed = true;
	}

	if ((pcc_rr->bit_width < 1) || (pcc_rr->bit_width > 32)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCRegisterResourceBitWidthError",
			"PCC Register Resource Bit Width is incorrect, got 0x%" PRIx8,
			pcc_rr->bit_width);
		*failed = true;
	}

	if (pcc_rr->address == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCRegisterResourceAddressZero",
			"PCC Register Resource Address is incorrect, got 0x%" PRIx64,
			pcc_rr->address);
		*failed = true;
	}

	fwts_log_nl(fw);
}

static void pcc_check_doorbell_preserve_mask(
	fwts_framework *fw,
	const char *name,
	ACPI_OBJECT *pcc_obj,
	bool *failed)
{
	if (pcc_obj->Type != ACPI_TYPE_INTEGER) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHElementTwoNotInteger",
			"PCCH object %s returned a package with element two "
			"was not an ACPI_INTEGER. This does not conform to the "
			"PCC specification.", name);
		*failed = true;
		return;
	}

	fwts_log_info_verbatim(fw, "PCC Doorbell Preserve Mask for %s:", name);
	fwts_log_info_simp_int(fw, "  Preserve Mask:      ", pcc_obj->Integer.Value);
	fwts_log_nl(fw);
}

static void pcc_check_doorbell_write_mask(
	fwts_framework *fw,
	const char *name,
	ACPI_OBJECT *pcc_obj,
	bool *failed)
{
	if (pcc_obj->Type != ACPI_TYPE_INTEGER) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHElementTwoNotInteger",
			"PCCH object %s returned a package with element three "
			"was not an ACPI_INTEGER. This does not conform to the "
			"PCC specification.", name);
		*failed = true;
		return;
	}

	fwts_log_info_verbatim(fw, "PCC Doorbell Write Mask for %s:", name);
	fwts_log_info_simp_int(fw, "  Write Mask:         ", pcc_obj->Integer.Value);
	fwts_log_nl(fw);

	if (pcc_obj->Integer.Value == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCDoorBellWriteMaskZero",
			"PCC Doorbell Write Mask is incorrect, got 0x%" PRIx64,
			pcc_obj->Integer.Value);
		*failed = true;
	}
}

static void pcc_check_buffer(
	fwts_framework *fw,
	const char *name,
	ACPI_BUFFER *buf)
{
	ACPI_OBJECT *obj;
	bool failed = false;

	if (buf->Pointer == NULL) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHNullPointer",
			"PCCH object %s returned a NULL pointer when evaluated.", name);
		return;
	}

	obj = buf->Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHNotPackage",
			"PCCH object %s did not return an ACPI_PACKAGE when evaluated.", name);
		return;
	}

	if (obj->Package.Count != 4) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCHExpectedTwoElements",
			"PCCH object %s did not return an ACPI_PACKAGE "
			"that contained two elements, got %" PRIu32 " instead.",
			name, obj->Package.Count);
		return;
	}

	pcc_check_shared_memory_region(fw, name, &obj->Package.Elements[0], &failed);
	pcc_check_doorbell_address(fw, name, &obj->Package.Elements[1], &failed);
	pcc_check_doorbell_preserve_mask(fw, name, &obj->Package.Elements[2], &failed);
	pcc_check_doorbell_write_mask(fw, name, &obj->Package.Elements[3], &failed);

	if (!failed)
		fwts_passed(fw, "PCC passed; %s returned sane looking data structures.", name);
}

static int pcc_test1(fwts_framework *fw)
{
	ACPI_BUFFER       buf;
	ACPI_STATUS	  ret;
	fwts_list *pccs;
	static char *name = "PCCH";
	size_t name_len = strlen(name);
	int count = 0;

	fwts_log_info(fw,
		"This test checks the sanity of the Processor Clocking Control "
		"as found on some HP ProLiant machines.  Most computers do not "
		"use this interface to control the CPU clock frequency, so this "
		"test will be skipped.");
	fwts_log_nl(fw);

	if ((pccs = fwts_acpi_object_get_names()) != NULL) {
		fwts_list_link	*item;

		fwts_list_foreach(item, pccs) {
			char *pcc_name = fwts_list_data(char*, item);
			size_t len = strlen(pcc_name);

			if (strncmp(name, pcc_name + len - name_len, name_len) == 0) {
				ret = fwts_acpi_object_evaluate(fw, pcc_name, NULL, &buf);
				if (ACPI_FAILURE(ret) == AE_OK) {
					pcc_check_buffer(fw, pcc_name, &buf);
					count++;
	        			free(buf.Pointer);
				}
			}
		}
	}

	if (count > 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCCTooManyPCCHObjects",
			"The firmware contains too many PCCH objects, expected 1, got %d.", count);
	}

	/* Nothing found, this is not an error */
	if (count == 0) {
		fwts_log_info(fw, "This machine does not use Processor Clocking Control (PCC).");
		fwts_infoonly(fw);
	}

	return FWTS_OK;
}

/* Just one big test */
static fwts_framework_minor_test pcc_tests[] = {
	{ pcc_test1, "Processor Clocking Control (PCC) test." },
	{ NULL, NULL }
};

static fwts_framework_ops pcc_ops = {
	.description = "Processor Clocking Control (PCC) test.",
	.init        = pcc_init,
	.deinit      = pcc_deinit,
	.minor_tests = pcc_tests
};

FWTS_REGISTER("pcc", &pcc_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
