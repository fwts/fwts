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

#ifndef __FWTS_ACPI_METHOD_H__
#define __FWTS_ACPI_METHOD_H__

#include "fwts.h"

/* acpica headers */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "acpi.h"
#pragma GCC diagnostic error "-Wunused-parameter"

int fwts_acpi_init(fwts_framework *fw);
int fwts_acpi_deinit(fwts_framework *fw);
char *fwts_acpi_object_exists(const char *name);
fwts_list *fwts_acpi_object_get_names(void);
void fwts_acpi_object_dump(fwts_framework *fw, const ACPI_OBJECT *obj);
void fwts_acpi_object_evaluate_report_error(fwts_framework *fw,
	const char *name, const ACPI_STATUS status);
ACPI_STATUS fwts_acpi_object_evaluate(fwts_framework *fw, char *name,
	ACPI_OBJECT_LIST *arg_list, ACPI_BUFFER *buf);

/* Test types */
#define METHOD_MANDATORY	1
#define METHOD_OPTIONAL		2
#define METHOD_MOBILE		4
#define METHOD_SILENT		8

#define ACPI_TYPE_INTBUF	(ACPI_TYPE_INVALID + 1)

typedef void (*fwts_method_return)(fwts_framework *fw, char *name,
	ACPI_BUFFER *ret_buff, ACPI_OBJECT *ret_obj, void *private);

typedef struct {
	ACPI_OBJECT_TYPE type;	/* Type */
	const char 	*name;	/* Field name */
} fwts_package_element;

/* Time format for ACPI000E (Time and Alarm Device) */
typedef struct {
	uint16_t	year;
	uint8_t		month;
	uint8_t		day;
	uint8_t		hour;
	uint8_t		minute;
	uint8_t		second;
	uint8_t		valid;	// pad1 for _SRT
	uint16_t	milliseconds;
	uint16_t	timezone;
	uint8_t		daylight;
	uint8_t		pad2[3];
} __attribute__ ((packed)) fwts_acpi_time_buffer;

/* Returns for NVDIMM control methods */
typedef struct {
	uint16_t	status;
	uint16_t	extended_status;
	uint16_t	validation_flags;
	uint32_t	health_status_flags;
	uint32_t	health_status_attributes;
	uint8_t		reserved[50];
}  __attribute__ ((packed)) nch_return_t;

typedef struct {
	uint16_t	status;
	uint16_t	extended_status;
	uint16_t	validation_flags;
	uint32_t	data_loss_count;
	uint8_t		reserved[54];
}  __attribute__ ((packed)) nbs_return_t;

typedef struct {
	uint16_t	status;
	uint16_t	extended_status;
	uint32_t	health_error_injection;
	uint32_t	health_status_attributes;
	uint8_t		reserved[52];
}  __attribute__ ((packed)) nic_return_t;

typedef struct {
	uint16_t	status;
	uint16_t	extended_status;
}  __attribute__ ((packed)) nih_return_t;

typedef struct {
	uint16_t	status;
	uint16_t	extended_status;
	uint16_t	validation_flags;
	uint32_t	health_status_errors;
	uint32_t	health_status_attributes;
	uint8_t		reserved[50];
}  __attribute__ ((packed)) nig_return_t;

