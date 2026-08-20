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

#define FWTS_ACPI_DEVICE_HID "ACPI0008"
#define FWTS_ACPI_DEVICE "ambient light sensor"

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

static int ambient_light_init(fwts_framework *fw)
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

static int method_test_ALI(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_ALI", NULL, 0, fwts_method_test_integer_return, NULL);
}

static int method_test_ALC(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_ALC", NULL, 0, fwts_method_test_integer_return, NULL);
}

static int method_test_ALT(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_ALT", NULL, 0, fwts_method_test_integer_return, NULL);
}

static void method_test_ALR_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint32_t i;
	bool failed = false;
	uint32_t adjustment = 0, illuminance = 0;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	/* Could be one or more sub-packages */
	for (i = 0; i < obj->Package.Count; i++) {
		ACPI_OBJECT *pkg;

		pkg = &obj->Package.Elements[i];
		if (pkg->Package.Count != 2) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_ALRBadSubPackageElementCount",
				"%s sub-package %" PRIu32 " was expected to "
				"have 2 elements, got %" PRIu32 " elements instead.",
				name, i, pkg->Package.Count);
			failed = true;
		} else {
			/* elements should be listed in monotonically increasing order */
			if (pkg->Package.Elements[0].Type != ACPI_TYPE_INTEGER ||
			    adjustment > pkg->Package.Elements[0].Integer.Value) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"Method_ALRBadSubPackageReturnType",
					"%s sub-package %" PRIu32
					" element 0 is an invalid integer.",
					name, i);
				failed = true;
			}

			if (pkg->Package.Elements[1].Type != ACPI_TYPE_INTEGER ||
			    illuminance > pkg->Package.Elements[1].Integer.Value) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"Method_ALRBadSubPackageReturnType",
					"%s sub-package %" PRIu32
					" element 1 is an invalid integer.",
					name, i);
				failed = true;
			}
			adjustment = pkg->Package.Elements[0].Integer.Value;
			illuminance = pkg->Package.Elements[1].Integer.Value;
		}
	}

	if (!failed)
		fwts_method_passed_sane(fw, name, "package");
}

static int method_test_ALR(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_ALR", NULL, 0, method_test_ALR_return, NULL);
}

static int method_test_ALP(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_ALP", NULL, 0, fwts_method_test_polling_return, "_ALP");
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

static int method_test_HID(fwts_framework *fw)
{
	return fwts_method_test_HID(fw, &device);
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

static fwts_framework_minor_test ambient_light_tests[] = {
	/* Device Specific Objects */
	{ method_test_ALI, "Test _ALI (Ambient Light Illuminance)." },
	{ method_test_ALC, "Test _ALC (Ambient Light Colour Chromaticity)." },
	{ method_test_ALT, "Test _ALT (Ambient Light Temperature)." },
	{ method_test_ALR, "Test _ALR (Ambient Light Response)."},
	{ method_test_ALP, "Test _ALP (Ambient Light Polling)."},
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

static int ambient_light_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	fwts_acpica_deinit();

	return FWTS_OK;
}

static fwts_framework_ops ambient_light_ops = {
	.description = "Ambient light sensor device test",
	.init        = ambient_light_init,
	.deinit      = ambient_light_deinit,
	.minor_tests = ambient_light_tests
};

FWTS_REGISTER("acpi_als", &ambient_light_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
