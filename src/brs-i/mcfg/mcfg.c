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
#include "fwts_log.h"

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
 */


/* Standard AML IDs for an ECAM-capable PCI host bridge. */
#define HID_ECAM		"PNP0A08"
#define CID_PCI			"PNP0A03"

static bool acpi_initialized;
static fwts_acpi_table_info *table;
static fwts_list *memory_map_list;

static int mcfg_brsi_init(fwts_framework *fw)
{
	if (fwts_acpi_find_table(fw, "MCFG", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI tables.");
		return FWTS_ERROR;
	}

	if (fwts_acpi_init(fw) != FWTS_OK) {
		fwts_log_error(fw, "Cannot initialise ACPI.");
		return FWTS_ERROR;
	}
	acpi_initialized = true;

	memory_map_list = fwts_memory_map_table_load(fw);
	if (memory_map_list == NULL) {
		fwts_log_warning(fw, "No memory map table found");
	}

	return FWTS_OK;
}

static int mcfg_brsi_deinit(fwts_framework *fw)
{
	if (memory_map_list)
		fwts_memory_map_table_free(memory_map_list);

	if (acpi_initialized)
		fwts_acpi_deinit(fw);

	return FWTS_OK;
}

static int compare_config_space(
	fwts_framework *fw,
	const fwts_acpi_mcfg_configuration *config)
{
	uint8_t *mapped_config_space;
	uint8_t config_space[16];
	size_t page_size, n;
	bool match;
	char path[PATH_MAX];
	FILE *fp;
	int i;

	page_size = fwts_page_size();

	/*
	 * Sanity check on first config, this is enough to
	 * see if MMIO base is OK or not
	 */
	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/%4.4" PRIx16 ":00:00.0/config",
		config->pci_segment_group_number);

	if ((fp = fopen(path, "r")) == NULL) {
		fwts_warning(fw, "Could not open %s.", path);
		return FWTS_ERROR;
	}
	n = fread(config_space, 1, sizeof(config_space), fp);
	(void)fclose(fp);
	if (n != sizeof(config_space)) {
		fwts_warning(fw, "Could only read %zd bytes from %s, expecting %zd.", n, path, sizeof(config_space));
		return FWTS_ERROR;
	}

	if ((mapped_config_space = fwts_mmap(config->base_address, page_size)) == FWTS_MAP_FAILED) {
		char *data;

		if ((data = fwts_get("/sys/kernel/security/lockdown")) != NULL) {
			if (strstr(data, "[none]") == NULL) {
				free(data);
				fwts_log_info(fw, "Kernel is in lockdown mode. Aborted.");
				fwts_log_info(fw, "Please unlock the kernel before testing.");
				fwts_log_info(fw, "Make sure you disable secureboot and disable "
					"the kernel lockdown, (by kernel parameter lockdown=None).");
				return FWTS_ABORTED;
			}
		}
		free(data);

		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
			"Cannot mmap PCI config space at 0x%" PRIx64 ".",
			config->base_address);
		return FWTS_ERROR;
	}

	/* We only need to check if just the config space is readable */
	if (fwts_safe_memread(mapped_config_space, sizeof(config_space)) != FWTS_OK) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
			"Cannot read PCI config space at 0x%" PRIx64 ".",
			config->base_address);
		(void)fwts_munmap(mapped_config_space, page_size);
		return FWTS_ERROR;
	}

	/*
	 * Need to explicitly do byte comparisons on region
	 * memcmp() fails as this can do 64 bit reads
	 */
	for (match = true, i = 0; i < 16; i++) {
		if (config_space[i] != mapped_config_space[i]) {
			match = false;
			break;
		}
	}
	(void)fwts_munmap(mapped_config_space, page_size);

	if (match)
		fwts_passed(fw, "PCI config space verified.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"ACPI_040",
			"PCI config space appears to not work.");

	return FWTS_OK;
}

