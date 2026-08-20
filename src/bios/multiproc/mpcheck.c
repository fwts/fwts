/*
 * Copyright (C) 2011-2026 Canonical
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
#include <inttypes.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

static fwts_mp_data mp_data;
static bool fwts_mp_not_used = false;

static inline fwts_log_level fwts_level(const fwts_log_level level)
{
	return fwts_mp_not_used ? LOG_LEVEL_LOW : level;
}

static bool mpcheck_find_bus(const uint8_t id, const int depth)
{
	fwts_list_link	*entry;

	if (depth > 16)
		return false;		/* too deep? */

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);

		if (*data == FWTS_MP_BUS_ENTRY) {
			fwts_mp_bus_entry *bus_entry =
				fwts_list_data(fwts_mp_bus_entry *, entry);
			if (id == bus_entry->bus_id)
				return true;
		}
		if (*data == FWTS_MP_BUS_HIERARCHY_ENTRY) {
			fwts_mp_bus_hierarchy_entry *bus_hierarchy_entry =
				fwts_list_data(fwts_mp_bus_hierarchy_entry *, entry);
			if (id == bus_hierarchy_entry->bus_id)
				return mpcheck_find_bus(bus_hierarchy_entry->parent_bus, depth+1);
		}
	}
	return false;
}


static int mpcheck_test_cpu_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry1;
	int bootstrap_cpu = -1;
	bool usable_cpu_found = false;

	fwts_list_foreach(entry1, &mp_data.entries) {
		uint8_t *data1 = fwts_list_data(uint8_t *, entry1);

		if (*data1 == FWTS_MP_CPU_ENTRY) {
			fwts_list_link	*entry2;
			int m = 0;
			uint32_t phys_addr1 = mp_data.phys_addr +
				((void *)data1 - (void *)mp_data.header);

			fwts_mp_processor_entry *cpu_entry1 =
				fwts_list_data(fwts_mp_processor_entry *, entry1);

			/* Check Local APIC ID is unique */
			fwts_list_foreach(entry2, &mp_data.entries) {
				uint8_t *data2 = fwts_list_data(uint8_t *, entry2);
				uint32_t phys_addr2 = mp_data.phys_addr +
					((void *)data2 - (void *)mp_data.header);

				if (*data2 == FWTS_MP_CPU_ENTRY) {
					if ((n < m) && (entry2 != entry1)) {
						fwts_mp_processor_entry *cpu_entry2 =
							fwts_list_data(fwts_mp_processor_entry *, entry2);

						if (cpu_entry1->local_apic_id == cpu_entry2->local_apic_id) {
							fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
								"MPCPUEntryLAPICId",
								"CPU Entry %d (@0x%8.8" PRIx32 ")"
								" and %d (@0x%8.8" PRIx32 ") "
								"have the same Local APIC ID "
								"0x%2.2" PRIx8 ".",
								n, phys_addr1, m, phys_addr2,
								cpu_entry1->local_apic_id);
							failed = true;
							break;
						}
					}
					m++;
				}
			}

			/*
			if ((cpu_entry1->local_apic_version != 0x11) &&
			    (cpu_entry1->local_apic_version != 0x14)) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPCPUEntryLAPICVersion",
					"CPU Entry %d (@0x%8.8" PRIx32 ") has an "
					"invalid Local APIC Version %2.2x, should be "
					"0x11 or 0x14.",
					n, phys_addr,
					cpu_entry1->local_apic_version);
				failed = true;
			}
			*/
			if (cpu_entry1->cpu_flags & 1)
				usable_cpu_found = true;

			if ((cpu_entry1->cpu_flags >> 1) & 1) {
				if (bootstrap_cpu != -1) {
					fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
						"MPCPUEntryBootCPU",
						"CPU Entry %d (@0x%8.8" PRIx32 ") "
						"is marked as a boot CPU but CPU entry "
						"%d is the first boot CPU.",
						n, phys_addr1, bootstrap_cpu);
					failed = true;
				} else
					bootstrap_cpu = n;
			}
			n++;
		}
	}

	if (!usable_cpu_found) {
		fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
			"MPCPUEntryUsable",
			"CPU entries 0..%d were not marked as usable. "
			"There should be at least one usable CPU.",
			n-1);
		failed = true;
	}

	if (!failed)
		fwts_passed(fw, "All %d CPU entries look sane.", n);

	return FWTS_OK;
}

