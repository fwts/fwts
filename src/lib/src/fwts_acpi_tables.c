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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "fwts.h"

#include "fwts_acpi_object_eval.h"

#define BIOS_START	(0x000e0000)		/* Start of BIOS memory */
#define BIOS_END  	(0x000fffff)		/* End of BIOS memory */
#define BIOS_LENGTH	(BIOS_END - BIOS_START)	/* Length of BIOS memory */
#define PAGE_SIZE	(4096)

static fwts_acpi_table_info	tables[ACPI_MAX_TABLES];

typedef enum {
	ACPI_TABLES_NOT_LOADED		= 0,
	ACPI_TABLES_LOADED_OK		= 1,
	ACPI_TABLES_LOADED_FAILED	= 2
} acpi_table_load_state;

static acpi_table_load_state acpi_tables_loaded = ACPI_TABLES_NOT_LOADED;

/*
 *  acpi_table_generic_init()
 *  	Generic ACPI table init function
 */
int acpi_table_generic_init(fwts_framework *fw, char *name, fwts_acpi_table_info **table)
{
	if (fwts_acpi_find_table(fw, name, 0, table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI tables.");
		return FWTS_ERROR;
	}
	if (*table == NULL || (*table)->length == 0) {
		fwts_log_error(fw, "ACPI %s table does not exist, skipping test", name);
		return FWTS_SKIP;
	}
	return FWTS_OK;
}

/*
 *  fwts_acpi_find_rsdp_efi()
 *  	Get RSDP address from EFI if possible
 */
static inline void *fwts_acpi_find_rsdp_efi(void)
{
	void *addr;

	addr = fwts_scan_efi_systab("ACPI20");
	if (!addr)
		addr = fwts_scan_efi_systab("ACPI");

	return addr;
}

/*
 *  fwts_acpi_find_rsdp_klog()
 *	Get RSDP by parsing kernel log
 */
static void *fwts_acpi_find_rsdp_klog(void)
{
	fwts_list *klog;
	fwts_list_link *item;
	void *rsdp = NULL;

	if ((klog = fwts_klog_read()) == NULL)
		return NULL;

	fwts_list_foreach(item, klog) {
		char *text = fwts_text_list_text(item);
		char *ptr = strstr(text, "ACPI: RSDP");

		if (ptr) {
			rsdp = (void *)strtoul(ptr + 11, NULL, 16);
			break;
		}
	}

	fwts_text_list_free(klog);

	return rsdp;
}

/*
 *  fwts_acpi_find_rsdp_bios()
 *	Find RSDP address by scanning BIOS memory
 */
static void *fwts_acpi_find_rsdp_bios(void)
{
#ifdef FWTS_ARCH_INTEL
	uint8_t *bios;
	uint8_t *ptr;
	void *addr = 0;

	if ((bios = fwts_mmap(BIOS_START, BIOS_LENGTH)) == FWTS_MAP_FAILED)
		return 0;

	/* Scan BIOS for RSDP, ACPI spec states it is aligned on 16 byte intervals */
	for (ptr = bios; ptr < (bios+BIOS_LENGTH); ptr += 16) {
		fwts_acpi_table_rsdp *rsdp = (fwts_acpi_table_rsdp*)ptr;

		/* Can we read this memory w/o segfaulting? */
		if (fwts_safe_memread(rsdp, 8) != FWTS_OK)
			continue;

		/* Look for RSD PTR string */
		if (strncmp(rsdp->signature, "RSD PTR ",8) == 0) {
			int length = (rsdp->revision < 1) ? 20 : 36;
			if (fwts_safe_memread(ptr, length) != FWTS_OK)
				continue;
			if (fwts_checksum(ptr, length) == 0) {
				addr = (void*)(BIOS_START+(ptr - bios));
				break;
			}
		}
	}
	(void)fwts_munmap(bios, BIOS_LENGTH);

	return addr;
#else
	return NULL;
#endif
}

/*
 *  fwts_acpi_get_rsdp()
 *	given the address of the rsdp, map in the region, copy it and
 *	return the rsdp table. Return NULL if fails.
 */
static fwts_acpi_table_rsdp *fwts_acpi_get_rsdp(fwts_framework *fw, void *addr, size_t *rsdp_len)
{
	uint8_t *mem;
	fwts_acpi_table_rsdp *rsdp;
	*rsdp_len = 0;

	/* Assume version 2.0 size, we don't care about a few bytes over allocation if it's version 1.0 */
	if ((rsdp = (fwts_acpi_table_rsdp*)fwts_low_calloc(1, sizeof(fwts_acpi_table_rsdp))) == NULL) {
		fwts_log_error(fw, "Cannot allocate cached RSDP, out of low memory.");
		return NULL;
	}

	if ((mem = fwts_mmap((long int)addr, sizeof(fwts_acpi_table_rsdp))) == FWTS_MAP_FAILED)
		return NULL;

	if (fwts_safe_memcpy(rsdp, mem, sizeof(fwts_acpi_table_rsdp)) != FWTS_OK) {
		fwts_log_error(fw, "Cannot safely copy RSDP from address %p.", mem);
		goto err;
	}
	(void)fwts_munmap(mem, sizeof(fwts_acpi_table_rsdp));

	/* Determine original RSDP size from revision. */
	*rsdp_len = (rsdp->revision < 1) ? 20 : 36;

	/* Must have the correct signature */
	if (strncmp(rsdp->signature, "RSD PTR ", 8)) {
		fwts_log_error(fw, "RSDP did not have expected signature.");
		goto err;
	}
	return rsdp;

err:
	(void)fwts_munmap(mem, sizeof(fwts_acpi_table_rsdp));
	fwts_low_free(rsdp);
	return NULL;
}

/*
 *  fwts_acpi_load_table()
 *	given the address of a ACPI table, map in firmware, find out size,
 *	copy it and return the copy. Returns NULL if fails.
 */
static void *fwts_acpi_load_table(const off_t addr)
{
	fwts_acpi_table_header *hdr;
	void *mem;
	void *table;
	int len;

	if ((hdr = fwts_mmap((off_t)addr, sizeof(fwts_acpi_table_header))) == FWTS_MAP_FAILED)
		return NULL;

	if (fwts_safe_memread(hdr, sizeof(fwts_acpi_table_header)) != FWTS_OK) {
		(void)fwts_munmap(hdr, sizeof(fwts_acpi_table_header));
		return NULL;
	}

	len = hdr->length;
	if (len < (int)sizeof(fwts_acpi_table_header)) {
		(void)fwts_munmap(hdr, sizeof(fwts_acpi_table_header));
		return NULL;
	}

	(void)fwts_munmap(hdr, sizeof(fwts_acpi_table_header));

	if ((table = fwts_low_calloc(1, len)) == NULL)
		return NULL;
	if ((mem = fwts_mmap((off_t)addr, len)) == FWTS_MAP_FAILED)
		return NULL;
	if (fwts_safe_memcpy(table, mem, len) != FWTS_OK) {
		(void)fwts_munmap(mem, len);
		return NULL;
	}
	(void)fwts_munmap(mem, len);

	return table;
}

/*
 *  fwts_acpi_add_table()
 *	Add a table to internal ACPI table cache. Ignore duplicates based on
 *	their address.
 */
static void fwts_acpi_add_table(
	const char *name,			/* Table Name */
	void *table,				/* Table binary blob */
	const uint64_t addr,			/* Address of table */
	const size_t length,			/* Length of table */
	const fwts_acpi_table_provenance provenance)
						/* Where we got the table from */
{
	int i;
	uint32_t which = 0;

	for (i = 0; i < ACPI_MAX_TABLES; i++) {
		if (addr && tables[i].addr == addr) {
			/* We don't need it, it's a duplicate, so free and return */
			fwts_low_free(table);
			return;
		}
		if (strncmp(tables[i].name, name, 4) == 0)
			which++;
		if (tables[i].data == NULL) {
			memcpy(tables[i].name, name, 4);
			tables[i].name[4] = 0;
			tables[i].data = table;
			tables[i].addr = addr;
			tables[i].length = length;
			tables[i].which = which;
			tables[i].index = i;
			tables[i].provenance = provenance;
			tables[i].has_aml =
				((!strcmp(tables[i].name, "DSDT")) ||
				 (!strcmp(tables[i].name, "SSDT")));
			return;
		}
	}
}

/*
 *  fwts_acpi_free_tables()
 *	free up the cached copies of the ACPI tables
 */
int fwts_acpi_free_tables(void)
{
	int i;

	for (i = 0; i < ACPI_MAX_TABLES; i++) {
		if (tables[i].data) {
			fwts_low_free(tables[i].data);
			memset(&tables[i], 0, sizeof(fwts_acpi_table_info));
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_acpi_is_reduced_hardware()
 *	Check the ACPI tables for HW_REDUCED_ACPI bit in flag field.
 */
fwts_bool fwts_acpi_is_reduced_hardware(fwts_framework *fw)
{
	fwts_acpi_table_info *table = NULL;
	const fwts_acpi_table_fadt *fadt;

	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table FACP.");
		return FWTS_BOOL_ERROR;
	}
	if (table == NULL) {
		fwts_log_error(fw, "ACPI table FACP does not exist!");
		return FWTS_BOOL_ERROR;
	}
	fadt = (const fwts_acpi_table_fadt *) table->data;

	if ((fadt->header.revision >= 5) &&
			(fadt->header.length >= 116) &&
			(fadt->flags & FWTS_FADT_FLAGS_HW_REDUCED_ACPI)) {
		return FWTS_TRUE;
	}
	return FWTS_FALSE;
}

/*
 *  fwts_acpi_handle_fadt_tables()
 *	depending on whether 32 or 64 bit address is usable, get the table
 *	address and load it. This handles the DSDT and FACS as pointed to
 *	by the FADT
 *
 *	Note, we pass in the addresses of the 32 and 64 bit pointers in the
 *	FADT because the FADT may be smaller than expected and we only want
 *	to accesses these fields if the FADT is large enough.
 */
static int fwts_acpi_handle_fadt_tables(
	fwts_framework *fw,
	const fwts_acpi_table_fadt *fadt,/* FADT */
	const char *name,		/* Name of Table addr32/addr 64 point to */
	const char *name_addr32,	/* Name of 32 bit addr */
	const char *name_addr64,	/* Name of 64 bit addr */
	const uint32_t *addr32,		/* 32 bit addr */
	const uint64_t *addr64,		/* 64 bit addr */
	const fwts_acpi_table_provenance provenance)
{
	off_t addr = 0;
	fwts_acpi_table_header *header;

	/* newer version can have address in 64 and 32 bit pointers */
	if ((addr64 != NULL) && (fadt->header.length >= 140)) {
		if (*addr64 == 0) {
			/* Work around buggy firmware, use 32 bit addr instead */
			addr = (off_t)*addr32;
			fwts_log_warning(fw, "FADT %s 64 bit pointer was zero, "
				"falling back to using %s 32 bit pointer.",
				name_addr64, name_addr32);
		} else {
			/* Use default 64 bit addr */
			addr = (off_t)*addr64;
		}
		/* Is it sane? */
		if (addr == 0) {
			fwts_log_warning(fw, "Failed to load %s: Cannot determine "
				"address of %s from FADT, fields %s and %s are zero.",
				name, name, name_addr32, name_addr64);
			return FWTS_NULL_POINTER;
		}
	} else if ((addr32 != NULL) && (fadt->header.length >= 44)) {
		addr = (off_t)*addr32;
		/* Is it sane? */
		if (addr == 0)  {
			fwts_log_warning(fw, "Failed to load %s: Cannot determine "
				"address of %s from FADT, field %s is zero.",
				name, name, name_addr32);
			return FWTS_NULL_POINTER;
		}
	} else if (fadt->header.length < 44) {
		fwts_log_error(fw, "Failed to load %s: FADT is too small and "
			"does not have any %s or %s fields.",
			name, name_addr32, name_addr64);
		return FWTS_ERROR;
	} else {
		/* This should not happen, addr64 or addr32 are NULL */
		fwts_log_error(fw, "Failed to load %s: fwts error with FADT.", name);
		return FWTS_NULL_POINTER;
	}

	/* Sane address found, load and add the table */
	if ((header = fwts_acpi_load_table(addr)) == NULL) {
		fwts_log_error(fw, "Could not load %s from address 0x%" PRIx64 ".",
			name, (uint64_t)addr);
		return FWTS_ERROR;
	}
	fwts_acpi_add_table(header->signature, header,
		(uint64_t)addr, header->length, provenance);
	return FWTS_OK;
}

/*
 *  fwts_acpi_handle_fadt()
 *	The FADT points to the FACS and DSDT with either 32 or 64 bit pointers.
 *	Locate the FACS and DSDT tables and load them.
 */
static int fwts_acpi_handle_fadt(
	fwts_framework *fw,
	const uint64_t phys_addr,
	const fwts_acpi_table_fadt *fadt,
	const fwts_acpi_table_provenance provenance)
{
	static uint64_t	facs_last_phys_addr;	/* default to zero */
	int result;

	/*
	 *  The FADT handling may occur twice if it appears
	 *  in the RSDT and the XDST, so as an optimisation
	 *  we just need to handle it once.
	 */
	if (facs_last_phys_addr == phys_addr)
		return FWTS_OK;

	facs_last_phys_addr = phys_addr;

	/* Determine FACS addr and load it.
	 * Will ignore the missing FACS in the hardware-reduced mode.
	 */
PRAGMA_PUSH
PRAGMA_PACK_WARN_OFF
	result = fwts_acpi_handle_fadt_tables(fw, fadt,
			"FACS", "FIRMWARE_CTRL", "X_FIRMWARE_CTRL",
			&fadt->firmware_control, &fadt->x_firmware_ctrl,
			provenance);
PRAGMA_POP
	if (result != FWTS_OK) {
		if ((result == FWTS_NULL_POINTER) &&
				(fwts_acpi_is_reduced_hardware(fw) == FWTS_TRUE)) {
			fwts_log_info(fw, "Ignore the missing FACS. "
					"It is optional in hardware-reduced mode");
		} else {
			fwts_log_error(fw, "Failed to load FACS.");
			return FWTS_ERROR;
		}
	}
	/* Determine DSDT addr and load it */
PRAGMA_PUSH
PRAGMA_PACK_WARN_OFF
	if (fwts_acpi_handle_fadt_tables(fw, fadt,
	    "DSDT", "DSTD", "X_DSDT",
	    &fadt->dsdt, &fadt->x_dsdt, provenance) != FWTS_OK) {
		fwts_log_error(fw, "Failed to load DSDT.");
		return FWTS_ERROR;
	}
PRAGMA_POP
	return FWTS_OK;
}

/*
 *  fwts_acpi_load_tables_from_firmware()
 *  	Load up cached copies of all the ACPI tables
 */
static int fwts_acpi_load_tables_from_firmware(fwts_framework *fw)
{
	fwts_acpi_table_rsdp *rsdp;
	fwts_acpi_table_xsdt *xsdt;
	fwts_acpi_table_rsdt *rsdt;
	fwts_acpi_table_header *header;

	void *	rsdp_addr = NULL;
	size_t	rsdp_len;
	int 	num_entries;
	int 	i;

	/* Check for RSDP as EFI, then BIOS, parsed klog, if not found, give up */
	if (fw->rsdp)
		rsdp_addr = (void*)fw->rsdp;
	if (!rsdp_addr)
		rsdp_addr = fwts_acpi_find_rsdp_efi();
	if (!rsdp_addr)
		rsdp_addr = fwts_acpi_find_rsdp_bios();
	if (!rsdp_addr)
		rsdp_addr = fwts_acpi_find_rsdp_klog();
	if (!rsdp_addr)
		return FWTS_ERROR;

	/* Load and save cached RSDP */
	if ((rsdp = fwts_acpi_get_rsdp(fw, rsdp_addr, &rsdp_len)) == NULL)
		return FWTS_ERROR;

	fwts_acpi_add_table("RSDP", rsdp, (uint64_t)(long int)rsdp_addr, rsdp_len, FWTS_ACPI_TABLE_FROM_FIRMWARE);

	/* Load any tables from XSDT if it's valid */
	if (rsdp->xsdt_address) {
		if ((xsdt = fwts_acpi_load_table((off_t)rsdp->xsdt_address)) != NULL) {
			fwts_acpi_add_table("XSDT", xsdt, (uint64_t)rsdp->xsdt_address,
				xsdt->header.length, FWTS_ACPI_TABLE_FROM_FIRMWARE);
			num_entries = (xsdt->header.length - sizeof(fwts_acpi_table_header)) / 8;
			for (i = 0; i < num_entries; i++) {
				if (xsdt->entries[i]) {
					if ((header = fwts_acpi_load_table((off_t)xsdt->entries[i])) != NULL) {
						if (strncmp("FACP", header->signature, 4) == 0)
							if (fwts_acpi_handle_fadt(fw,
							    (uint64_t)xsdt->entries[i],
							    (fwts_acpi_table_fadt *)header,
							    FWTS_ACPI_TABLE_FROM_FIRMWARE) != FWTS_OK)
								goto fail;
						fwts_acpi_add_table(header->signature, header, xsdt->entries[i],
							header->length, FWTS_ACPI_TABLE_FROM_FIRMWARE);
					}
				}
			}
		}
	}

	/* Load any tables from RSDT if it's valid */
	if (rsdp->rsdt_address) {
		if ((rsdt = fwts_acpi_load_table((off_t)rsdp->rsdt_address)) != NULL) {
			fwts_acpi_add_table("RSDT", rsdt, (uint64_t)rsdp->rsdt_address,
				rsdt->header.length, FWTS_ACPI_TABLE_FROM_FIRMWARE);
			num_entries = (rsdt->header.length - sizeof(fwts_acpi_table_header)) / 4;
			for (i = 0; i < num_entries; i++) {
				if (rsdt->entries[i]) {
					if ((header = fwts_acpi_load_table((off_t)rsdt->entries[i])) != NULL) {
						if (strncmp("FACP", header->signature, 4) == 0)
							if (fwts_acpi_handle_fadt(fw,
							    (uint64_t)rsdt->entries[i],
							    (fwts_acpi_table_fadt *)header,
							    FWTS_ACPI_TABLE_FROM_FIRMWARE) != FWTS_OK)
								goto fail;
						fwts_acpi_add_table(header->signature, header, (uint64_t)rsdt->entries[i],
							header->length, FWTS_ACPI_TABLE_FROM_FIRMWARE);
					}
				}
			}
		}
	}

	return FWTS_OK;
fail:
	/*
	 * Free'ing the tables will cause acpica_init to fail
	 * and so we abort any ACPI related tests
	 */
	fwts_acpi_free_tables();
	return FWTS_ERROR;
}

/*
 *  fwts_fake_physical_addr()
 *	Loading tables from file may result in data without an originating
 *	physical address of the table, so fake a unique 32 bit address for the table.
 */
static uint32_t fwts_fake_physical_addr(const size_t size)
{
	static uint32_t fake_phys_addr = 0xbff00000;
	uint32_t addr = fake_phys_addr;

	fake_phys_addr += (size + 16);

	return addr;
}

/*
 *  fwts_acpi_load_table_from_acpidump()
 *	Load an ACPI table from the output of acpidump or fwts --dump
 */
static uint8_t *fwts_acpi_load_table_from_acpidump(
	fwts_framework *fw,
	FILE *fp,
	char *name,
	uint64_t *addr,
	size_t *size)
{
	uint32_t offset, expected_offset = 0;
	uint8_t  data[16];
	char buffer[128];
	uint8_t *table;
	uint8_t *tmp = NULL;
	char *ptr;
	size_t len = 0;
	unsigned long long table_addr;
	ptrdiff_t name_len;

	*size = 0;

	if (fgets(buffer, sizeof(buffer), fp) == NULL)
		return NULL;

	/*
	 * Parse tablename followed by address, e.g.
	 *   DSTD @ 0xbfa02344
	 *   SSDT4 @ 0xbfa0f230
	 */
	ptr = strstr(buffer, "@ 0x");
	if (ptr == NULL)
		return NULL; /* Can't find table name */

	name_len = ptr - buffer;
	/*
	 * We should have no more than the table name (4..5 chars)
	 * plus a space left between the start of the buffer and
	 * the @ sign.  If we have more then something is wrong with
	 * the data. So just ignore this garbage as we don't want to
	 * overflow the name on the following strcpy()
	 */
	if ((name_len > 6) || (name_len < 5))
		return NULL; /* Name way too long or too short */

	if (sscanf(ptr, "@ 0x%Lx\n", &table_addr) < 1)
		return NULL; /* Can't parse address */

	*(ptr-1) = '\0';
	strcpy(name, buffer);

	/* In fwts RSD PTR is known as the RSDP */
	if (strncmp(name, "RSD PTR", 7) == 0)
		strcpy(name, "RSDP");

	/*
	 *  Pull in 16 bytes at a time, data MUST be conforming to the
	 *  acpidump format, e.g.:
	 *   0000: 46 41 43 50 f4 00 00 00 03 f9 41 4d 44 20 20 20  FACP......AMD
	 *
	 *  This parser expects hex data to be separated by one space,
	 *  anything not conforming to this rigid format will be prematurely
	 *  aborted
	 */
	while (fgets(buffer, sizeof(buffer), fp) ) {
		uint8_t *new_tmp;
		int n;

		/* Get offset */
		if (sscanf(buffer,"  %" SCNx32 ": ", &offset) < 1)
			break;

		/* Offset are not correct, abort with truncated table */
		if (offset != expected_offset) {
			fwts_log_error(fw, "ACPI dump offsets in table '%s'"
				" are not incrementing by 16 bytes per row. "
				" Table truncated prematurely due to bad acpidump data.",
				name);
			break;
		}

		expected_offset += 16;

		/* Data follows the colon, abort if not found */
		ptr = strstr(buffer, ": ");
		if (!ptr) {
			fwts_log_error(fw, "ACPI dump in table '%s' did not contain "
				"any data, expecting at least 1 hex byte of data per row.",
				name);
			break;
		}

		ptr += 2;
		/* Now expect 16 lots of 2 hex digits and a space */
		for (n = 0; n < 16; n++) {
			/*
			 *  Need to be 100% sure 2 hex digits. Maybe a short row
		 	 *  because it is the end of the table, so assume it is the
			 *  end of the table if not hex digits.
			 */
			if (!isxdigit(*ptr) || !isxdigit(*(ptr + 1))) {
				break;
			}
			if (sscanf(ptr, "%2" SCNx8, &data[n]) != 1) {
				/* Bug in parsing! should never happen! */
				fwts_log_error(fw, "ACPI dump in table '%s' at hex byte position "
					"%d did parse correctly, got "
					"'%2.2s' which could not be parsed as a hex value.",
					name, n, ptr);
				break;
			}
			ptr += 3;
		}

		/* Got no data? */
		if (n == 0) {
			fwts_log_error(fw, "ACPI dump in table '%s' did not contain "
				"any data, expecting at least 1 hex byte of data per row.",
				name);
			break;
		}

		len += n;
		if ((new_tmp = realloc(tmp, len)) == NULL) {
			free(tmp);
			fwts_log_error(fw, "ACPI table parser run out of memory parsing table '%s'.", name);
			return NULL;
		} else
			tmp = new_tmp;

		memcpy(tmp + offset, data, n);

		/* Treat less than a full row as last one */
		if (n != 16)
			break;
	}
	/* Unlikely, but an empty table should be checked for */
	if (!tmp) {
		fwts_log_error(fw, "ACPI table parser found an empty table '%s'.", name);
		return NULL;
	}

	/* Allocate the table using low 32 bit memory */
	if ((table = fwts_low_malloc(len)) == NULL) {
		free(tmp);
		fwts_log_error(fw, "ACPI table parser run out of 32 bit memory parsing table '%s'.", name);
		return NULL;
	}
	memcpy(table, tmp, len);
	free(tmp);

	if (table_addr == 0)
		table_addr = fwts_fake_physical_addr(len);

	/* We may need to fake a physical address if its null */
	*addr = (uint64_t)(table_addr == 0 ? fwts_fake_physical_addr(len) : table_addr);
	*size = len;

	return table;
}

/*
 *  fwts_acpi_load_tables_from_acpidump()
 *	Load in all ACPI tables from output of acpidump or fwts --dump
 */
static int fwts_acpi_load_tables_from_acpidump(fwts_framework *fw)
{
	FILE *fp;

	if (!fw->acpi_table_acpidump_file)
		return FWTS_ERROR;

	if ((fp = fopen(fw->acpi_table_acpidump_file, "r")) == NULL) {
		fwts_log_error(fw, "Cannot open '%s' to read ACPI tables.",
			fw->acpi_table_acpidump_file);
		return FWTS_ERROR;
	}

	while (!feof(fp)) {
		uint64_t addr;
		uint8_t *table;
		size_t length;
		char name[16];

		if ((table = fwts_acpi_load_table_from_acpidump(fw, fp, name, &addr, &length)) != NULL)
			fwts_acpi_add_table(name, table, addr, length, FWTS_ACPI_TABLE_FROM_FILE);
	}

	(void)fclose(fp);

	return FWTS_OK;
}

/*
 *  fwts_acpi_load_table_from_file()
 *	load table from a raw binary dump
 */
static uint8_t *fwts_acpi_load_table_from_file(const int fd, size_t *length)
{
	uint8_t *ptr = NULL, *tmp;
	size_t size = 0;
	char buffer[4096];

	*length = 0;

	/*
	 *  read table into buffer in the normal heap
	 *  so we don't have to do expensive fwts_low_reallocs
	 *  per iteration on long tables
	 */
	for (;;) {
		ssize_t n = read(fd, buffer, sizeof(buffer));

		if (n == 0)
			break;
		if (n < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				fwts_low_free(ptr);
				return NULL;
			}
			continue;
		}
		if (n > (ssize_t)sizeof(buffer))
			goto err;	/* Unlikely */
		if (size + n > 0xffffffff)
			goto err;	/* Very unlikely */

		if ((tmp = (uint8_t*)realloc(ptr, size + n + 1)) == NULL) {
			free(ptr);
			return NULL;
		}
		ptr = tmp;
		memcpy(ptr + size, buffer, n);
		size += n;
	}

	if (!ptr || !size)
		goto err_no_data;

	/*
	 *  ..and copy table into a 32 bit memory space buffer
	 */
	if ((tmp = fwts_low_malloc(size)) == NULL)
		goto err;

	*length = size;
	memcpy(tmp, ptr, size);
	free(ptr);
	return tmp;

err:
	free(ptr);
err_no_data:
	*length = 0;
	return NULL;
}

/*
 *  fwts_acpi_load_tables_from_file_generic()
 *	load acpi tables from a given path with a given file extension
 */
static int fwts_acpi_load_tables_from_file_generic(
	fwts_framework *fw,
	char *acpi_table_path,
	const char *extension,
	int *count)
{
	struct dirent **dir_entries;
	int i, n;

	*count = 0;
	if (!acpi_table_path)
		return FWTS_ERROR;

	/*
 	 * Read in directory in alphabetical name sorted order
	 * to ensure the tables are always loaded into memory
	 * in some form of deterministic order
	 */
	if ((n = scandir(acpi_table_path, &dir_entries, 0, alphasort)) < 0)
		return FWTS_ERROR;

	for (i = 0; i < n; i++) {
		/* Ignore . directories */
		if (dir_entries[i]->d_name[0] == '.')
			continue;

		if (strstr(dir_entries[i]->d_name, extension)) {
			char path[PATH_MAX];
			int fd;

			snprintf(path, sizeof(path), "%s/%s",
				acpi_table_path, dir_entries[i]->d_name);
			if ((fd = open(path, O_RDONLY)) >= 0) {
				uint8_t *table;
				size_t length;
				struct stat buf;

				if (fstat(fd, &buf) < 0) {
					fwts_log_error(fw, "Cannot stat file '%s'\n", path);
					(void)close(fd);
					continue;
				}
				/* Must be a regular file */
				if (!S_ISREG(buf.st_mode)) {
					(void)close(fd);
					continue;
				}

				if ((table = fwts_acpi_load_table_from_file(fd, &length)) != NULL) {
					char name[9];	/* "RSD PTR " or standard ACPI 4 letter name */

					fwts_acpi_table_rsdp *rsdp  = (fwts_acpi_table_rsdp *)table;

					/* Could be RSDP or a standard ACPI table, so check */

					if (!strncmp(rsdp->signature, "RSD PTR ", 8)) {
						/* In fwts, RSD PTR is tagged as the RSDP */
						strcpy(name, "RSDP");
					} else {
						/* Assume it is a standard ACPI table */
						fwts_acpi_table_header *hdr = (fwts_acpi_table_header *)table;

						strncpy(name, hdr->signature, 4);
						name[4] = '\0';
					}

					(*count)++;

					if (!strncmp(name, "XSDT", 4) || !strncmp(name, "RSDT", 4)) {
						/*
						 * For XSDT and RSDT we don't bother loading at this point.
						 * These tables point to the other tables, however, we can't
						 * figure out which table each pointer references because
						 * we are loading in raw table data and we don't know where
						 * these were located in the original machine.  So the best
						 * way forward is to ignore these tables and instead leave
						 * the fix up stage fwts_acpi_load_tables_fixup() to magically
						 * create faked XSDT and RSDT entries based on the tables
						 * we've loaded from file.
						 */
						fwts_low_free(table);
					} else {
						fwts_acpi_add_table(name, table,
							(uint64_t)fwts_fake_physical_addr(length), length,
							FWTS_ACPI_TABLE_FROM_FILE);
					}
				}
				(void)close(fd);
			} else
				fwts_log_error(fw, "Cannot load ACPI table from file '%s'\n", path);
		}
		free(dir_entries[i]);
	}
	free(dir_entries);
	return FWTS_OK;
}

/*
 *  fwts_acpi_load_tables_from_file()
 *	load raw acpi tables from a user given path, files must end in ".dat"
 */
static int fwts_acpi_load_tables_from_file(fwts_framework *fw)
{
	int count;

	if (!fw->acpi_table_path)
		return FWTS_ERROR;

	fwts_acpi_load_tables_from_file_generic(fw, fw->acpi_table_path, ".dat", &count);
	if (count == 0) {
		fwts_log_error(fw, "Could not find any ACPI tables in directory '%s'.\n", fw->acpi_table_path);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  fwts_acpi_table_fixable()
 *	return true if a table can be put into RSDT or XSDT
 */
static bool fwts_acpi_table_fixable(fwts_acpi_table_info *table)
{
	fwts_acpi_table_header *header;

	if (table == NULL)
		return false;

	header = (fwts_acpi_table_header *)table->data;

	/* We don't want RSDT or XSDT in the RSDT and XSDT */
	if (strncmp(header->signature, "RSDT", 4) == 0)
		return false;
	if (strncmp(header->signature, "XSDT", 4) == 0)
		return false;

	return true;
}

/*
 *  fwts_acpi_fixup_addr_from_fadt()
 *	fixup the ACPI table address of a given table if we have it defined
 *	in the FADT.  This is only for fixed up tables loaded from file
 *	where these table addresses are unknown and hence are faked physical
 *	addresses to keep ACPICA happy.
 */
void fwts_acpi_fixup_addr_from_fadt(
	fwts_framework *fw,
	const char *name,
	uint32_t addr32,
	uint64_t addr64)
{
	fwts_acpi_table_info *table = NULL;

	/* Fetch the table */
	if (fwts_acpi_find_table(fw, name, 0, &table) != FWTS_OK)
		return;
	if (!table)
		return;

	/*
	 *  OK, we have something to patch up, 64 bit addresses
	 *  are used in preference to 32 bit. And only patch
	 *  the addresses if they are non-zero.
	 */
	if (addr64)
		table->addr = addr64;
	else if (addr32)
		table->addr = addr32;
}

/*
 *  fwts_acpi_load_tables_fixup()
 *	tables loaded from file sometimes do not contain the original
 *	physical address of the tables, so these need faking. Also, some
 *	poor ACPI dumping implementations fail to save the RSDP, RSDT and
 *	XSDT, so we need to create fake versions from scratch.
 */
static int fwts_acpi_load_tables_fixup(fwts_framework *fw)
{
	int i, j, count;
	char *oem_tbl_id;
	fwts_acpi_table_info *table = NULL;
	fwts_acpi_table_rsdp *rsdp = NULL;
	fwts_acpi_table_fadt *fadt = NULL;
	uint64_t rsdt_fake_addr = 0, xsdt_fake_addr = 0;
	bool redo_rsdp_checksum = false;

	/* Fetch the OEM Table ID */
	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "ACPI table find failure.");
		return FWTS_ERROR;
	}
	if (!table) {
		fwts_log_error(fw, "Cannot find FACP.");
		return FWTS_ERROR;
	}

	fadt = (fwts_acpi_table_fadt *)table->data;
	oem_tbl_id = fadt->header.oem_tbl_id;

	/* Get FACS */
	if (fwts_acpi_find_table(fw, "FACS", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "ACPI table find failure.");
		return FWTS_ERROR;
	}
	if (!table) {
		size_t size = 64;
		uint64_t facs_addr;
		fwts_acpi_table_facs *facs = NULL;

		if (fwts_acpi_is_reduced_hardware(fw))	/* FACS is optional on reduced hardware */
			fwts_log_info(fw, "No FACS found, fwts has faked one instead.");
		else	/* This is most unexpected, so warn about it */
			fwts_log_warning(fw, "No FACS found, fwts has faked one instead.");

		if ((facs = fwts_low_calloc(1, size)) == NULL) {
			fwts_log_error(fw, "Cannot allocate fake FACS.");
			return FWTS_ERROR;
		}
		(void)memcpy(facs->signature, "FACS", 4);
		facs->length = size;
		facs->hardware_signature = 0xf000a200;	/* Some signature */
		facs->flags = 0;
		facs->version = 2;

		/* Get physical address of FACS, try to take it from FACS first,
		   and if that fails, create a fake one and update FACS */
		if (fadt->header.length >= 140 && fadt->x_firmware_ctrl != 0) {
			facs_addr = fadt->x_firmware_ctrl;
		} else if (fadt->firmware_control != 0) {
			facs_addr = (uint64_t)fadt->firmware_control;
		} else {
			facs_addr = (uint64_t)fwts_fake_physical_addr(size);
			if (fadt->header.length >= 140)
				fadt->x_firmware_ctrl = facs_addr;
			else
				fadt->firmware_control = (uint32_t)facs_addr;
			fadt->header.checksum -= fwts_checksum((uint8_t*)&facs_addr, sizeof(facs_addr));
		}

		fwts_acpi_add_table("FACS", facs, (uint64_t)facs_addr,
			size, FWTS_ACPI_TABLE_FROM_FIXUP);
	}

	/*
	 *  In the case where the tables have be loaded from file
	 *  and the address is not known we may have a valid FACP (FADT)
	 *  that points to the FACS and DSDT, so we need to ensure
	 *  fake addresses created by fwts for these are in-sync
	 *  before we create RSDT and XSDTs
	 */
	fwts_acpi_fixup_addr_from_fadt(fw, "FACS", fadt->firmware_control, fadt->x_firmware_ctrl);
	fwts_acpi_fixup_addr_from_fadt(fw, "DSDT", fadt->dsdt, fadt->x_dsdt);

	/* Figure out how many tables we need to put into RSDT and XSDT */
	for (count = 0, i = 0; ; i++) {
		if (fwts_acpi_get_table(fw, i, &table) != FWTS_OK)
			break;
		if (table == NULL)	/* No more tables */
			break;
		if (fwts_acpi_table_fixable(table))
			count++;
	}

	/* Get RSDT */
	if (fwts_acpi_find_table(fw, "RSDT", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "ACPI table find failure.");
		return FWTS_ERROR;
	}
	if (table) {
		rsdt_fake_addr = table->addr;
	} else {
		/* No RSDT? go and fake one */

		fwts_acpi_table_rsdt *rsdt;
		size_t size = sizeof(fwts_acpi_table_rsdt) + (count * sizeof(uint32_t));

		if ((rsdt = fwts_low_calloc(1, size)) == NULL) {
			fwts_log_error(fw, "Cannot allocate fake RSDT.");
			return FWTS_ERROR;
		}

		for (i = 0,j = 0; j < count; i++)
			if (fwts_acpi_get_table(fw, i, &table) == FWTS_OK)
				if (table && fwts_acpi_table_fixable(table))
					rsdt->entries[j++] = (uint32_t)table->addr;

		(void)memcpy(rsdt->header.signature, "RSDT", 4);
		rsdt->header.length = size;
		rsdt->header.revision = 1;
		(void)memcpy(rsdt->header.oem_id, "FWTSID", 6);
		(void)memcpy(rsdt->header.oem_tbl_id, oem_tbl_id, 8);
		rsdt->header.oem_revision = 1;
		(void)memcpy(rsdt->header.creator_id, "FWTS", 4);
		rsdt->header.creator_revision = 1;
		rsdt->header.checksum = 256 - fwts_checksum((uint8_t*)rsdt, size);

		rsdt_fake_addr = fwts_fake_physical_addr(size);
		fwts_acpi_add_table("RSDT", rsdt, rsdt_fake_addr, size, FWTS_ACPI_TABLE_FROM_FIXUP);
	}

	/* Get XSDT */
	if (fwts_acpi_find_table(fw, "XSDT", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "ACPI table find failure.");
		return FWTS_ERROR;
	}
	if (table) {
		xsdt_fake_addr = table->addr;
	} else {
		/* No XSDT? go and fake one */

		fwts_acpi_table_xsdt *xsdt;
		size_t size = sizeof(fwts_acpi_table_rsdt) + (count * sizeof(uint64_t));

		if ((xsdt = fwts_low_calloc(1, size)) == NULL) {
			fwts_log_error(fw, "Cannot allocate fake XSDT.");
			return FWTS_ERROR;
		}

		for (i = 0,j = 0; j < count; i++)
			if (fwts_acpi_get_table(fw, i, &table) == FWTS_OK)
				if (table && fwts_acpi_table_fixable(table))
					xsdt->entries[j++] = table->addr;

		(void)memcpy(xsdt->header.signature, "XSDT", 4);
		xsdt->header.length = size;
		xsdt->header.revision = 1;
		(void)memcpy(xsdt->header.oem_id, "FWTSID", 6);
		(void)memcpy(xsdt->header.oem_tbl_id, oem_tbl_id, 8);
		xsdt->header.oem_revision = 1;
		(void)memcpy(xsdt->header.creator_id, "FWTS", 4);
		xsdt->header.creator_revision = 1;
		xsdt->header.checksum = 256 - fwts_checksum((uint8_t*)xsdt, size);

		xsdt_fake_addr = fwts_fake_physical_addr(size);
		fwts_acpi_add_table("XSDT", xsdt, xsdt_fake_addr, size, FWTS_ACPI_TABLE_FROM_FIXUP);
	}

	/* Get RSDP */
	if (fwts_acpi_find_table(fw, "RSDP", 0, &table) != FWTS_OK)
		return FWTS_ERROR;
	if (table)
		rsdp = (fwts_acpi_table_rsdp *)table->data;
	else {
		/* No RSDP? go and fake one */
		size_t size = sizeof(fwts_acpi_table_rsdp);

		if ((rsdp = fwts_low_calloc(1, size)) == NULL) {
			fwts_log_error(fw, "Cannot allocate fake RSDP.");
			return FWTS_ERROR;
		}

		(void)memcpy(rsdp->signature, "RSD PTR ", 8);
		(void)memcpy(rsdp->oem_id, "FWTSID", 6);
		rsdp->revision = 2;
		rsdp->length = sizeof(fwts_acpi_table_rsdp);
		rsdp->reserved[0] = 0;
		rsdp->reserved[1] = 0;
		rsdp->reserved[2] = 0;

		rsdp->checksum = 256 - fwts_checksum((uint8_t*)rsdp, 20);
		rsdp->extended_checksum = 256 - fwts_checksum((uint8_t*)rsdp, rsdp->length);

		fwts_acpi_add_table("RSDP", rsdp, (uint64_t)fwts_fake_physical_addr(size),
			sizeof(fwts_acpi_table_rsdp), FWTS_ACPI_TABLE_FROM_FIXUP);
	}

	/* Now we have all the tables, final fix up is required */
	if (fw->target_arch == FWTS_ARCH_ARM64) {
		if ((rsdp->revision > 0) && (rsdp->length >= 36) &&
		    (rsdp->xsdt_address != xsdt_fake_addr)) {
			rsdp->xsdt_address = xsdt_fake_addr;
			redo_rsdp_checksum = true;
		}
	} else {
		if (rsdp->rsdt_address != rsdt_fake_addr) {
			rsdp->rsdt_address = rsdt_fake_addr;
			redo_rsdp_checksum = true;
		}

		if ((rsdp->revision > 0) && (rsdp->length >= 36) &&
		    (rsdp->xsdt_address != xsdt_fake_addr)) {
			rsdp->xsdt_address = xsdt_fake_addr;
			redo_rsdp_checksum = true;
		}
	}
	/* And update checksum if we've updated the rsdp */
	if (redo_rsdp_checksum) {
		rsdp->checksum = 0;	/* Clear old checksum */
		rsdp->checksum = 256 - fwts_checksum((uint8_t*)rsdp, 20);
		rsdp->extended_checksum = 0;	/* Clear old checksum */
		rsdp->extended_checksum = 256 - fwts_checksum((uint8_t*)rsdp, rsdp->length);
	}

	return FWTS_OK;
}


static int fwts_acpi_load_tables_from_sysfs(fwts_framework *fw)
{
	int count, total;
	fwts_acpi_load_tables_from_file_generic(fw, "/sys/firmware/acpi/tables", "", &total);
	fwts_acpi_load_tables_from_file_generic(fw, "/sys/firmware/acpi/tables/dynamic", "", &count);
	total += count;
	if (total == 0) {
		fwts_log_error(fw, "Could not find any ACPI tables in directory '/sys/firmware/acpi/tables'.\n");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  fwts_acpi_load_tables()
 *	Load from firmware or from files in a specified directory
 */
int fwts_acpi_load_tables(fwts_framework *fw)
{
	int ret;
	bool require_fixup = false;

	if (fw->acpi_table_path != NULL) {
		ret = fwts_acpi_load_tables_from_file(fw);
		require_fixup = true;
	} else if (fw->acpi_table_acpidump_file != NULL) {
		ret = fwts_acpi_load_tables_from_acpidump(fw);
		require_fixup = true;
	} else if (fwts_check_root_euid(fw, true) == FWTS_OK) {
		ret = fwts_acpi_load_tables_from_sysfs(fw);

		/* Load from sysfs failed (e.g. no /sys/firmware/acpi/tables), so try memory */
		if (ret != FWTS_OK)
			ret = fwts_acpi_load_tables_from_firmware(fw);
		else
			require_fixup = true;
	} else {
		ret = FWTS_ERROR_NO_PRIV;
	}

	if (ret == FWTS_OK) {
		acpi_tables_loaded = ACPI_TABLES_LOADED_OK;

		/* Loading from file may require table address fixups */
		if (require_fixup)
			fwts_acpi_load_tables_fixup(fw);
	} else {
		acpi_tables_loaded = ACPI_TABLES_LOADED_FAILED;
	}

	return ret;
}

/*
 *  fwts_acpi_find_table()
 *  	Search for an ACPI table. There may be more than one, so
 *  	specify the one using which.
 */
int fwts_acpi_find_table(
	fwts_framework *fw,
	const char *name,
	const uint32_t which,
	fwts_acpi_table_info **info)
{
	int i;

	if (info == NULL)
		return FWTS_NULL_POINTER;

	*info = NULL;

	if (acpi_tables_loaded == ACPI_TABLES_LOADED_FAILED)
		return FWTS_ERROR;

	if (acpi_tables_loaded == ACPI_TABLES_NOT_LOADED) {
		int ret;

		if ((ret = fwts_acpi_load_tables(fw)) != FWTS_OK)
			return ret;
	}

	for (i = 0; i < ACPI_MAX_TABLES; i++) {
		if (tables[i].data == NULL)
			break;
		if ((strcmp(tables[i].name, name) == 0) &&
	            (tables[i].which == which)) {
			*info = &tables[i];
			return FWTS_OK;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_acpi_find_table_by_addr()
 *  	Search for a cached ACPI table given it's original physical address
 */
int fwts_acpi_find_table_by_addr(fwts_framework *fw, const uint64_t addr, fwts_acpi_table_info **info)
{
	int i;

	if (info == NULL)
		return FWTS_NULL_POINTER;

	*info = NULL;

	if (acpi_tables_loaded == ACPI_TABLES_NOT_LOADED) {
		int ret;
		if ((ret = fwts_acpi_load_tables(fw)) != FWTS_OK)
			return ret;
	}

	for (i = 0; i < ACPI_MAX_TABLES; i++) {
		if (tables[i].data == NULL)
			break;
		if (tables[i].addr == addr) {
			*info = &tables[i];
			return FWTS_OK;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_acpi_get_table()
 *  	Get the Nth cached ACPI table.
 */
int fwts_acpi_get_table(fwts_framework *fw, const uint32_t index, fwts_acpi_table_info **info)
{
	if (info == NULL)
		return FWTS_NULL_POINTER;

	*info = NULL;

	if (index >= ACPI_MAX_TABLES)
		return FWTS_ERROR;

	if (acpi_tables_loaded == ACPI_TABLES_NOT_LOADED) {
		int ret;
		if ((ret = fwts_acpi_load_tables(fw)) != FWTS_OK)
			return ret;
	}

	if (tables[index].data == NULL)
		return FWTS_OK;

	*info = &tables[index];
	return FWTS_OK;
}

/*
 *  fwts_acpi_obj_find()
 *  Returns whether obj_name can be found in the ACPI object name space.
 */
bool fwts_acpi_obj_find(fwts_framework *fw, const char *obj_name)
{
	char expanded[BUFSIZ];
	char *c = expanded;
	const char *obj_ptr;
	int i;
	fwts_list_link *item;
	fwts_list *objects;
	bool found = false;

	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return false;
	}
	if ((objects = fwts_acpi_object_get_names()) == NULL) {
		fwts_log_info(fw, "Cannot find any ACPI objects");
		fwts_acpi_deinit(fw);
		return false;
	}

	memset(expanded, 0, BUFSIZ);

	/*
	 *  Converts \_SB.A.BB.CCC.DDDD.EE to
	 *           \_SB_.A___.BB__.CCC_.DDDD.EE__
	 */
	for (i = -1, obj_ptr = obj_name; ; obj_ptr++) {
		if (*obj_ptr == '.' || *obj_ptr == '\0') {
			while (i < 4) {
				*c++ = '_';
				i++;
			}
			i = 0;
		} else {
			i++;
		}
		*c++ = *obj_ptr;
		if (!*obj_ptr)
			break;
	}

	/* Search for object */
	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char*, item);

		if (strcmp(expanded, name) == 0) {
			found = true;
			break;
		}
	}

	fwts_acpi_deinit(fw);
	return found;
}

/*
 *  fwts_acpi_reserved_zero_()
 *  verify whether the reserved field is zero
 */
void fwts_acpi_reserved_zero_(
	fwts_framework *fw,
	const char *table,
	const char *field,
	const uint64_t value,
	const uint8_t size,
	bool *passed)
{
	if (value != 0) {
		char label[20];

		strncpy(label, table, 4);	/* ACPI table name is 4 char long */
		strncpy(label + 4, "ReservedNonZero", sizeof(label) - 4);

		switch (size) {
		case sizeof(uint8_t):
			fwts_failed(fw, LOG_LEVEL_MEDIUM, label,
				"%4.4s %s field must be zero, got "
				"0x%2.2" PRIx8 " instead", table, field, (uint8_t)value);
			break;
		case sizeof(uint16_t):
			fwts_failed(fw, LOG_LEVEL_MEDIUM, label,
				"%4.4s %s field must be zero, got "
				"0x%4.4" PRIx16 " instead", table, field, (uint16_t)value);
			break;
		case sizeof(uint32_t):
			fwts_failed(fw, LOG_LEVEL_MEDIUM, label,
				"%4.4s %s field must be zero, got "
				"0x%8.8" PRIx32 " instead", table, field, (uint32_t)value);
			break;
		case sizeof(uint64_t):
			fwts_failed(fw, LOG_LEVEL_MEDIUM, label,
				"%4.4s %s field must be zero, got "
				"0x%16.16" PRIx64 " instead", table, field, value);
			break;
		}
		*passed = false;
	}
}

/*
 *  fwts_acpi_reserved_zero_array()
 *  verify whether the reserved array is all zeroed
 */
void fwts_acpi_reserved_zero_array(
	fwts_framework *fw,
	const char *table,
	const char *field,
	const uint8_t* data,
	const uint8_t length,
	bool *passed)
{
	uint32_t value = 0;
	char label[20];
	uint8_t i;

	strncpy(label, table, sizeof(label) - 1);
	strcat(label, "ReservedNonZero");

	for (i = 0; i < length; i++)
		value += data[i];

	if (value != 0) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM, label,
			"%4.4s %s field must be all zero, got below instead",
			table, field);

		for (i = 0; i < length; i++)
			fwts_log_info_verbatim(fw, "  %s [%2.2" PRId8 "] = 0x%2.2" PRIx8, field, i, data[i]);
	}
}

/*
 *  fwts_acpi_fixed_value()
 *  verify whether a field matches the designed fixed value
 */
void fwts_acpi_fixed_value(
	fwts_framework *fw,
	const fwts_log_level level,
	const char *table,
	const char *field,
	const uint8_t actual,
	const uint8_t must_be,
	bool *passed)
{
	char label[20];

	strncpy(label, table, 4);	/* ACPI table name is 4 char long */
	strncpy(label + 4, "BadFieldValue", sizeof(label) - 4);

	if (actual != must_be) {
		fwts_failed(fw, level, label,
			"%4.4s %s field must be %" PRIu8 ", got %" PRIu8 " instead.",
			table, field, must_be, actual);
		*passed = false;
	}
}

/*
 *  fwts_acpi_reserved_bits_()
 *  verify whether the reserved bits are zero
 */
void fwts_acpi_reserved_bits_(
	fwts_framework *fw,
	const char *table,
	const char *field,
	const uint64_t value,
	const uint8_t size,
	const uint8_t min,
	const uint8_t max,
	bool *passed)
{
	uint64_t mask = 0;
	uint8_t i;

	for (i = min; i <= max; i++) {
		mask |= (1ULL << i);
	}

	if (value & mask) {
		char label[24];
		strncpy(label, table, 4);	/* ACPI table name is 4 char long */
		strncpy(label + 4, "ReservedBitsNonZero", sizeof(label) - 4);

		switch (size) {
		case sizeof(uint8_t):
			fwts_failed(fw, LOG_LEVEL_HIGH, label,
				"%4.4s %s Bits [%" PRIu8 "..%" PRIu8 "] must be zero, got "
				"0x%2.2" PRIx8 " instead", table, field, max, min, (uint8_t)value);
			break;
		case sizeof(uint16_t):
			fwts_failed(fw, LOG_LEVEL_HIGH, label,
				"%4.4s %s Bits [%" PRIu8 "..%" PRIu8 "] must be zero, got "
				"0x%4.4" PRIx16 " instead", table, field, max, min, (uint16_t)value);
			break;
		case sizeof(uint32_t):
			fwts_failed(fw, LOG_LEVEL_HIGH, label,
				"%4.4s %s Bits [%" PRIu8 "..%" PRIu8 "] must be zero, got "
				"0x%8.8" PRIx32 " instead", table, field, max, min, (uint32_t)value);
			break;
		case sizeof(uint64_t):
			fwts_failed(fw, LOG_LEVEL_HIGH, label,
				"%4.4s %s Bits [%" PRIu8 "..%" PRIu8 "] must be zero, got "
				"0x%16.16" PRIx64 " instead", table, field, max, min, value);
			break;
		}
		*passed = false;
	}
}

/*
 *  fwts_acpi_reserved_type()
 *  verify whether the reserved types are used
 */
void fwts_acpi_reserved_type(
	fwts_framework *fw,
	const char *table,
	const uint8_t value,
	const uint8_t min,
	const uint8_t reserved,
	bool *passed)
{
	if (value < min || value >= reserved) {
		char label[20];

		strncpy(label, table, 4);	/* ACPI name is 4 char long */
		strncpy(label + 4, "BadSubTableType", sizeof(label) - 4);

		fwts_failed(fw, LOG_LEVEL_HIGH, label,
			"%4.4s must have subtable with Type %" PRIu8
			"..%" PRIu8 ", got %" PRIu8 " instead",
			table, min, reserved - 1, value);

		*passed = false;
	}
}

/*
 *  get_space_id_name()
 *  convert Generic Address Structure to a string, NULL if undefined
 */
static const char *get_space_id_name(const uint8_t id)
{
	/*
	 *  Space Id defined for Generic Address Structure (GAS)
	 *  See Table 5.1: Generic Address Structure (GAS) in ACPI spec
	 *  Also see fwts_acpi.h
	 */
	static const char *gas_space_id_names[] = {
		"System Memory (0x0)",
		"System I/O (0x1)",
		"PCI Configuration (0x2)",
		"Embedded Controller (0x3)",
		"SMBus (0x4)",
		"SystemCMOS (0x5)",
		"PciBarTarget (0x6)",
		"IPMI (0x7)",
		"General PurposeIO (0x8)",
		"GenericSerialBus (0x9)",
		"Platform Communications Channel (0xa)",
		"Platform Runtime Mechanism (0xb)"
	};

	if (id < FWTS_ARRAY_SIZE(gas_space_id_names))
		return gas_space_id_names[id];
	else if (id == 0x7f)
		return "Functional Fixed Hardware (0x7f)";
	else
		return NULL;
}

/*
 *  fwts_acpi_space_id()
 *  check whether gas space id matches
 */
void fwts_acpi_space_id(
	fwts_framework *fw,
	const char *table,
	const char *field,
	bool *passed,
	const uint8_t actual,
	const uint8_t num_type,
	...)
{
	bool matched = false;
	char must_be_id[255];
	uint16_t length = 0;
	uint8_t i, must_be;
	char label[25];
	va_list ap;

	strncpy(label, table, 4);	/* ACPI table name is 4 char long */
	strncpy(label + 4, "BadAddressSpaceId", sizeof(label) - 4);
	memset(must_be_id, 0, sizeof(must_be_id));

	va_start(ap, num_type);
	for (i = 0; i < num_type; i++) {
		const char *id_name;

		must_be = va_arg(ap, int);
		if (actual == must_be) {
			matched = true;
			break;
		}

		id_name = get_space_id_name(must_be);
		if (!id_name)
			continue;

		length += strlen(id_name);
		if (length >= sizeof(must_be_id))
			continue;

		strncat(must_be_id, id_name, strlen(id_name));
		if (i < num_type - 2)
			strncat(must_be_id, ", ", 3);
		else if (i == num_type - 2)
			strncat(must_be_id, " or ", 5);
	}

	if (!matched) {
		fwts_failed(fw, LOG_LEVEL_HIGH, label,
			"%4.4s %s Space ID must be one of %s, got %s instead.",
			table, field, must_be_id, get_space_id_name(actual));
		*passed = false;
	}

	 va_end(ap);
}

/*
 *  fwts_acpi_table_length()
 *  verify whether table length is sane
 */
bool fwts_acpi_table_length(
	fwts_framework *fw,
	const char *table,
	const uint32_t length,
	const uint32_t size)
{
	if (length < size) {
		char label[30];

		strncpy(label, table, 4);	/* ACPI name is 4 char long */
		strncpy(label + 4, "TooShort", sizeof(label) - 4);
		fwts_failed(fw, LOG_LEVEL_HIGH, label,
			"%4.4s table too short, expecting %2.2" PRIu8
			" bytes, instead got %2.2" PRIu8 " bytes",
			table, size, length);
		return false;
	}
	return true;
}

/*
 *  fwts_acpi_structure_length()
 *  verify whether sub structure length is sane
 */
bool fwts_acpi_structure_length(
	fwts_framework *fw,
	const char *table,
	const uint8_t type,
	const uint32_t actual_length,
	const uint32_t struct_length)
{
	if (actual_length != struct_length) {
		char label[30];

		strncpy(label, table, 4);	/* ACPI name is 4 char long */
		strncpy(label + 4, "BadStructureLength", sizeof(label) - 4);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, label,
			"%4.4s structure Type 0x%2.2" PRIx8 " should have "
			"length 0x%2.2" PRIx8 ", got 0x%2.2" PRIx8,
			table, type, struct_length, actual_length);
		return false;
	}
	return true;
}

/*
 *  fwts_acpi_structure_length_zero()
 *  verify whether sub structure length is zero
 */
bool fwts_acpi_structure_length_zero(
	fwts_framework *fw,
	const char *table,
	const uint16_t length,
	const uint32_t offset)
{
	if (length == 0) {
		char label[30];

		strncpy(label, table, 4);	/* ACPI name is 4 char long */
		strncpy(label + 4, "StructLengthZero", sizeof(label) - 4);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, label,
			"%4.4s structure (offset 0x%4.4" PRIx32 ") "
			"length cannot be 0", table, offset);
		return true;
	}
	return false;
}

/*
 *  fwts_acpi_structure_range()
 *  verify whether sub structure is out of table range
 */
bool fwts_acpi_structure_range(
	fwts_framework *fw,
	const char *table,
	const uint32_t table_length,
	const uint32_t offset)
{
	if (offset > table_length) {
		char label[30];

		strncpy(label, table, 4);	/* ACPI name is 4 char long */
		strncpy(label + 4, "BadTableLength", sizeof(label) - 4);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, label,
			"%4.4s has more subtypes than its size can handle", table);

		return true;
	}
	return false;
}

static uint32_t acpi_version;
/*
 *  fwts_get_acpi_version()
 * 	get acpi version from facp
 */
uint32_t fwts_get_acpi_version(fwts_framework *fw)
{
	fwts_acpi_table_fadt *fadt;
	fwts_acpi_table_info *table;
	uint8_t minor;

	if (acpi_version != 0)
		return acpi_version;

	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table FACP.");
		return 0;
	}

	if (table == NULL) {
		fwts_log_error(fw, "ACPI table FACP does not exist!");
		return 0;
	}

	fadt = (fwts_acpi_table_fadt *) table->data;
	minor = ((fadt->minor_version & 0xF) << 4) + ((fadt->minor_version & 0xF0) >> 4);
	acpi_version = (fadt->header.revision << 8) + minor;

	/* check FACP returns an invalid value */
	if (acpi_version > FWTS_ACPI_VERSION_NEXT ||
	    acpi_version < FWTS_ACPI_VERSION_10)
		return 0;

	return acpi_version;
}

/*
 *  fwts_get_fadt_version()
 * 	get fadt major and minor versions
 */
void fwts_get_fadt_version(fwts_framework *fw, uint8_t *major, uint8_t *minor)
{
	uint32_t acpi_version;

	/* ACPI version = FADT major & minor versions */
	acpi_version = fwts_get_acpi_version(fw);
	*major = (acpi_version & 0xF00) >> 8;
	*minor = (acpi_version & 0xFF) >> 4;
}