static bool mcfg_eval(ACPI_HANDLE object, char *name, bool *type_error, uint64_t *value)
{
	ACPI_STATUS status;
	ACPI_OBJECT *obj;
	bool has_default = !strcmp(name, "_SEG") || !strcmp(name, "_BBN");
	ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, NULL};
	status = AcpiEvaluateObject(object, name, NULL, &buf);

	if (status == AE_NOT_FOUND) {
		if (type_error)
			*type_error = false;
		if (has_default && value)
			*value = 0;
		return has_default;
	}

	if (ACPI_FAILURE(status))
		return false;

	if (buf.Pointer == NULL) {
		if (type_error)
			*type_error = !has_default;
		if (has_default && value)
			*value = 0;
		return has_default;
	}

	obj = buf.Pointer;
	if (obj->Type != ACPI_TYPE_INTEGER) {
		if (type_error)
			*type_error = true;
	} else {
		if (type_error)
			*type_error = false;
		if (value)
			*value = obj->Integer.Value;
	}

	free(obj);
	return true;
}

typedef struct {
	bool		found;
	uint8_t		start;
	uint8_t		end;
} mcfg_bus_range;

static ACPI_STATUS mcfg_crs_bus_callback(
	ACPI_RESOURCE *resource,
	void *context)
{
	mcfg_bus_range *range = context;

	if (!resource || !range)
		return AE_OK;

	if (resource->Type == ACPI_RESOURCE_TYPE_ADDRESS16 &&
	    resource->Data.Address16.ResourceType == ACPI_BUS_NUMBER_RANGE) {
		range->found = true;
		range->start = resource->Data.Address16.Address.Minimum;
		range->end   = resource->Data.Address16.Address.Maximum;
		return AE_CTRL_TERMINATE;
	}

	if (resource->Type == ACPI_RESOURCE_TYPE_ADDRESS32 &&
	    resource->Data.Address32.ResourceType == ACPI_BUS_NUMBER_RANGE) {
		range->found = true;
		range->start = resource->Data.Address32.Address.Minimum;
		range->end   = resource->Data.Address32.Address.Maximum;
		return AE_CTRL_TERMINATE;
	}

	if (resource->Type == ACPI_RESOURCE_TYPE_ADDRESS64 &&
	    resource->Data.Address64.ResourceType == ACPI_BUS_NUMBER_RANGE) {
		range->found = true;
		range->start = resource->Data.Address64.Address.Minimum;
		range->end   = resource->Data.Address64.Address.Maximum;
		return AE_CTRL_TERMINATE;
	}

	return AE_OK;
}

typedef struct {
	char		*name;
	uint16_t	seg;
	uint8_t		bus_start;
	uint8_t		bus_end;
	uint64_t	cba;
	bool		has_seg;
	bool		has_bus_range;
	bool		has_cba;
	bool		match_hid;
	bool		match_cid;
	bool		matched;
	bool		seg_type_error;
	bool		bbn_type_error;
	bool		cba_type_error;
} mcfg_bridge_info;

static fwts_list bridge_list;

static bool mcfg_object_id_is(const ACPI_OBJECT *obj, const char *id)
{
	char buf[16];

	if (!obj)
		return false;

	if (obj->Type == ACPI_TYPE_STRING) {
		if (obj->String.Pointer)
			return strcmp(obj->String.Pointer, id) == 0;
	}

	if (obj->Type == ACPI_TYPE_INTEGER) {
		if (fwts_method_valid_EISA_ID((uint32_t)obj->Integer.Value, buf, sizeof(buf)))
			return strcmp(buf, id) == 0;
	}

	return false;
}

static bool mcfg_object_hid_is(ACPI_HANDLE object, const char *id)
{
	bool rc;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };

	if (ACPI_FAILURE(AcpiEvaluateObject(object, "_HID", NULL, &buf)) || !buf.Pointer)
		return false;

	obj = buf.Pointer;

	rc = mcfg_object_id_is(obj, id);
	free(obj);
	return rc;
}

static bool mcfg_object_cid_contains(ACPI_HANDLE object, const char *id)
{
	uint32_t i;
	bool rc;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };

	if (ACPI_FAILURE(AcpiEvaluateObject(object, "_CID", NULL, &buf)) || !buf.Pointer)
		return false;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rc = mcfg_object_id_is(obj, id);
	} else {
		rc = false;
		for (i = 0; i < obj->Package.Count; i++) {
			if (mcfg_object_id_is(&obj->Package.Elements[i], id)) {
				rc = true;
				break;
			}
		}
	}

	free(obj);
	return rc;
}