static int mpcheck_test_bus_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry;
	int prev_bus_id = -1;

	static const char *const bus_types[] = {
		"CBUS",
		"CBUSII",
		"EISA",
		"FUTURE",
		"INTERN",
		"ISA",
		"MBI",
		"MBII",
		"MCA",
		"MPI",
		"MPSA",
		"NUBUS",
		"PCI",
		"PCMCIA",
		"TC",
		"VL",
		"VME",
		"XPRESS",
		NULL,
	};

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		if (*data == FWTS_MP_BUS_ENTRY) {
			int i;
			uint32_t phys_addr = mp_data.phys_addr +
				((void *)data - (void *)mp_data.header);
			fwts_mp_bus_entry *bus_entry =
				fwts_list_data(fwts_mp_bus_entry *, entry);

			for (i = 0; bus_types[i] != NULL; i++) {
				if (strncmp(bus_types[i], (char*)bus_entry->bus_type, strlen(bus_types[i])) == 0)
					break;
			}
			if (bus_types[i] == NULL) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPBusEntryBusType",
					"Bus Entry %d (@0x%8.8" PRIx32 ") has an "
					"unrecognised bus type: %6.6s",
					n, phys_addr, bus_entry->bus_type);
			}
			if (prev_bus_id == -1) {
				prev_bus_id = bus_entry->bus_id;
				if (prev_bus_id != 0) {
					fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
						"MPBusEntryLAPICId",
						"Bus Entry %d (@0x%8.8" PRIx32 ") has a "
						"Local APIC ID 0x%2.2x and should be 0x00.",
						n, phys_addr,
						prev_bus_id);
					failed = true;
				}
			} else {
				if (bus_entry->bus_id < prev_bus_id) {
					fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
						"MPBusEntryBusId",
						"Bus Entry %d (@0x%8.8" PRIx32 ") has a "
						"Bus ID 0x%2.2x and should be greater "
						"than 0x%2.2x.",
						n, phys_addr,
						bus_entry->bus_id,
						prev_bus_id);
					failed = true;
				}
			}
			n++;
		}
	}
	if (!failed)
		fwts_passed(fw, "All %d Bus Entries looked sane.", n);

	return FWTS_OK;
}

static int mpcheck_test_io_apic_entries(fwts_framework *fw)
{
	bool failed = false;
	bool enabled = false;
	int n = 0;
	fwts_list_link	*entry;

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		uint32_t phys_addr = mp_data.phys_addr + ((void *)data - (void *)mp_data.header);
		if (*data == FWTS_MP_IO_APIC_ENTRY) {
			fwts_mp_io_apic_entry *io_apic_entry = fwts_list_data(fwts_mp_io_apic_entry *, entry);

			if (io_apic_entry->address == 0) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPIOAPICNullAddr",
					"IO APIC Entry %d (@0x%8.8" PRIx32 ") has an "
					"invalid NULL address, should be non-zero.",
					n, phys_addr);
				failed = true;
			}
			if (io_apic_entry->flags & 1) {
				enabled = true;
			}
			n++;
		}
	}

	if (!enabled) {
		fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
			"MPIOAPICEnabled",
			"None of the %d IO APIC entries were enabled, "
			"at least one must be enabled.", n);
		failed = true;
	}

	if (!failed)
		fwts_passed(fw, "All %d IO APIC Entries look sane.", n);

	return FWTS_OK;
}

