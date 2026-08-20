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

#define FWTS_ACPI_DEVICE_HID "PNP0C0A"
#define FWTS_ACPI_DEVICE "battery"

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

static int acpi_battery_init(fwts_framework *fw)
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

static int method_test_BIF(fwts_framework *fw)
{
	ACPI_STATUS status;
	ACPI_HANDLE	method;

	/* _BIF can be superseded by _BIX */
	status = AcpiGetHandle (device, "_BIX", &method);
	if (ACPI_SUCCESS(status))
		return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_BIF", NULL, 0, fwts_method_test_BIF_return, NULL);
	else
		return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
			"_BIF", NULL, 0, fwts_method_test_BIF_return, NULL);
}


static int method_test_BIX(fwts_framework *fw)
{
	ACPI_STATUS status;
	ACPI_HANDLE	method;

	/* _BIX may not supported by older firmware */
	status = AcpiGetHandle (device, "_BIF", &method);
	if (ACPI_SUCCESS(status))
		return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_BIX", NULL, 0, fwts_method_test_BIX_return, NULL);
	else
		return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
			"_BIX", NULL, 0, fwts_method_test_BIX_return, NULL);
}

static int method_test_BMA(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];
	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;

	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_BMA", arg, 1, fwts_method_test_passed_failed_return, "_BMA");
}

static int method_test_BMS(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];
	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;

	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_BMS", arg, 1, fwts_method_test_passed_failed_return, "_BMS");
}

static int method_test_BPC(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_BPC", NULL, 0, fwts_method_test_BPC_return, NULL);
}

static int method_test_BPS(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_BPS", NULL, 0, fwts_method_test_BPS_return, NULL);
}

static int method_test_BPT(fwts_framework *fw)
{
	ACPI_OBJECT arg[3];
	uint64_t max = 5;
	int i;

	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;
	arg[2].Type = ACPI_TYPE_INTEGER;
	arg[2].Integer.Value = 0;
	for (i = 0; i <= 2; i++) {
		arg[1].Type = ACPI_TYPE_INTEGER;
		arg[1].Integer.Value = i;
		if (fwts_evaluate_method(fw, METHOD_OPTIONAL, &device, "_BPT", arg, 2,
			fwts_method_test_integer_max_return, &max) == FWTS_NOT_EXIST)
			break;
		fwts_log_nl(fw);
	}

	return FWTS_OK;
}

static int method_test_BST(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_BST", NULL, 0, fwts_method_test_BST_return, NULL);
}

static int method_test_BTH(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];
	uint8_t i, ret;
	arg[0].Type = ACPI_TYPE_INTEGER;

	for (i = 0; i <= 100; i++) {
		arg[0].Integer.Value = i;
		ret = fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_BTH", arg, 1, fwts_method_test_NULL_return, NULL);

		if (ret != FWTS_OK)
			break;
	}
	return ret;
}

static int method_test_BTP(fwts_framework *fw)
{
	static const int values[] = { 0, 1, 100, 200, 0x7fffffff };
	ACPI_STATUS status;
	uint8_t i;

	for (i = 0; i < FWTS_ARRAY_SIZE(values); i++) {
		ACPI_OBJECT arg[1];
		arg[0].Type = ACPI_TYPE_INTEGER;
		arg[0].Integer.Value = values[i];
		status = fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_BTP", arg, 1, fwts_method_test_NULL_return, NULL);
		if (ACPI_FAILURE(status))
			break;
	}
	return FWTS_OK;
}

static void method_test_PCL_return(fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_all_type(fw, name, obj, ACPI_TYPE_LOCAL_REFERENCE) != FWTS_OK)
		return;

	fwts_passed(fw,	"%s returned a sane package of %" PRIu32 " references.", name, obj->Package.Count);
}

static int method_test_PCL(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		 "_PCL", NULL, 0, method_test_PCL_return, NULL);
}

static int method_test_STA(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_STA", NULL, 0, fwts_method_test_STA_return, NULL);
}

static int method_test_BTM(fwts_framework *fw)
{
	static const int values[] = { 0, 1, 100, 200, 0x7fffffff };
	ACPI_STATUS status;
	uint8_t i;

	for (i = 0; i < FWTS_ARRAY_SIZE(values); i++) {
		ACPI_OBJECT arg[1];
		arg[0].Type = ACPI_TYPE_INTEGER;
		arg[0].Integer.Value = values[i];
		status = fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_BTM", arg, 1, fwts_method_test_NULL_return, NULL);
		if (ACPI_FAILURE(status))
			break;
	}
	return FWTS_OK;
}

static int method_test_BCT(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];
	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 50;	/* 50% */

	/*
	 * For now, just check that we get some integer back, values
	 * can be 0x00000000, 0x00000001-0xfffffffe and 0xffffffff,
	 * so anything is valid as long as it is an integer
	 */
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_BCT", arg, 1, fwts_method_test_integer_return, NULL);
}

static int method_test_BMD(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_BMD", NULL, 0, fwts_method_test_BMD_return, NULL);
}

static int method_test_BMC(fwts_framework *fw)
{
	static const int values[] = { 0, 1, 2, 4, 8};
	ACPI_STATUS status;
	uint8_t i;

	for (i = 0; i < FWTS_ARRAY_SIZE(values); i++) {
		ACPI_OBJECT arg[1];
		arg[0].Type = ACPI_TYPE_INTEGER;
		arg[0].Integer.Value = values[i];
		status = fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_BMC", arg, 1, fwts_method_test_NULL_return, NULL);
		if (ACPI_FAILURE(status))
			break;
	}
	return FWTS_OK;
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

static fwts_framework_minor_test acpi_battery_tests[] = {
	/* Device Specific Objects */
	{ method_test_BIF, "Test _BIF (Battery Information)." },
	{ method_test_BIX, "Test _BIX (Battery Information Extended)." },
	{ method_test_BMA, "Test _BMA (Battery Measurement Averaging)." },
	{ method_test_BMS, "Test _BMS (Battery Measurement Sampling Time)." },
	{ method_test_BPC, "Test _BPC (Battery Power Characteristics)." },
	{ method_test_BPS, "Test _BPS (Battery Power State)." },
	{ method_test_BPT, "Test _BPT (Battery Power Threshold)." },
	{ method_test_BPC, "Test _BPC (Battery Power Characteristics)." },
	{ method_test_BPS, "Test _BPS (Battery Power State)." },
	{ method_test_BPT, "Test _BPT (Battery Power Threshold)." },
	{ method_test_BST, "Test _BST (Battery Status)." },
	{ method_test_BTH, "Test _BTH (Battery Throttle Limit)." },
	{ method_test_BTP, "Test _BTP (Battery Trip Point)." },
	{ method_test_PCL, "Test _PCL (Power Consumer List)." },
	{ method_test_STA, "Test _STA (Status)." },
	{ method_test_BTM, "Test _BTM (Battery Time)." },
	{ method_test_BCT, "Test _BCT (Battery Charge Time)." },
	{ method_test_BMD, "Test _BMD (Battery Maintenance Data)." },
	{ method_test_BMC, "Test _BMC (Battery Maintenance Control)." },
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

static int acpi_battery_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	fwts_acpica_deinit();

	return FWTS_OK;
}

static fwts_framework_ops acpi_battery_ops = {
	.description = "ACPI battery device test",
	.init        = acpi_battery_init,
	.deinit      = acpi_battery_deinit,
	.minor_tests = acpi_battery_tests
};

FWTS_REGISTER("acpi_battery", &acpi_battery_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
