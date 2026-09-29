/*
 * Copyright (C) 2026 Xiang W <wangxiang@iscas.ac.cn>
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

#if defined(FWTS_HAS_ACPI) && defined(FWTS_ARCH_RISCV)

#include "fwts_acpi_object_eval.h"

/*
 * RISC-V BRS ACPI_040
 *
 * The PCI Memory-mapped Configuration Space (MCFG) table MUST NOT be
 * present if it violates the PCI Firmware Specification.
 *
 * BRS also requires that ECAM-compatible segments described in MCFG
 * are exposed with the standard AML IDs:
 *   _HID PNP0A08, _CID PNP0A03
 * so a generic OS can treat them as standard ECAM without quirks.
 *
 * Table length, empty-table and allocation-record sizing are already
 * covered by the generic "mcfg" test (src/acpi/mcfg/mcfg.c).
 */

#define ECAM_BUS_SIZE		(1ULL << 20)	/* 1 MiB per bus */

/* Standard AML IDs for an ECAM-capable PCI host bridge. */
#define HID_ECAM		"PNP0A08"
#define CID_PCI			"PNP0A03"

static fwts_acpi_table_info *table;

typedef struct {
	bool		found;
	bool		has_cba;
	uint64_t	cba;
} mcfg_pci_match;

static int mcfg_brsi_init(fwts_framework *fw)
{
	int rc;

	rc = acpi_table_generic_init(fw, "MCFG", &table);
	/* MCFG is optional on BRS-I. */
	if (table == NULL || table->length == 0)
		return FWTS_OK;

	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}

	return rc;
}

static int mcfg_brsi_deinit(fwts_framework *fw)
{
	return fwts_acpi_deinit(fw);
}

static ACPI_OBJECT *mcfg_eval_object(ACPI_HANDLE device, char *method)
{
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	if (ACPI_FAILURE(AcpiEvaluateObject(device, method, NULL, &buf)))
		return NULL;

	return buf.Pointer;
}

static bool mcfg_eval_integer(
	ACPI_HANDLE device,
	char *method,
	uint64_t *value)
{
	bool rc = false;
	ACPI_OBJECT *obj;

	obj = mcfg_eval_object(device, method);

	if (obj && (obj->Type == ACPI_TYPE_INTEGER)) {
		rc = true;
		if (value)
			*value = obj->Integer.Value;
	}

	free(obj);
	return rc;
}

static bool mcfg_id_to_str(const ACPI_OBJECT *obj, char *out, size_t out_len)
{
	if (!obj || !out || out_len < 8)
		return false;

	out[0] = 0;

	if (obj->Type == ACPI_TYPE_STRING) {
		if (!obj->String.Pointer)
			return false;
		snprintf(out, out_len, "%s", obj->String.Pointer);
		return true;
	}

	if (obj->Type == ACPI_TYPE_INTEGER)
		return fwts_method_valid_EISA_ID((uint32_t)obj->Integer.Value, out, out_len);

	return false;
}

static bool mcfg_id_is(const ACPI_OBJECT *obj, const char *id)
{
	char buf[16];

	if (!mcfg_id_to_str(obj, buf, sizeof(buf)))
		return false;

	return strcmp(buf, id) == 0;
}

static bool mcfg_cid_contains(const ACPI_OBJECT *obj, const char *id)
{
	uint32_t i;

	if (!obj)
		return false;

	if (obj->Type != ACPI_TYPE_PACKAGE)
		return mcfg_id_is(obj, id);

	for (i = 0; i < obj->Package.Count; i++) {
		if (mcfg_id_is(&obj->Package.Elements[i], id))
			return true;
	}

	return false;
}

/*
 * AcpiGetDevices() callback.
 *
 * Context      - fwts_acpi_mcfg_configuration
 * ReturnValue  - mcfg_pci_match *
 *
 * A single device matches only when all of:
 *   _HID  PNP0A08
 *   _CID  contains PNP0A03
 *   _SEG  present and == pci_segment_group_number
 *
 * AE_OK             - not a match, keep looking
 * AE_CTRL_TERMINATE - found; result in ReturnValue
 */
static ACPI_STATUS mcfg_match_pci_device(
	ACPI_HANDLE object,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	fwts_acpi_mcfg_configuration *config = context;
	mcfg_pci_match *match = return_value ? *return_value : NULL;
	ACPI_OBJECT *hid_obj;
	ACPI_OBJECT *cid_obj;
	bool match_cid;
	bool match_hid;
	uint64_t seg;

	FWTS_UNUSED(nesting_level);

	if (!config || !match)
		return AE_OK;

	/* Confirm _HID is PNP0A08 */
	hid_obj = mcfg_eval_object(object, "_HID");
	if (!hid_obj)
		return AE_OK;
	match_hid = mcfg_id_is(hid_obj, HID_ECAM);
	free(hid_obj);
	if (!match_hid)
		return AE_OK;

	/* Confirm _CID is PNP0A03 */
	cid_obj = mcfg_eval_object(object, "_CID");
	if (!cid_obj)
		return AE_OK;
	match_cid = mcfg_cid_contains(cid_obj, CID_PCI);
	free(cid_obj);
	if (!match_cid)
		return AE_OK;

	seg = 0; /* default _SEG is zero */
	mcfg_eval_integer(object, "_SEG", &seg);
	if ((uint16_t)seg != config->pci_segment_group_number)
		return AE_OK;

	match->found = true;
	match->has_cba = mcfg_eval_integer(object, "_CBA", &match->cba);

	return AE_CTRL_TERMINATE;
}