static bool mpcheck_find_io_apic(const uint8_t id)
{
	fwts_list_link	*entry;

	if (id == 0xff)
		return true;	/* match all */

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
#if 0
		if (*data == FWTS_MP_CPU_ENTRY) {
			fwts_mp_processor_entry *cpu_entry =
				fwts_list_data(fwts_mp_processor_entry *, entry);
			if (id == cpu_entry->local_apic_id)
				return true;
		}
#endif
		if (*data == FWTS_MP_IO_APIC_ENTRY) {
			fwts_mp_io_apic_entry *io_apic_entry =
				fwts_list_data(fwts_mp_io_apic_entry *, entry);
			if (id == io_apic_entry->id)
				return true;
		}
	}
	return false;
}

static int mpcheck_test_io_interrupt_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry;

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		uint32_t phys_addr = mp_data.phys_addr + ((void *)data - (void *)mp_data.header);
		if (*data == FWTS_MP_IO_INTERRUPT_ENTRY) {
			fwts_mp_io_interrupt_entry *io_interrupt_entry =
				fwts_list_data(fwts_mp_io_interrupt_entry *, entry);

			if (io_interrupt_entry->type > 3) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPIOIRQType",
					"IO Interrupt Entry %d (@0x%8.8" PRIx32 ") has a "
					"Type 0x%2.2" PRIx8 " and should be 0x00..0x03.",
					n, phys_addr,
					io_interrupt_entry->type);
				failed = true;
			}
			if (!mpcheck_find_io_apic(io_interrupt_entry->destination_io_apic_id)) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPIOAPICId",
					"IO Interrupt Entry %d (@0x%8.8" PRIx32 ") has a "
					"Destination IO APIC ID 0x%2.2" PRIx8 " "
					"which has not been defined.",
					n, phys_addr,
					io_interrupt_entry->destination_io_apic_id);
				failed = true;
			}
			n++;
		}
	}

	if (!failed)
		fwts_passed(fw, "All %d IO Interrupt Entries look sane.", n);

	return FWTS_OK;
}

static int mpcheck_test_local_interrupt_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry;

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		uint32_t phys_addr = mp_data.phys_addr + ((void *)data - (void *)mp_data.header);
		if (*data == FWTS_MP_LOCAL_INTERRUPT_ENTRY) {
			fwts_mp_local_interrupt_entry *local_interrupt_entry =
				fwts_list_data(fwts_mp_local_interrupt_entry *, entry);
			if (local_interrupt_entry->type > 3) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPLocalIRQType",
					"Local Interrupt Entry %d (@0x%8.8" PRIx32 ") has a "
					"Type 0x%2.2" PRIx8 " and should be 0x00..0x03.",
					n, phys_addr,
					local_interrupt_entry->type);
				failed = true;
			}
#if 0
			if (!mpcheck_find_io_apic(local_interrupt_entry->destination_local_apic_id)) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_HIGH),
					"MPLocalIRQDestIRQAPIDId",
					"Local Interrupt Entry %d (@0x%8.8" PRIx32 ") has a"
					"Destination IO APIC ID 0x%2.2" PRIx8 " "
					"which has not been defined.",
					n, phys_addr,
					local_interrupt_entry->destination_local_apic_id);
				failed = true;
			}
#endif
			n++;
		}
	}

	if (!failed)
		fwts_passed(fw, "All %d Local Interrupt Entries look sane.", n);

	return FWTS_OK;
}

