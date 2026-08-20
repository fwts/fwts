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

#define FWTS_ACPI_DEVICE_HID "ACPI0012"
#define FWTS_ACPI_DEVICE "NVDIMM"

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

static int acpi_nvdimm_init(fwts_framework *fw)
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

static int method_test_NCH(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NCH", NULL, 0, fwts_method_test_NCH_return, NULL);
}

static int method_test_NBS(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NBS", NULL, 0, fwts_method_test_NBS_return, NULL);
}

static int method_test_NIC(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NIC", NULL, 0, fwts_method_test_NIC_return, NULL);
}

static int method_test_NIH(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NIH", NULL, 0, fwts_method_test_NIH_return, NULL);
}

static int method_test_NIG(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NIG", NULL, 0, fwts_method_test_NIG_return, NULL);
}

/* Evaluate Device Identification Objects - all are optional */
static int method_test_HID(fwts_framework *fw)
{
	return fwts_method_test_HID(fw, &device);
}

static fwts_framework_minor_test acpi_nvdimm_tests[] = {
	/* Device Specific Objects */
	{ method_test_NCH, "Test _NCH (NVDIMM Current Health Information)." },
	{ method_test_NBS, "Test _NBS (NVDIMM Boot Status)." },
	{ method_test_NIC, "Test _NIC (NVDIMM Health Error Injection Capabilities)." },
	{ method_test_NIH, "Test _NIH (NVDIMM Inject/Clear Health Errors)." },
	{ method_test_NIG, "Test _NIG (NVDIMM Inject Health Error Status)." },
	/* Device Identification Objects - all are optional */
	{ method_test_HID, "Test _HID (Hardware ID)." },
	{ NULL, NULL }
};

static int acpi_nvdimm_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	fwts_acpica_deinit();

	return FWTS_OK;
}

static fwts_framework_ops acpi_nvdimm_ops = {
	.description = "NVDIMM device test",
	.init        = acpi_nvdimm_init,
	.deinit      = acpi_nvdimm_deinit,
	.minor_tests = acpi_nvdimm_tests
};

FWTS_REGISTER("acpi_nvdimm", &acpi_nvdimm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI)

#endif