static ACPI_STATUS mcfg_collect_bridge(
	ACPI_HANDLE object,
	UINT32 nesting_level,
	void *context,
	void **return_value)
{
	mcfg_bridge_info *info;
	mcfg_bus_range range = { false, 0, 0 };
	ACPI_BUFFER name_buf = { ACPI_ALLOCATE_BUFFER, NULL };
	uint64_t seg, bbn;

	FWTS_UNUSED(context);
	FWTS_UNUSED(nesting_level);
	FWTS_UNUSED(return_value);

	info = calloc(1, sizeof(*info));
	if (!info)
		return AE_NO_MEMORY;

	if (ACPI_SUCCESS(AcpiGetName(object, ACPI_FULL_PATHNAME, &name_buf))) {
		info->name = name_buf.Pointer;
	}

	info->match_hid = mcfg_object_hid_is(object, HID_ECAM);
	info->match_cid = mcfg_object_cid_contains(object, CID_PCI);
	if (!info->match_hid || !info->match_cid) {
		fwts_list_append(&bridge_list, info);
		return AE_OK;
	}

	if (mcfg_eval(object, "_SEG", &info->seg_type_error, &seg)) {
		info->has_seg = true;
		info->seg = seg;
	}

	AcpiWalkResources(object, "_CRS", mcfg_crs_bus_callback, &range);
	if (range.found) {
		info->has_bus_range = true;
		info->bus_start = range.start;
		info->bus_end   = range.end;
	} else {
		if (mcfg_eval(object, "_BBN", &info->bbn_type_error, &bbn)) {
			info->has_bus_range = true;
			info->bus_start = bbn;
			info->bus_end = bbn;
		}
	}

	info->has_cba = mcfg_eval(object, "_CBA", &info->cba_type_error, &info->cba);

	fwts_list_append(&bridge_list, info);

	return AE_OK;
}

static void mcfg_free_bridge(void *data)
{
	mcfg_bridge_info *br = data;
	if (!br)
		return;
	if (br->name)
		free(br->name);
	free(br);
}

/* Smallest power-of-two >= v (v > 0). Returns 0 on overflow. */
static uint64_t mcfg_next_pow2(uint64_t v)
{
	if (v == 0)
		return 0;
	if (v == 1)
		return 1;
	return 1ULL << (64 - __builtin_clzll(v - 1));
}