static int mpcheck_test_sys_addr_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry;

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		if (*data == FWTS_MP_SYS_ADDR_ENTRY) {
			uint32_t phys_addr = mp_data.phys_addr + ((void *)data - (void *)mp_data.header);
			fwts_mp_system_address_space_entry *sys_addr_entry =
				fwts_list_data(fwts_mp_system_address_space_entry *, entry);

			if (!mpcheck_find_bus(sys_addr_entry->bus_id, 0)) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPSysAddrSpaceBusId",
					"System Address Space Mapping Entry %d (@0x%8.8" PRIx32 ") "
					"has an Bus ID 0x%2.2" PRIx8 " "
					"that is not defined in any of the Bus Entries.",
					n, phys_addr, sys_addr_entry->bus_id);
				failed = true;
			}
			if (sys_addr_entry->address_type > 3) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPSysAddrSpaceType",
					"System Address Space Mapping Entry %d (@0x%8.8" PRIx32 ") "
					"has an incorrect Address Type: %2.2" PRIx8 ", "
					"should be 0..3.",
					n, phys_addr,
					sys_addr_entry->address_type);
				failed = true;
			}
			if (sys_addr_entry->address_length == 0) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPSysAddrSpaceAddrLength",
					"System Address Space Mapping Entry %d "
					"(@0x%8.8" PRIx32 ") has a "
					"zero sized Address Length.",
					n, phys_addr);
				failed = true;
			}
			n++;
		}
	}

	if (!failed)
		fwts_passed(fw, "All %d System Address Space Mapping Entries looks sane.", n);

	return FWTS_OK;
}

static int mpcheck_test_bus_hierarchy_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry;

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		if (*data == FWTS_MP_BUS_HIERARCHY_ENTRY) {
			uint32_t phys_addr = mp_data.phys_addr + ((void *)data - (void *)mp_data.header);
			fwts_mp_bus_hierarchy_entry *bus_hierarchy_entry =
				fwts_list_data(fwts_mp_bus_hierarchy_entry *, entry);
			if (bus_hierarchy_entry->length != 8) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPBusHieraracyLength",
					"Bus Hierarchy Entry %d (@x%8.8" PRIx32 ") "
					"length was 0x%2.2" PRIx8 ", it should be 0x08.",
					n, phys_addr,
					bus_hierarchy_entry->length);
				failed = true;
			}
			if (!mpcheck_find_bus(bus_hierarchy_entry->parent_bus, 0)) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPBusHierarchyParents",
					"Bus Hierarchy Entry %d (@x%8.8" PRIx32 ") "
					"did not have parents that "
					"connected to a top level Bus entry.",
					n, phys_addr);
				failed = true;
			}
			n++;
		}
	}

	if (!failed)
		fwts_passed(fw, "All %d Bus Hierarchy Entries look sane.", n);

	return FWTS_OK;
}

static int mpcheck_test_compat_bus_address_space_entries(fwts_framework *fw)
{
	bool failed = false;
	int n = 0;
	fwts_list_link	*entry;

	fwts_list_foreach(entry, &mp_data.entries) {
		uint8_t *data = fwts_list_data(uint8_t *, entry);
		if (*data == FWTS_MP_COMPAT_BUS_ADDRESS_SPACE_ENTRY) {
			uint32_t phys_addr = mp_data.phys_addr + ((void *)data - (void *)mp_data.header);
			fwts_mp_compat_bus_address_space_entry *compat_bus_entry =
				fwts_list_data(fwts_mp_compat_bus_address_space_entry*, entry);

			if (compat_bus_entry->length != 8) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPCompatBusLength",
					"Compatible Bus Address Space Entry %d "
					"(@x%8.8" PRIx32 ") "
					"length was 0x%2.2" PRIx8 ", it should be 0x08.",
					n, phys_addr,
					compat_bus_entry->length);
				failed = true;
			}
			if (compat_bus_entry->range_list > 1) {
				fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
					"MPCompatBusRangeList",
					"Compatible Bus Address Space Entry %d "
					"(@x%8.8" PRIx32 ") Range List was 0x%8.8" PRIx32
					", it should be 0x00 or 0x01.", n, phys_addr,
					compat_bus_entry->range_list);
				failed = true;
			}
			n++;
		}
	}

	if (!failed)
		fwts_passed(fw, "All %d Compatible Bus Address Space Entries look sane.", n);

	return FWTS_OK;
}