static int mcfg_brsi_test1(fwts_framework *fw)
{
	fwts_acpi_table_mcfg *mcfg;
	fwts_acpi_mcfg_configuration *config;
	int nr, i;
	bool passed = true;

	if (table == NULL || table->length == 0) {
		fwts_passed(fw,
			"MCFG is not present. RISC-V BRS ACPI_040 allows "
			"omitting MCFG when there is no ECAM-compatible "
			"PCIe segment to describe.");
		return FWTS_OK;
	}

	/*
	 * Length / record-count validation lives in src/acpi/mcfg/mcfg.c.
	 * Bail out quietly if the table is too truncated to parse.
	 */
	if (table->length < sizeof(fwts_acpi_table_mcfg))
		return FWTS_OK;

	mcfg = (fwts_acpi_table_mcfg *)table->data;
	nr = (table->length - sizeof(fwts_acpi_table_mcfg)) /
	     sizeof(fwts_acpi_mcfg_configuration);
	if (nr <= 0) {
		fwts_skipped(fw, "MCFG has no allocation entries to match.");
		return FWTS_OK;
	}

	config = &mcfg->configuration[0];
	for (i = 0; i < nr; i++, config++) {
		mcfg_pci_match match;
		void *retval;

		/*
		 * PCI Firmware Specification ECAM: base is bus 0 / device 0 /
		 * function 0 of the segment. Each bus is 1 MiB, so the base
		 * must be 1 MiB aligned.
		 */
		if (config->base_address & (ECAM_BUS_SIZE - 1)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"MCFG entry #%d base address 0x%" PRIx64
				" is not 1 MiB aligned.",
				i, config->base_address);
		}

		memset(&match, 0, sizeof(match));
		retval = &match;

		AcpiGetDevices(HID_ECAM, mcfg_match_pci_device,
					config, &retval);

		if (!match.found) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"MCFG entry #%d segment %" PRIu16
				" (buses %" PRIu8 "-%" PRIu8 ", base 0x%" PRIx64
				") has no host bridge with _HID %s, _CID %s "
				"and matching _SEG.",
				i, config->pci_segment_group_number,
				config->start_bus_number,
				config->end_bus_number,
				config->base_address,
				HID_ECAM, CID_PCI);
			continue;
		}

		fwts_log_info(fw,
			"MCFG entry #%d segment %" PRIu16
			" buses %" PRIu8 "-%" PRIu8 " base 0x%" PRIx64
			" matches a host bridge with _HID %s, _CID %s "
			"and _SEG.",
			i, config->pci_segment_group_number,
			config->start_bus_number, config->end_bus_number,
			config->base_address,
			HID_ECAM, CID_PCI);

		if (match.has_cba && match.cba != config->base_address) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"Host bridge _CBA 0x%" PRIx64
				" does not match MCFG entry #%d base "
				"address 0x%" PRIx64 ".",
				match.cba, i, config->base_address);
		}
	}

	if (passed)
		fwts_passed(fw,
			"MCFG allocations are 1 MiB aligned and each matches "
			"a standard %s/%s host bridge (_SEG, and _CBA when "
			"present).",
			HID_ECAM, CID_PCI);
	else
		fwts_advice(fw,
			"RISC-V BRS requires ECAM-compatible implementations "
			"to be exposed using MCFG together with _HID %s and "
			"_CID %s. Do not publish MCFG for a vendor-specific "
			"bridge, and do not rely on MCFG table-header quirks.",
			HID_ECAM, CID_PCI);

	return FWTS_OK;
}

static fwts_framework_minor_test mcfg_brsi_tests[] = {
	{ mcfg_brsi_test1,
	  "Check MCFG ACPI_040 ECAM alignment and PNP0A08/PNP0A03 pairing." },
	{ NULL, NULL }
};

static fwts_framework_ops mcfg_brsi_ops = {
	.description = "RISC-V BRS-I MCFG PCI Memory-mapped Configuration Space test.",
	.init        = mcfg_brsi_init,
	.deinit      = mcfg_brsi_deinit,
	.minor_tests = mcfg_brsi_tests
};

FWTS_REGISTER("mcfg_brsi", &mcfg_brsi_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BRSI)

#endif