static int mcfg_brsi_test1(fwts_framework *fw)
{
	fwts_acpi_table_mcfg *mcfg;
	fwts_acpi_mcfg_configuration *config;
	int nr, i;
	const char *memory_map_name;
	fwts_list_link *item;
	mcfg_bridge_info *br;
	bool passed = true;
	bool found, cba_mismatch;
	uint64_t num_buses;
	uint64_t align;
	ACPI_STATUS status;

	if (table == NULL || table->length == 0) {
		fwts_passed(fw,
			"MCFG is not present. RISC-V BRS ACPI_040 allows "
			"omitting MCFG when there is no ECAM-compatible "
			"PCIe segment to describe.");
		return FWTS_OK;
	}

	memory_map_name = fwts_memory_map_name(fw->firmware_type);

	if (table->length < sizeof(fwts_acpi_table_mcfg)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
			"Invalid MCFG ACPI table size: got %zd bytes expecting more",
			table->length);
		fwts_advice(fw,
			"MCFG table must be least %zd bytes (header size) with "
			"multiples of %zd bytes for each MCFG entry.",
			sizeof(fwts_acpi_table_mcfg),
			sizeof(fwts_acpi_mcfg_configuration));
		return FWTS_ERROR;
	}

	mcfg = (fwts_acpi_table_mcfg *)table->data;
	nr = (table->length - sizeof(fwts_acpi_table_mcfg)) /
	     sizeof(fwts_acpi_mcfg_configuration);
	if (!nr) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
			"No MCFG ACPI table entries");
		return FWTS_ERROR;
	}

	if (table->length != sizeof(fwts_acpi_table_mcfg) + nr * sizeof(fwts_acpi_mcfg_configuration)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
			"MCFG table is not a multiple of record size");
		return FWTS_ERROR;
	}

	fwts_list_init(&bridge_list);
	status = AcpiGetDevices(HID_ECAM, mcfg_collect_bridge, fw, NULL);
	if (ACPI_FAILURE(status) || fwts_list_len(&bridge_list) == 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
			"MCFG is present but no PCIe host bridge with _HID %s was found. "
			"ECAM-compatible segments must be exposed with standard AML IDs "
			"(_HID %s, _CID %s) so a generic OS can use them without quirks.",
			HID_ECAM, HID_ECAM, CID_PCI);
		fwts_list_free_items(&bridge_list, mcfg_free_bridge);
		return FWTS_ERROR;
	}

	config = &mcfg->configuration[0];
	for (i = 0; i < nr; i++, config++) {
		fwts_log_info_verbatim(fw, "Configuration Entry #%d:", i);
		fwts_log_info_verbatim(fw, "  Base Address  : 0x%" PRIx64, config->base_address);
		fwts_log_info_verbatim(fw, "  Segment       : %" PRIu16, config->pci_segment_group_number);
		fwts_log_info_verbatim(fw, "  Start bus     : %" PRIu8, config->start_bus_number);
		fwts_log_info_verbatim(fw, "  End bus       : %" PRIu8, config->end_bus_number);

		/* start_bus <= end_bus */
		if (config->start_bus_number > config->end_bus_number) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"MCFG entry %d: start_bus (%u) > end_bus (%u)", i,
				config->start_bus_number, config->end_bus_number);
		}

		/* reserved must be 0 */
		if (config->reserved[0] || config->reserved[1] || config->reserved[2] || config->reserved[3]) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
				"MCFG entry %d: reserved field is not zero", i);
		}

		/*  ECAM base must be aligned to the whole region size */
		num_buses = (uint64_t)config->end_bus_number - config->start_bus_number + 1;
		align = mcfg_next_pow2(num_buses << 20);
		if (config->base_address & (align - 1)) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"MCFG entry #%d base 0x%" PRIx64" is not aligned to 0x%" PRIx64
				" (2^(n+20) for %u buses).", i, config->base_address, align,
				(unsigned)num_buses);
		}

		if ((memory_map_list != NULL) &&
		    (!fwts_memory_map_is_reserved(memory_map_list, config->base_address))) {
			passed = false;
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"MCFG MMIO config space at 0x%" PRIx64
				" is not reserved in the memory map table",
				config->base_address);
			fwts_advice(fw,
				"The PCI Express configuration space should be defined in "
				"the MCFG table and may optionally be marked reserved in "
				"the %s. Linux checks whether the region is reserved in "
				"the memory map and will reject MMCONFIG if there is a "
				"discrepancy. It is recommended to define this region in "
				"the %s table.", memory_map_name, memory_map_name);
		}

		for (int j = i + 1; j < nr; j++) {
			fwts_acpi_mcfg_configuration *a = &mcfg->configuration[i];
			fwts_acpi_mcfg_configuration *b = &mcfg->configuration[j];
			if (a->pci_segment_group_number == b->pci_segment_group_number && !(
			    a->start_bus_number > b->end_bus_number ||
			    b->start_bus_number > a->end_bus_number)) {
				passed = false;
				fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
					"MCFG entries #%d and #%d on segment %u have overlapping bus ranges.",
					i, j, a->pci_segment_group_number);
			}
		}

		found = false;
		cba_mismatch = false;
		fwts_list_foreach(item, &bridge_list) {
			br = fwts_list_data(mcfg_bridge_info *, item);
			if (br->matched)
				continue;
			if (!br->match_hid)
				continue;
			if (!br->match_cid)
				continue;
			if (!br->has_seg)
				continue;
			if (br->seg != config->pci_segment_group_number)
				continue;
			if (!br->has_bus_range)
				continue;
			if (br->bus_start >= config->start_bus_number &&
				br->bus_end   <= config->end_bus_number) {
				found = true;
				br->matched = true;
				if (br->has_cba && br->cba != config->base_address) {
					passed = false;
					cba_mismatch = true;
					fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
					"Host bridge _CBA 0x%" PRIx64
					" does not match MCFG entry #%d base "
					"address 0x%" PRIx64 ".",
					br->cba, i, config->base_address);
				}
			}
		}

		if (!found) {
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

		if (!cba_mismatch) {
			fwts_log_info(fw,
				"MCFG entry #%d segment %" PRIu16
				" buses %" PRIu8 "-%" PRIu8 " base 0x%" PRIx64
				" matches a host bridge with _HID %s, _CID %s "
				"and _SEG.",
				i, config->pci_segment_group_number,
				config->start_bus_number, config->end_bus_number,
				config->base_address,
				HID_ECAM, CID_PCI);
		}
	}

	fwts_list_foreach(item, &bridge_list) {
		br = fwts_list_data(mcfg_bridge_info *, item);

		if (!br->match_hid) {
			fwts_log_warning(fw,
				"Device %s matches %s via _HID or _CID, but _HID is not %s.",
				br->name ? br->name : "(unknown)", HID_ECAM, HID_ECAM);
			continue;
		}
		if (!br->match_cid) {
			fwts_log_warning(fw,
				"Device %s has _HID %s but _CID does not contain %s.",
				br->name ? br->name : "(unknown)", HID_ECAM, CID_PCI);
			continue;
		}

		if (br->seg_type_error) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "ACPI_040",
				"Device %s _SEG exists but did not return an Integer.",
				br->name ? br->name : "(unknown)");
		}
		if (br->bbn_type_error) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
				"Device %s _BBN exists but did not return an Integer.",
				br->name ? br->name : "(unknown)");
		}
		if (br->cba_type_error) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
				"Device %s _CBA exists but did not return an Integer.",
				br->name ? br->name : "(unknown)");
		}

		if (!br->matched && !br->has_cba) {
			fwts_warning(fw,
				"PNP0A08 bridge %s (segment %u, buses %u-%u) is "
				"neither covered by an MCFG entry nor has _CBA. "
				"A generic OS may not have ECAM access to this "
				"bridge (BRS §8.3.8).",
				br->name ? br->name : "(unknown)",
				br->seg, br->bus_start, br->bus_end);
		}

		if (!br->has_bus_range) {
			fwts_log_info(fw,
				"PNP0A08 bridge %s on segment %u has no bus range "
				"(_CRS or _BBN). Matching against MCFG entries was skipped.",
				br->name ? br->name : "(unknown)", br->seg);
		}
	}

	if (passed)
		fwts_passed(fw,
			"MCFG table is structurally valid (bus ranges, reserved fields, "
			"1 MiB alignment, no overlapping ranges), MMIO space is reserved "
			"in the memory map, and each entry matches a standard %s/%s "
			"host bridge (_SEG, and _CBA when present).",
			HID_ECAM, CID_PCI);


	fwts_list_free_items(&bridge_list, mcfg_free_bridge);

	return FWTS_OK;
}

static int mcfg_brsi_test2(fwts_framework *fw)
{
	fwts_acpi_table_mcfg *mcfg = (fwts_acpi_table_mcfg *)table->data;

	if (table == NULL || table->length == 0)
		return FWTS_OK;
	if (table->length < sizeof(fwts_acpi_table_mcfg) + sizeof(fwts_acpi_mcfg_configuration)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "ACPI_040",
			"No MCFG ACPI table entries");
		return FWTS_ERROR;
	}

	return compare_config_space(fw, &mcfg->configuration[0]);
}

static fwts_framework_minor_test mcfg_brsi_tests[] = {
	{ mcfg_brsi_test1,
	  "Check MCFG ACPI_040 table structure, ECAM base alignment and host-bridge pairing." },
	{mcfg_brsi_test2,
	  "Validate MCFG PCI config space is readable and matches sysfs."},
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