static void mpcheck_mp_used(fwts_framework *fw)
{
	fwts_list *klog;

	/* Can't read kernel log, not fatal */
	if ((klog = fwts_klog_read()) == NULL)
		return;

	/*
	 * Kernel reported that MADT is being used, so MP tables are ignored,
	 * this is only issues when the ACPI LAPIC is being used AND the
	 * ACPI IOAPIC is used too.
	 */
	if (fwts_klog_regex_find(fw, klog,
		"Using ACPI \\(MADT\\) for SMP configuration information") > 0) {
		fwts_mp_not_used = true;
		fwts_log_info(fw,
			"The kernel is using the ACPI MADT for SMP "
			"configuration information, so the Multiprocessor Tables "
			"are not used by the kernel. Any errors in the tables "
			"will not affect the operation of Linux unless it is "
			"booted with ACPI disabled.");
		fwts_log_nl(fw);
		fwts_log_info(fw,
			"NOTE: Since ACPI is being used in preference to the "
			"Multiprocessor Tables, any errors found in the mpcheck "
			"tests will be tagged as LOW errors.");
		fwts_log_nl(fw);
	}

	fwts_klog_free(klog);
}

static int mpcheck_init(fwts_framework *fw)
{
	mpcheck_mp_used(fw);

	if (fwts_mp_data_get(&mp_data) != FWTS_OK) {
		fwts_log_info(fw,
			"Failed to find the Multiprocessor Table data, skipping mpcheck test.");
		if (fwts_mp_not_used)
			fwts_log_info(fw,
				"NOTE: Since the ACPI MADT is being used instead for "
				"SMP configuration, this is not a problem.");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

static int mpcheck_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	return fwts_mp_data_free(&mp_data);
}

static int mpcheck_test_header(fwts_framework *fw)
{
	bool failed = false;

	if (strncmp((char*)mp_data.header->signature, FWTS_MP_HEADER_SIGNATURE, 4)) {
		fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
			"MPHeaderSig",
			"MP header signature should be %s, got %4.4s.",
			FWTS_MP_HEADER_SIGNATURE, mp_data.header->signature);
		failed = true;
	}

	if ((mp_data.header->spec_rev != 1) && (mp_data.header->spec_rev != 4)) {
		fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
			"MPHeaderRevision",
			"MP header spec revision should be 1 or 4, got %" PRIx8 ".",
			mp_data.header->spec_rev);
		failed = true;
	}
	if (mp_data.header->lapic_address == 0) {
		fwts_failed(fw, fwts_level(LOG_LEVEL_MEDIUM),
			"MPHeaderLAPICAddrNull",
			"MP header LAPIC address is NULL.");
		failed = true;
	}
	if (!failed)
		fwts_passed(fw, "MP header looks sane.");

	return FWTS_OK;
}


static fwts_framework_minor_test mpcheck_tests[] = {
	{ mpcheck_test_header,  		"Test MP header." },
	{ mpcheck_test_cpu_entries,		"Test MP CPU entries." },
	{ mpcheck_test_bus_entries,		"Test MP Bus entries." },
	{ mpcheck_test_io_apic_entries, 	"Test MP IO APIC entries." },
	{ mpcheck_test_io_interrupt_entries,	"Test MP IO Interrupt entries." },
	{ mpcheck_test_local_interrupt_entries, "Test MP Local Interrupt entries." },
	{ mpcheck_test_sys_addr_entries,	"Test MP System Address entries." },
	{ mpcheck_test_bus_hierarchy_entries,	"Test MP Bus Hierarchy entries." },
	{ mpcheck_test_compat_bus_address_space_entries, "Test MP Compatible Bus Address Space entries." },
	{ NULL, NULL }
};

static fwts_framework_ops mpcheck_ops = {
	.description = "MultiProcessor Tables tests.",
	.init        = mpcheck_init,
	.deinit      = mpcheck_deinit,
	.minor_tests = mpcheck_tests,
};

FWTS_REGISTER("mpcheck", &mpcheck_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ROOT_PRIV)

#endif
