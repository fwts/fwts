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

#ifndef __FWTS_ACPI_TABLES_H__
#define __FWTS_ACPI_TABLES_H__

#include "fwts.h"

#define ACPI_MAX_TABLES		(128)

#define fwts_acpi_revision_check(table, actual, must_be, passed) \
	fwts_acpi_fixed_value(fw, LOG_LEVEL_HIGH, table, "Revision", actual, must_be, passed)

#define fwts_acpi_reserved_bits(table, field, value, min, max, passed) \
	fwts_acpi_reserved_bits_(fw, table, field, value, sizeof(value), min, max, passed)

#define fwts_acpi_reserved_zero(table, field, value, passed) \
	fwts_acpi_reserved_zero_(fw, table, field, value, sizeof(value), passed)

typedef enum {
	FWTS_ACPI_TABLE_FROM_FIRMWARE,	/* directly from firmware */
	FWTS_ACPI_TABLE_FROM_FILE,	/* loaded from file, e.g. from acpidump */
	FWTS_ACPI_TABLE_FROM_FIXUP	/* auto-generated fixup by fwts */
} fwts_acpi_table_provenance;

typedef struct {
	char	   name[5];
	void       *data;
	size_t	   length;
	uint32_t   which;
	uint32_t   index;
	bool	   has_aml;
	uint64_t   addr;
	fwts_acpi_table_provenance provenance;
} fwts_acpi_table_info;

int acpi_table_generic_init(fwts_framework *fw, char *name, fwts_acpi_table_info **table);

#define acpi_table_init(name, table)				\
static int name ## _init (fwts_framework *fw)			\
{								\
	return acpi_table_generic_init(fw, # name, table);	\
}

int fwts_acpi_load_tables(fwts_framework *fw);
int fwts_acpi_free_tables(void);

int fwts_acpi_find_table(fwts_framework *fw, const char *name, const uint32_t which,
	fwts_acpi_table_info **info);
int fwts_acpi_find_table_by_addr(fwts_framework *fw, const uint64_t addr,
	fwts_acpi_table_info **info);
int fwts_acpi_get_table(fwts_framework *fw, const uint32_t index, fwts_acpi_table_info **info);
bool fwts_acpi_obj_find(fwts_framework *fw, const char *obj_name);

fwts_bool fwts_acpi_is_reduced_hardware(fwts_framework *fw);

void fwts_acpi_reserved_zero_(fwts_framework *fw, const char *table, const char *field,
	const uint64_t value, const uint8_t size, bool *passed);
void fwts_acpi_reserved_zero_array(fwts_framework *fw, const char *table, const char *field,
	const uint8_t* data, const uint8_t length, bool *passed);
void fwts_acpi_reserved_bits_(fwts_framework *fw, const char *table, const char *field,
	const uint64_t value, const uint8_t size, const uint8_t min, const uint8_t max, bool *passed);
void fwts_acpi_reserved_type(fwts_framework *fw, const char *table, const uint8_t value,
	const uint8_t min, const uint8_t reserved, bool *passed);
bool fwts_acpi_table_length(fwts_framework *fw, const char *table, const uint32_t length,
	const uint32_t size);
bool fwts_acpi_structure_length(fwts_framework *fw, const char *table, const uint8_t type,
	const uint32_t actual_length, const uint32_t struct_length);
bool fwts_acpi_structure_length_zero(fwts_framework *fw, const char *table,
	const uint16_t length, const uint32_t offset);
bool fwts_acpi_structure_range(fwts_framework *fw, const char *table, const uint32_t table_length,
	const uint32_t offset);
void fwts_acpi_fixed_value(fwts_framework *fw, const fwts_log_level level, const char *table,
	const char *field, const uint8_t actual, const uint8_t must_be, bool *passed);
void fwts_acpi_space_id(fwts_framework *fw, const char *table, const char *field, bool *passed,
	const uint8_t actual, const uint8_t num_type, ...);

uint32_t fwts_get_acpi_version(fwts_framework *fw);
void fwts_get_fadt_version(fwts_framework *fw, uint8_t *major, uint8_t *minor);

#endif
