/*
 * Copyright (C) 2017-2026 Canonical
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

#include "fwts_acpi_object_eval.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#define FWTS_ACPI_DEVICE_HID "PNP0C09"
#define FWTS_ACPI_DEVICE "EC"

static ACPI_HANDLE device;

static ACPI_STATUS get_device_handle(ACPI_HANDLE handle, uint32_t level,
					  void *context, void **ret_val)
{
	FWTS_UNUSED(level);
	FWTS_UNUSED(context);
	FWTS_UNUSED(ret_val);

	device = handle;
	return AE_CTRL_TERMINATE;
}

static int acpi_ec_init(fwts_framework *fw)
{
	ACPI_STATUS status;

	if (fwts_acpica_init(fw) != FWTS_OK)
		return FWTS_ERROR;

	status = AcpiGetDevices(FWTS_ACPI_DEVICE_HID, get_device_handle, NULL, NULL);
	if (ACPI_FAILURE(status)) {
		fwts_log_error(fw, "Cannot find the ACPI device");
		return FWTS_ERROR;
	}

	if (!device) {
		fwts_log_error(fw, "ACPI %s device does not exist, skipping test", FWTS_ACPI_DEVICE);
		fwts_acpica_deinit();
		return FWTS_SKIP;
	} else {
		ACPI_BUFFER buffer;
		char full_name[128];

		buffer.Length = sizeof(full_name);
		buffer.Pointer = full_name;

		status = AcpiGetName(device, ACPI_FULL_PATHNAME, &buffer);
		if (ACPI_SUCCESS(status)) {
			fwts_log_info_verbatim(fw, "ACPI %s device: %s", FWTS_ACPI_DEVICE, full_name);
			fwts_log_nl(fw);
		}
	}

	return FWTS_OK;
}

static int method_test_HID(fwts_framework *fw)
{
	return fwts_method_test_HID(fw, &device);
}

static void method_test_GPE_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);
	FWTS_UNUSED(buf);
	bool failed = false;

	if (fwts_acpi_is_reduced_hardware(fw)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"MethodGPEExist",
			"%s is not required for Hardware-reduced ACPI platforms",
			name);
		return;
	}

	switch (obj->Type) {
	case ACPI_TYPE_INTEGER:
		if (obj->Integer.Value <= 255)
			fwts_passed(fw, "%s returned an integer 0x%8.8" PRIx64,
				name, (uint64_t)obj->Integer.Value);
		else
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"MethodGPEBadInteger",
				"%s returned an invalid integer 0x%8.8" PRIx64,
				name, (uint64_t)obj->Integer.Value);
		break;
	case ACPI_TYPE_PACKAGE:
		if (obj->Package.Elements[0].Type != ACPI_TYPE_LOCAL_REFERENCE) {
			failed = true;
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodGPEBadPackage",
			"%s sub-package element 0 is not a reference.",	name);
		}

		if (obj->Package.Elements[1].Type != ACPI_TYPE_INTEGER) {
			failed = true;
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodGPEBadPackage",
			"%s sub-package element 1 is not an integer.", name);
		}

		if (!failed)
			fwts_method_passed_sane(fw, name, "package");

		break;
	default:
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodGPEBadReturnType",
			"%s did not return an integer or a package.", name);
		break;
	}
}

static int method_test_GPE(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_GPE", NULL, 0, method_test_GPE_return, "_GPE");
}

static void method_test_CRS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint8_t *data;
	bool passed = true;
	const char *tag = "Unknown";
	char *objname = (char*)private;
	char tmp[128];

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;
	if (obj->Buffer.Pointer == NULL) {
		snprintf(tmp, sizeof(tmp), "Method%sNullBuffer", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s returned a NULL buffer pointer.", name);
		return;
	}
	if (obj->Buffer.Length < 1) {
		snprintf(tmp, sizeof(tmp), "Method%sBufferTooSmall", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return a buffer of at least one byte in length.", name);
		return;
	}

	data = (uint8_t*)obj->Buffer.Pointer;

	if (data[0] & 128)
		fwts_method_test_CRS_large_resource_items(fw, name, objname, data, obj->Buffer.Length, &passed, &tag);
	else
		fwts_method_test_CRS_small_resource_items(fw, name, objname, data, obj->Buffer.Length, &passed, &tag);

	if (passed)
		fwts_passed(fw, "%s (%s) looks sane.", name, tag);
}

static int method_test_CRS(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_CRS", NULL, 0, method_test_CRS_return, NULL);
}

/* Evaluate Device Identification Objects - all are optional */
static int method_test_ADR(fwts_framework *fw)
{
	return fwts_method_test_ADR(fw, &device);
}

static int method_test_CID(fwts_framework *fw)
{
	return fwts_method_test_CID(fw, &device);
}

static int method_test_CLS(fwts_framework *fw)
{
	return fwts_method_test_CLS(fw, &device);
}

static int method_test_DDN(fwts_framework *fw)
{
	return fwts_method_test_DDN(fw, &device);
}

static int method_test_HRV(fwts_framework *fw)
{
	return fwts_method_test_HRV(fw, &device);
}

static int method_test_MLS(fwts_framework *fw)
{
	return fwts_method_test_MLS(fw, &device);
}

static int method_test_PLD(fwts_framework *fw)
{
	return fwts_method_test_PLD(fw, &device);
}

static int method_test_SUB(fwts_framework *fw)
{
	return fwts_method_test_SUB(fw, &device);
}

static int method_test_SUN(fwts_framework *fw)
{
	return fwts_method_test_SUN(fw, &device);
}

static int method_test_STR(fwts_framework *fw)
{
	return fwts_method_test_STR(fw, &device);
}

static int method_test_UID(fwts_framework *fw)
{
	return fwts_method_test_UID(fw, &device);
}

static fwts_framework_minor_test acpi_ec_tests[] = {
	/* Device Specific Objects */
	{ method_test_CRS, "Test _CRS (Current Resource Settings)." },
	{ method_test_HID, "Test _HID (Hardware ID)." },
	{ method_test_GPE, "Test _GPE (General Purpose Events)." },
	/* Device Identification Objects - all are optional */
	{ method_test_ADR, "Test _ADR (Return Unique ID for Device)." },
	{ method_test_CID, "Test _CID (Compatible ID)." },
	{ method_test_CLS, "Test _CLS (Class Code)." },
	{ method_test_DDN, "Test _DDN (DOS Device Name)." },
	{ method_test_HID, "Test _HID (Hardware ID)." },
	{ method_test_HRV, "Test _HRV (Hardware Revision Number)." },
	{ method_test_MLS, "Test _MLS (Multiple Language String)." },
	{ method_test_PLD, "Test _PLD (Physical Device Location)." },
	{ method_test_SUB, "Test _SUB (Subsystem ID)." },
	{ method_test_SUN, "Test _SUN (Slot User Number)." },
	{ method_test_STR, "Test _STR (String)." },
	{ method_test_UID, "Test _UID (Unique ID)." },
	{ NULL, NULL }
};

static int acpi_ec_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	fwts_acpica_deinit();

	return FWTS_OK;
}

static fwts_framework_ops acpi_ec_ops = {
	.description = "ACPI embedded controller device test",
	.init        = acpi_ec_init,
	.deinit      = acpi_ec_deinit,
	.minor_tests = acpi_ec_tests
};

FWTS_REGISTER("acpi_ec", &acpi_ec_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