#define fwts_method_check_type(fw, name, buf, type) 			\
	fwts_method_check_type__(fw, name, buf, type, #type)

#define fwts_method_package_elements_type(fw, name, obj, info)	\
	fwts_method_package_elements_type__(fw, name, obj, info, FWTS_ARRAY_SIZE(info))

int fwts_method_check_type__(fwts_framework *fw, const char *name,
	const ACPI_BUFFER *buf, const ACPI_OBJECT_TYPE type,
	const char *type_name);
int fwts_method_check_element_type(fwts_framework *fw, const char *name,
	const ACPI_OBJECT *obj, const uint32_t subpkg,
	const uint32_t element, const ACPI_OBJECT_TYPE type);
const char *fwts_method_type_name(const ACPI_OBJECT_TYPE type);
void fwts_method_passed_sane(fwts_framework *fw, const char *name,
	const char *type);
void fwts_method_passed_sane_uint64(fwts_framework *fw, const char *name,
	const uint64_t value);
void fwts_method_failed_null_object(fwts_framework *fw, const char *name,
	const char *type);
bool fwts_method_type_matches(const ACPI_OBJECT_TYPE t1,
	const ACPI_OBJECT_TYPE t2);
int fwts_method_buffer_size(fwts_framework *fw, const char *name,
	const ACPI_OBJECT *obj, const size_t buf_size);
int fwts_method_package_count_min(fwts_framework *fw, const char *name,
	const ACPI_OBJECT *obj, const uint32_t min);
int fwts_method_package_count_equal(fwts_framework *fw, const char *name,
	const ACPI_OBJECT *obj, const uint32_t count);
int fwts_method_subpackage_count_equal(fwts_framework *fw, const char *name,
	const ACPI_OBJECT *obj, const uint32_t sub, const uint32_t count);
int fwts_method_package_elements_all_type(fwts_framework *fw,
	const char *name, const ACPI_OBJECT *obj, const ACPI_OBJECT_TYPE type);
int fwts_method_package_elements_type__(fwts_framework *fw, const char *name,
	const ACPI_OBJECT *obj, const fwts_package_element *info,
	const uint32_t count);
int fwts_method_test_revision(fwts_framework *fw, const char *name,
	const uint32_t cur_revision, const uint32_t spec_revision);

void fwts_method_test_integer_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_string_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_reference_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_NULL_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_buffer_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj,void *private);
void fwts_method_test_all_reference_package_return(fwts_framework *fw,
	char *name, ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_integer_reserved_bits_return(fwts_framework *fw,
	char *name, ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_integer_max_return(fwts_framework *fw,
	char *name, ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_package_integer_return(fwts_framework *fw,
	char *name, ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_passed_failed_return(fwts_framework *fw,
	char *name, ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_polling_return( fwts_framework *fw,
	char *name, ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);

void method_evaluate_found_method(fwts_framework *fw,
	char *name, fwts_method_return check_func, void *private,
	ACPI_OBJECT_LIST *arg_list);
void fwts_evaluate_found_method(fwts_framework *fw, ACPI_HANDLE *parent,
	char *name, fwts_method_return check_func, void *private,
	ACPI_OBJECT_LIST *arg_list);
int fwts_evaluate_method(fwts_framework *fw, const uint32_t test_type,
	ACPI_HANDLE *parent, char *name, ACPI_OBJECT *args,
	uint32_t num_args, fwts_method_return check_func, void *private);

bool fwts_method_valid_HID_string(const char *str);
bool fwts_method_valid_EISA_ID(const uint32_t id, char *buf,
	const size_t buf_len);

void fwts_method_test_CRS_large_size(fwts_framework *fw, const char *name,
	const char *objname, const uint8_t *data, const size_t crs_length,
	const size_t min, const size_t max, bool *passed);
void fwts_method_test_CRS_large_resource_items(fwts_framework *fw,
	const char *name, const char *objname, const uint8_t *data,
	const uint64_t length, bool *passed, const char **tag);
void fwts_method_test_CRS_small_resource_items(fwts_framework *fw,
	const char *name, const char *objname, const uint8_t *data,
	const size_t length, bool *passed, const char **tag) ;

/* Device Identification Objects - see Section 6 Device Configuration */
int fwts_method_test_ADR(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_CID(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_CLS(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_DDN(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_HID(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_HRV(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_MLS(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_PLD(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_SUB(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_SUN(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_STR(fwts_framework *fw, ACPI_HANDLE *device);
int fwts_method_test_UID(fwts_framework *fw, ACPI_HANDLE *device);

void fwts_method_test_BMD_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_BPC_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_BPS_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_valid_CID_Type(fwts_framework *fw, char *name,
	ACPI_OBJECT *obj);
void fwts_method_test_CID_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_HID_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_MLS_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_PLD_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_SUB_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_UID_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);

void fwts_method_test_NBS_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_NCH_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_NIC_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_NIH_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_NIG_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);

void fwts_method_test_STA_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_BIF_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_BIX_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);
void fwts_method_test_BST_return(fwts_framework *fw, char *name,
	ACPI_BUFFER *buf, ACPI_OBJECT *obj, void *private);

#endif
