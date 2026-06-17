/*
 * Copyright (C) 2010-2026 Canonical
 *
 * Portions of this code original from the Linux-ready Firmware Developer Kit
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
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include "fwts_acpi_object_eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef FWTS_ARCH_INTEL
#include <sys/io.h>
#else
static inline int ioperm(int a, ...)
{
	return a * 0;
}
#endif
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#define FADT_RESET_SUPPORTED		(1 << 10)

static const fwts_acpi_table_fadt *fadt;
static int fadt_size;

static const fwts_acpi_table_facs *facs;

static int fadt_init(fwts_framework *fw)
{
	fwts_acpi_table_info *table;

	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK) {
		fwts_log_error(fw, "Cannot read ACPI table FACP.");
		return FWTS_ERROR;
	}
	if (table == NULL) {
		fwts_log_error(fw, "ACPI table FACP does not exist!");
		return FWTS_ERROR;
	}
	fadt = (const fwts_acpi_table_fadt *)table->data;
	fadt_size = table->length;

	/*  Not having a FADT is not a failure on x86 */
	if (fadt_size == 0) {
		if (fw->target_arch == FWTS_ARCH_X86) {
			fwts_log_info(fw,
				      "FADT does not exist, this is not "
				      "necessarily a failure, skipping tests.");
			return FWTS_SKIP;
		} else {
			fwts_log_error(fw, "ACPI table FACP has zero length!");
			return FWTS_ERROR;
		}
	}

	/*
	 * Some tests require data from the FACS (5.2.10), which
	 * requires that we are not in hardware-reduced mode
	 */
	if (!fwts_acpi_is_reduced_hardware(fw)) {
		if (fwts_acpi_find_table(fw, "FACS", 0, &table) != FWTS_OK) {
			fwts_log_error(fw, "Cannot read ACPI table FACS.");
			return FWTS_ERROR;
		}
		if (table == NULL) {
			fwts_log_error(fw, "ACPI table FACS does not exist!");
			return FWTS_ERROR;
		}
		facs = (const fwts_acpi_table_facs *)table->data;
	}

	return FWTS_OK;
}

static void flag_info(fwts_framework *fw, const char *name, int state)
{
	const char *enabled = "set";
	const char *disabled = "not set";
	const char *ptr;

	ptr = (state) ? enabled : disabled;
	fwts_log_info(fw, "     %s is %s", name, ptr);
}

static int fadt_info(fwts_framework *fw)
{
	fwts_log_info(fw, "FADT: flag states");
	flag_info(fw, "WBINVD",
		  fadt->flags & FWTS_FACP_FLAG_WBINVD);
	flag_info(fw, "WBINVD_FLUSH",
		  fadt->flags & FWTS_FACP_FLAG_WBINVD_FLUSH);
	flag_info(fw, "PROC_C1",
		  fadt->flags & FWTS_FACP_FLAG_PROC_C1);
	flag_info(fw, "P_LVL2_UP",
		  fadt->flags & FWTS_FACP_FLAG_P_LVL2_UP);
	flag_info(fw, "PWR_BUTTON",
		  fadt->flags & FWTS_FACP_FLAG_PWR_BUTTON);
	flag_info(fw, "SLP_BUTTON",
		  fadt->flags & FWTS_FACP_FLAG_SLP_BUTTON);
	flag_info(fw, "FIX_RTC",
		  fadt->flags & FWTS_FACP_FLAG_FIX_RTC);
	flag_info(fw, "RTC_S4",
		  fadt->flags & FWTS_FACP_FLAG_RTC_S4);
	flag_info(fw, "TMR_VAL_EXT",
		  fadt->flags & FWTS_FACP_FLAG_TMR_VAL_EXT);
	flag_info(fw, "DCK_CAP",
		  fadt->flags & FWTS_FACP_FLAG_DCK_CAP);
	flag_info(fw, "RESET_REG_SUP",
		  fadt->flags & FWTS_FACP_FLAG_RESET_REG_SUP);
	flag_info(fw, "SEALED_CASE",
		  fadt->flags & FWTS_FACP_FLAG_SEALED_CASE);
	flag_info(fw, "HEADLESS",
		  fadt->flags & FWTS_FACP_FLAG_HEADLESS);
	flag_info(fw, "CPU_SW_SLP",
		  fadt->flags & FWTS_FACP_FLAG_CPU_SW_SLP);
	flag_info(fw, "PCI_EXP_WAK",
		  fadt->flags & FWTS_FACP_FLAG_PCI_EXP_WAK);
	flag_info(fw, "USE_PLATFORM_CLOCK",
		  fadt->flags & FWTS_FACP_FLAG_USE_PLATFORM_CLOCK);
	flag_info(fw, "S4_RTC_STS_VALID",
		  fadt->flags & FWTS_FACP_FLAG_S4_RTC_STS_VALID);
	flag_info(fw, "REMOTE_POWER_ON_CAPABLE",
		  fadt->flags & FWTS_FACP_FLAG_REMOTE_POWER_ON_CAPABLE);
	flag_info(fw, "FORCE_APIC_CLUSTER_MODEL",
		  fadt->flags & FWTS_FACP_FLAG_FORCE_APIC_CLUSTER_MODEL);
	flag_info(fw, "FORCE_APIC_PHYSICAL_DESTINATION_MODE",
		  fadt->flags & FWTS_FACP_FLAG_FORCE_APIC_PHYSICAL_DESTINATION_MODE);
	flag_info(fw, "HW_REDUCED_ACPI",
		  fadt->flags & FWTS_FACP_FLAG_HW_REDUCED_ACPI);
	flag_info(fw, "LOW_POWER_S0_IDLE_CAPABLE",
		  fadt->flags & FWTS_FACP_FLAG_LOW_POWER_S0_IDLE_CAPABLE); 
	if ((fadt->flags & FWTS_FACP_FLAG_PERSISTENT_CPU_CACHES) == 0)
		fwts_log_info(fw, "     PERSISTENT_CPU_CACHES is not reported");
	else if ((fadt->flags & FWTS_FACP_FLAG_PERSISTENT_CPU_CACHES) == FWTS_FACP_FLAG_PERSISTENT_CPU_CACHES) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			    "FADTCPUCACHESRESERVED",
			    "FADT persistent cpu caches value 11b should be reserved.");
	} else {
		fwts_log_info(fw, "     PERSISTENT_CPU_CACHES is 0x%" PRIx8,
		(fadt->flags & FWTS_FACP_FLAG_PERSISTENT_CPU_CACHES) >> 22);
	}
	fwts_log_info(fw, "FADT: IA-PC Boot Architecture flag states");
	flag_info(fw, "LEGACY_DEVICES", fadt->iapc_boot_arch &
		  FWTS_FACP_IAPC_BOOT_ARCH_LEGACY_DEVICES);
	flag_info(fw, "8042", fadt->iapc_boot_arch &
		  FWTS_FACP_IAPC_BOOT_ARCH_8042);
	flag_info(fw, "VGA_NOT_PRESENT", fadt->iapc_boot_arch &
		  FWTS_FACP_IAPC_BOOT_ARCH_VGA_NOT_PRESENT);
	flag_info(fw, "MSI_NOT_SUPPORTED", fadt->iapc_boot_arch &
		  FWTS_FACP_IAPC_BOOT_ARCH_MSI_NOT_SUPPORTED);
	flag_info(fw, "PCIE_ASPM_CONTROLS", fadt->iapc_boot_arch &
		  FWTS_FACP_IAPC_BOOT_ARCH_PCIE_ASPM_CONTROLS);
	flag_info(fw, "CMOS_RTC_NOT_PRESENT", fadt->iapc_boot_arch &
		  FWTS_FACP_IAPC_BOOT_ARCH_CMOS_RTC_NOT_PRESENT);

	fwts_log_info(fw, "FADT: ARM Boot Architecture flag states");
	flag_info(fw, "PSCI_COMPLIANT", fadt->arm_boot_flags &
		  FWTS_FACP_ARM_BOOT_ARCH_PSCI_COMPLIANT);
	flag_info(fw, "PSCI_USE_HVC", fadt->arm_boot_flags &
		  FWTS_FACP_ARM_BOOT_ARCH_PSCI_USE_HVC);

	fwts_infoonly(fw);
	return FWTS_OK;
}

static int fadt_checksum(fwts_framework *fw)
{
	const uint8_t *data = (const uint8_t *)fadt;
	ssize_t length = fadt->header.length;
	uint8_t checksum = 0;

	/* verify the table checksum */
	checksum = fwts_checksum(data, length);
	if (checksum == 0)
		fwts_passed(fw, "FADT checksum is correct");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTChecksum",
			    "FADT checksum is incorrect: 0x%x", checksum);

	return FWTS_OK;
}

static int fadt_revision(fwts_framework *fw)
{
	const uint8_t LATEST_MAJOR = (FWTS_ACPI_VERSION_NOW & 0xF00) >> 8;
	const uint8_t LATEST_MINOR = (FWTS_ACPI_VERSION_NOW & 0xFF) >> 4;
	uint8_t major;
	uint8_t minor;

	fwts_get_fadt_version(fw, &major, &minor);

	fwts_log_info(fw, "FADT revision: %" PRIu8 ".%" PRIu8, major, minor);
	fwts_log_info(fw, "FADT table length: %" PRIu32, fadt->header.length);

	if (major == LATEST_MAJOR && minor == LATEST_MINOR)
		fwts_passed(fw, "FADT revision is up to date.");
	else {
		fwts_warning(fw, "FADT revision is outdated: %" PRIu8 ".%" PRIu8,
			     major, minor);
		fwts_advice(fw, "The most recent revision of the FADT "
			    "defined in the ACPI specification is %" PRIu8 ".%" PRIu8 ".  "
			    "While older revisions of the FADT can be used, "
			    "newer ones may enable additional functionality "
			    "that cannot be used until the FADT is updated.",
			    LATEST_MAJOR, LATEST_MINOR);
	}

	return FWTS_OK;
}

static void acpi_table_check_fadt_firmware_ctrl(fwts_framework *fw)
{
	/* tests for very old FADTs */
	if (fadt->header.length < 140 && fadt->firmware_control == 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADT32BitFACSNull",
			    "FADT 32 bit FIRMWARE_CONTROL is null.");
		fwts_advice(fw,
			    "The ACPI version 1.0 FADT has a NULL "
			    "FIRMWARE_CTRL and it needs to be defined "
			    "to point to a valid Firmware ACPI Control "
			    "Structure (FACS).");

		/* can't do much else */
		return;
	}

	/* for more recent FADTs, things get more complicated */
	if (fadt->firmware_control == 0 && fadt->x_firmware_ctrl == 0) {
		if (fwts_acpi_is_reduced_hardware(fw)) {
			fwts_passed(fw,
				    "FADT 32 bit FIRMWARE_CONTROL and "
				    "64 bit X_FIRMWARE_CONTROL (FACS "
				    "address) are null in hardware reduced "
				    "mode.");
			fwts_advice(fw,
				    "When in hardware reduced mode, it is "
				    "allowed to have both the 32-bit "
				    "FIRMWARE_CTRL and 64-bit X_FIRMWARE_CTRL "
				    "fields set to zero as they are.  This "
				    "indicates that no FACS is available.");
		} else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				    "FADTFACSZero",
				    "FADT 32 bit FIRMWARE_CONTROL and "
				    "64 bit X_FIRMWARE_CONTROL (FACS "
				    "address) are both null.");
			fwts_advice(fw,
				    "The 32 bit FIRMWARE_CTRL or 64 "
				    "bit X_FIRMWARE_CTRL should point "
				    "to a valid Firmware ACPI Control "
				    "Structure (FACS) when ACPI hardware "
				    "reduced mode is not set. ");
		}

	} else 	if (fadt->firmware_control != 0 && fadt->x_firmware_ctrl != 0) {
		if ((uint64_t)fadt->firmware_control == fadt->x_firmware_ctrl) {
			fwts_passed(fw,
				    "Both FIRMWARE_CTRL and X_FIRMWARE_CTRL "
				    "are being used and contain the same FACS "
				    "address.");
			fwts_advice(fw,
				    "While having FIRMWARE_CTRL and "
				    "X_FIRMWARE_CTRL both set to an address "
				    "is not compliant with the ACPI "
				    "specification, they are both set to "
				    "the same address, which at least "
				    "mitigates the ambiguity in determining "
				    "which address is the correct one to use "
				    "for the FACS.  Ideally, only one of the "
				    "two addresses should be set but as a "
				    "practical matter, this will work.");
		} else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FwCtrl32and64Differ",
				    "FIRMWARE_CONTROL is 0x%" PRIx32
				    " and differs from X_FIRMWARE_CONTROL "
				    "0x%" PRIx64,
				    fadt->firmware_control,
				    fadt->x_firmware_ctrl);
			fwts_advice(fw,
				    "One would expect the 32 bit FIRMWARE_CTRL "
				    "and 64 bit X_FIRMWARE_CTRL pointers to "
				    "point to the same FACS, however they do "
				    "not which is clearly ambiguous and wrong. "
				    "The kernel works around this by using the "
				    "64-bit X_FIRMWARE_CTRL pointer to the "
				    "FACS. ");
		}
	} else { /* only one of firmware_control/x_firmware_ctrl is 0 */
		fwts_passed(fw,
			    "Only one of FIRMWARE_CTRL and X_FIRMWARE_CTRL "
			    "is non-zero.");
	}
}

static void acpi_table_check_fadt_dsdt(fwts_framework *fw)
{
	/* check out older FADTs */
	if (fadt->header.length < 148) {
		if (fadt->dsdt == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"FADTDSDTNull",
				"FADT DSDT address is null.");
		/* can't do much else */
		return;
	}

	/* if one field is being used, the other should be null */
	if ((fadt->dsdt != 0 && fadt->x_dsdt == 0) ||
	    (fadt->dsdt == 0 && fadt->x_dsdt != 0))
		fwts_passed(fw,
			    "FADT has only one of X_DSDT or DSDT addresses "
			    "being used.");
	else {
		if (fadt->dsdt == 0 && fadt->x_dsdt == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTOneDSDTNull",
				    "FADT X_DSDT and DSDT addresses cannot "
				    "both be null.");
	}

	/* unexpected use of addresses */
	if (fadt->dsdt != 0 && fadt->x_dsdt == 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADTXDSDTNull",
			    "FADT X_DSDT address is null.");
		fwts_advice(fw,
			    "An ACPI 2.0 or newer FADT is being used but "
			    "the 64 bit X_DSDT is null.  "
			    "The kernel will fall back to using "
			    "the 32 bit DSDT pointer instead.");
	}

	/*
	 * If you are going to insist on using both fields, even though
	 * that is incorrect, at least make it unambiguous as to which
	 * address is the one to use, by using the same value in both
	 * fields.
	 */
	if ((uint64_t)fadt->dsdt == fadt->x_dsdt && fadt->dsdt != 0) {
		fwts_passed(fw,
			    "FADT 32 bit DSDT and 64 bit X_DSDT "
			    "both point to the same physical address "
			    "(0x%" PRIx64 ").", fadt->x_dsdt);
		fwts_advice(fw,
			    "While it is not correct to use both of the "
			    "32- and 64-bit DSDT address fields in recent "
			    "versions of ACPI, they are at least the same "
			    "address, which keeps the kernel from getting "
			    "confused.  At some point, the 32-bit DSDT "
			    "address may get ignored so it is recommended "
			    "that the FADT be upgraded to only use the 64-"
			    "bit X_DSDT field.  In the meantime, however, "
			    "ACPI will still behave correctly.");
	}
	if ((uint64_t)fadt->dsdt != fadt->x_dsdt && fadt->dsdt != 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADT32And64Mismatch",
			    "FADT 32 bit DSDT (0x%" PRIx32 ") "
			    "does not point to same "
			    "physical address as 64 bit X_DSDT "
			    "(0x%" PRIx64 ").",
			    fadt->dsdt, fadt->x_dsdt);
		fwts_advice(fw,
			    "One would expect the 32 bit DSDT and "
			    "64 bit X_DSDT pointers to point to the "
			    "same DSDT, however they don't which is "
			    "clearly ambiguous and wrong. "
			    "The kernel works around this by assuming the "
			    "64 bit X_DSDT pointer to the DSDT is the correct "
			    "one to use.");
	}
}

static void acpi_table_check_fadt_reserved(fwts_framework *fw)
{
	if (fadt->reserved == (uint8_t)0)
		fwts_passed(fw, "FADT first reserved field is zero.");
	else if (fadt->reserved == (uint8_t)1)
		fwts_passed(fw, "FADT first reserved field is one "
			        "(compatible with ACPI 1.0).");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTReservedZero",
			    "FADT first reserved field is not zero: 0x%02x",
			    fadt->reserved);

	if (fadt->header.length < 112)
		return;

	if (fadt->reserved1 == (uint8_t)0)
		fwts_passed(fw, "FADT second reserved field is zero.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTReservedZero",
			    "FADT second reserved field is not zero: 0x%02x",
			    fadt->reserved1);

	if (fadt->header.revision == 5 &&
	    fadt->minor_version == 0 &&
	    fadt->arm_boot_flags != 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTReservedZero",
			    "FADT 5.0 reserved field 'arm_boot_flags' is not zero: 0x%04x",
			    fadt->arm_boot_flags);
}

static void acpi_table_check_fadt_pm_profile(fwts_framework *fw)
{
	fwts_log_info(fw, "FADT Preferred PM Profile: %hhu (%s)",
		fadt->preferred_pm_profile,
		fwts_acpi_fadt_preferred_pm_profile(fadt->preferred_pm_profile));

	if (fadt->preferred_pm_profile <= 8)
		fwts_passed(fw, "FADT has a valid preferred PM profile.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTPMProfile",
			    "FADT preferred PM profile is invalid.");
}

static void acpi_table_check_fadt_reduced_hardware(fwts_framework *fw)
{
	const char *IS = "IS";
	const char *IS_NOT = "IS NOT";
	bool rhw;
	bool passed;
	static const fwts_acpi_gas null_gas;
	uint32_t flag_mask;

	rhw = fwts_acpi_is_reduced_hardware(fw);
	fwts_log_info(fw, "FADT indicates ACPI %s in reduced hardware mode.",
		      rhw ? IS : IS_NOT);

	if (!rhw)
		return;

	passed = true;

	/* check all the fields that will be ignored */
	if (fadt->smi_cmd != 0) {
		passed = false;
		fwts_log_info(fw, "SMI_CMD is non-zero: 0x%x",
			      fadt->smi_cmd);
	}
	if (fadt->acpi_enable != 0) {
		passed = false;
		fwts_log_info(fw, "ACPI_ENABLE is non-zero: 0x%x",
			      fadt->acpi_enable);
	}
	if (fadt->acpi_disable != 0) {
		passed = false;
		fwts_log_info(fw, "ACPI_DISABLE is non-zero: 0x%x",
			      fadt->acpi_disable);
	}
	if (fadt->s4bios_req != 0) {
		passed = false;
		fwts_log_info(fw, "S4BIOS_REQ is non-zero: 0x%x",
			      fadt->s4bios_req);
	}
	if (fadt->pstate_cnt != 0) {
		passed = false;
		fwts_log_info(fw, "PSTATE_CNT is non-zero: 0x%x",
			      fadt->pstate_cnt);
	}
	if (fadt->pm1a_evt_blk != 0) {
		passed = false;
		fwts_log_info(fw, "PM1A_EVT_BLK is non-zero: 0x%x",
			      fadt->pm1a_evt_blk);
	}
	if (fadt->pm1b_evt_blk != 0) {
		passed = false;
		fwts_log_info(fw, "PM1B_EVT_BLK is non-zero: 0x%x",
			      fadt->pm1b_evt_blk);
	}
	if (fadt->pm1a_cnt_blk != 0) {
		passed = false;
		fwts_log_info(fw, "PM1A_CNT_BLK is non-zero: 0x%x",
			      fadt->pm1a_cnt_blk);
	}
	if (fadt->pm1b_cnt_blk != 0) {
		passed = false;
		fwts_log_info(fw, "PM1B_CNT_BLK is non-zero: 0x%x",
			      fadt->pm1b_cnt_blk);
	}
	if (fadt->pm2_cnt_blk != 0) {
		passed = false;
		fwts_log_info(fw, "PM2_CNT_BLK is non-zero: 0x%x",
			      fadt->pm2_cnt_blk);
	}
	if (fadt->pm_tmr_blk != 0) {
		passed = false;
		fwts_log_info(fw, "PM_TMR_BLK is non-zero: 0x%x",
			      fadt->pm_tmr_blk);
	}
	if (fadt->gpe0_blk != 0) {
		passed = false;
		fwts_log_info(fw, "GPE0_BLK is non-zero: 0x%x",
			      fadt->gpe0_blk);
	}
	if (fadt->gpe1_blk != 0) {
		passed = false;
		fwts_log_info(fw, "GPE1_BLK is non-zero: 0x%x",
			      fadt->gpe1_blk);
	}
	if (fadt->pm1_evt_len != 0) {
		passed = false;
		fwts_log_info(fw, "PM1_EVT_LEN is non-zero: 0x%x",
			      fadt->pm1_evt_len);
	}
	if (fadt->pm1_cnt_len != 0) {
		passed = false;
		fwts_log_info(fw, "PM1_CNT_LEN is non-zero: 0x%x",
			      fadt->pm1_cnt_len);
	}
	if (fadt->pm2_cnt_len != 0) {
		passed = false;
		fwts_log_info(fw, "PM2_CNT_LEN is non-zero: 0x%x",
			      fadt->pm2_cnt_len);
	}
	if (fadt->pm_tmr_len != 0) {
		passed = false;
		fwts_log_info(fw, "PM_TMR_LEN is non-zero: 0x%x",
			      fadt->pm_tmr_len);
	}
	if (fadt->gpe0_blk_len != 0) {
		passed = false;
		fwts_log_info(fw, "GPE0_BLK_LEN is non-zero: 0x%x",
			      fadt->gpe0_blk_len);
	}
	if (fadt->gpe1_blk_len != 0) {
		passed = false;
		fwts_log_info(fw, "GPE1_BLK_LEN is non-zero: 0x%x",
			      fadt->gpe1_blk_len);
	}
	if (fadt->gpe1_base != 0) {
		passed = false;
		fwts_log_info(fw, "GPE1_BASE is non-zero: 0x%x",
			      fadt->gpe1_base);
	}
	if (fadt->cst_cnt != 0) {
		passed = false;
		fwts_log_info(fw, "CST_CNT is non-zero: 0x%x",
			      fadt->cst_cnt);
	}
	if (fadt->p_lvl2_lat != 0) {
		passed = false;
		fwts_log_info(fw, "P_LVL2_LAT is non-zero: 0x%x",
			      fadt->p_lvl2_lat);
	}
	if (fadt->p_lvl3_lat != 0) {
		passed = false;
		fwts_log_info(fw, "P_LVL3_LAT is non-zero: 0x%x",
			      fadt->p_lvl3_lat);
	}
	if (fadt->flush_size != 0) {
		passed = false;
		fwts_log_info(fw, "FLUSH_SIZE is non-zero: 0x%x",
			      fadt->flush_size);
	}
	if (fadt->flush_stride != 0) {
		passed = false;
		fwts_log_info(fw, "FLUSH_STRIDE is non-zero: 0x%x",
			      fadt->flush_stride);
	}
	if (fadt->duty_offset != 0) {
		passed = false;
		fwts_log_info(fw, "DUTY_OFFSET is non-zero: 0x%x",
			      fadt->duty_offset);
	}
	if (fadt->duty_width != 0) {
		passed = false;
		fwts_log_info(fw, "DUTY_WIDTH is non-zero: 0x%x",
			      fadt->duty_width);
	}
	if (fadt->day_alrm != 0) {
		passed = false;
		fwts_log_info(fw, "DAY_ALRM is non-zero: 0x%x",
			      fadt->day_alrm);
	}
	if (fadt->mon_alrm != 0) {
		passed = false;
		fwts_log_info(fw, "MON_ALRM is non-zero: 0x%x",
			      fadt->mon_alrm);
	}
	if (fadt->century != 0) {
		passed = false;
		fwts_log_info(fw, "CENTURY is non-zero: 0x%x",
			      fadt->century);
	}
	if (memcmp((const void *)&fadt->x_pm1a_evt_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_PM1A_EVT_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_pm1b_evt_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_PM1B_EVT_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_pm1a_cnt_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_PM1A_CNT_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_pm1b_cnt_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_PM1B_CNT_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_pm2_cnt_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_PM2_CNT_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_pm_tmr_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_PM_TMR_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_gpe0_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_GPE0_BLK is a non-zero general "
			      "address structure.");
	}
	if (memcmp((const void *)&fadt->x_gpe1_blk,
		   (const void *)&null_gas,
		   sizeof(fwts_acpi_gas))) {
		passed = false;
		fwts_log_info(fw,
			      "X_GPE1_BLK is a non-zero general "
			      "address structure.");
	}

	if (passed)
		fwts_passed(fw, "All FADT reduced hardware fields are zero.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTRHWNonZero",
			    "Some FADT reduced hardware fields are non-zero.");

	/* now check all the reserved flags */
	flag_mask = FWTS_FACP_FLAG_WBINVD_FLUSH		|
		    FWTS_FACP_FLAG_PROC_C1		|
		    FWTS_FACP_FLAG_P_LVL2_UP		|
		    FWTS_FACP_FLAG_RTC_S4		|
		    FWTS_FACP_FLAG_TMR_VAL_EXT		|
		    FWTS_FACP_FLAG_CPU_SW_SLP		|
		    FWTS_FACP_FLAG_PCI_EXP_WAK		|
		    FWTS_FACP_FLAG_S4_RTC_STS_VALID	|
		    FWTS_FACP_FLAG_REMOTE_POWER_ON_CAPABLE;

	if (fadt->flags & flag_mask)
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTRHWFlagsNonZero",
			    "Some FADT reduced hardware flags are set.");
	else
		fwts_passed(fw, "All FADT reduced hardware flags are not set.");


	if ((fadt->flags & FWTS_FACP_FLAG_FORCE_APIC_CLUSTER_MODEL) ||
	    (fadt->flags & FWTS_FACP_FLAG_FORCE_APIC_PHYSICAL_DESTINATION_MODE))
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADTRHWAPICFlags",
			    "FADT APIC flags are set for reduced hardware "
			    "mode but may be irrelevant.");
	else
		fwts_passed(fw,
			    "FADT APIC flags are not set in reduced "
			    "hardware mode.");
}

static void acpi_table_check_fadt_smi_cmd(fwts_framework *fw)
{
	/*
	 *  Section 5.2.9 (Fixed ACPI Description Table) of the ACPI 5.0
	 *  specification states that if SMI_CMD is zero then it is
	 *  a system that does not support System Management Mode, so
	 *  in that case, don't check SCI_INT being valid.
	 */
	if (fadt->smi_cmd != 0) {
		if (fadt->sci_int == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTSCIIRQZero",
				    "FADT SCI Interrupt is 0x00, but must "
				    "be defined since SMI command indicates "
				    "System Management Mode is supported.");
		else
			fwts_passed(fw,
				    "FADT SMI_CMD indicates System Management "
				    "Mode is supported, and the SCI Interrupt "
				    "is non-zero.");
	} else {
		if ((fadt->acpi_enable == 0) &&
		    (fadt->acpi_disable == 0) &&
		    (fadt->s4bios_req == 0) &&
		    (fadt->pstate_cnt == 0) &&
		    (fadt->cst_cnt == 0)) {
			/*
			 * Not an error, but intentional, so feedback
			 * this finding.
			 */
			fwts_passed(fw, "The FADT SMI_CMD is zero, system "
				    "does not support System Management Mode.");
		}
		else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTSMICMDZero",
				    "FADT SMI_CMD is 0x00, however, one or "
				    "more of ACPI_ENABLE, ACPI_DISABLE, "
				    "S4BIOS_REQ, PSTATE_CNT and CST_CNT are "
				    "defined which means SMI_CMD should be "
				    "defined, otherwise SMI commands cannot "
				    "be sent.");
			fwts_advice(fw,
				    "The configuration seems to suggest that "
				    "SMI command should be defined to allow "
				    "the kernel to trigger system management "
				    "interrupts via the SMD_CMD port.  The "
				    "fact that SMD_CMD is zero which is "
				    "invalid means that SMIs are not possible "
				    "through the normal ACPI mechanisms. This "
				    "means some firmware based machine "
				    "specific functions will not work.");
		}
	}
}

static void acpi_table_check_fadt_acpi_enable(fwts_framework *fw)
{
	if (fadt->acpi_enable != 0) {
		if (fadt->smi_cmd == 0)
			fwts_failed(fw, LOG_LEVEL_HIGH,
				    "SMMNeedsAcpiEnableCmd",
				    "FADT SMI ACPI enable command is non-zero, "
				    "but SMM is not supported.");
		else
			fwts_passed(fw, "FADT SMI ACPI enable command is "
				    "non-zero, and SMM is supported.");
	}

	return;
}

static void acpi_table_check_fadt_acpi_disable(fwts_framework *fw)
{
	if (fadt->acpi_disable != 0) {
		if (fadt->smi_cmd == 0)
			fwts_failed(fw, LOG_LEVEL_HIGH,
				    "SMMNeedsAcpiDisableCmd",
				    "FADT SMI ACPI disable command is "
				    "non-zero, but SMM is not supported.");
		else
			fwts_passed(fw, "FADT SMI ACPI disable command is "
				    "non-zero, and SMM is supported.");
	}

	return;
}

static void acpi_table_check_fadt_s4bios_req(fwts_framework *fw)
{
	if (facs && facs->length >= 24)
		fwts_passed(fw,
			    "FADT indicates we are not in reduced hardware "
			    "mode, and required FACS is present.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FACSMustBePresent",
			    "FADT indicates we are not in reduced hardware "
			    "mode, which requires an FACS be present, but "
			    "none has been found.");

	if (facs && (facs->flags & FWTS_FACS_FLAG_S4BIOS_F)) {
		if (fadt->s4bios_req == 0) {
			if (fadt->smi_cmd == 0) {
				fwts_passed(fw,
					    "FADT indicates System Management "
					    "Mode is not supported, which "
					    "allows a zero S4BIOS_REQ value.");
				fwts_advice(fw,
					    "There is an inconsistency between "
					    "the FADT and FACS.  The FADT "
					    "indicates no SMM support, and "
					    "no S4BIOS_REQ command, but the "
					    "FACS indicates S4BIOS_REQ is "
					    "supported.  One of these may "
					    "be incorrect.");
			} else {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					    "SMMHasNoS4BIOSReqCmd",
					    "FADT SMI S4BIOS_REQ command is "
					    "zero, but this is not allowed "
					    "when SMM is supported, and the "
					    "S4BIOS_F flag is set in the "
					    "FACS.");
			}
		} else {
			if (fadt->smi_cmd == 0) {
				fwts_passed(fw,
					    "FADT indicates System Management "
					    "Mode is not supported, but a "
					    "S4BIOS_REQ command is defined.");
				fwts_advice(fw,
					    "There is an inconsistency between "
					    "the FADT and FACS.  The FADT "
					    "indicates no SMM support, but it "
					    "defines an S4BIOS_REQ command, "
					    "and the FACS indicates "
					    "S4BIOS_REQ is supported.  One "
					    "of these may be incorrect.");
			} else {
				fwts_passed(fw,
					    "FADT S4BIOS_REQ command is "
					    "non-zero, SMM is supported so "
					    "the command can be used, and "
					    "the FACS indicates S4BIOS_REQ "
					    "is supported.");
			}
		}
	} else {
		if (fadt->s4bios_req == 0)
			fwts_passed(fw, "FADT S4BIOS_REQ command is not set "
				    "and FACS indicates it is not supported.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SMMS4BIOSCmdDefined",
				    "FADT S4BIOS_REQ command is defined "
				    "but FACS indicates it is not supported.");
	}

	return;
}

static void acpi_table_check_fadt_pstate_cnt(fwts_framework *fw)
{
	if (fadt->pstate_cnt == 0) {
		if (fadt->smi_cmd == 0)
			fwts_passed(fw,
				    "FADT SMI PSTATE_CNT command is zero, "
				    "which is allowed since SMM is not "
				    "supported.");
	} else {
		if (fadt->smi_cmd == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SMMHasExtraPstateCntCmd",
				    "FADT SMI PSTATE_CNT command is "
				    "non-zero, but SMM is not supported.");
		else
			fwts_passed(fw, "FADT SMI PSTATE_CNT command is "
				    "non-zero, and SMM is supported.");
	}

	return;
}

static void acpi_table_check_fadt_pm1a_evt_blk(fwts_framework *fw)
{
	bool both_zero;
	bool both_nonzero;

	if (fadt->header.length < 160) {
		if (fadt->pm1a_evt_blk == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm1aEvtBlkNotSet",
				    "FADT PM1A_EVT_BLK is a required field "
				    "and must have a 32-bit address set.");
		else
			fwts_passed(fw,
				    "FADT required PM1A_EVT_BLK field is "
				    "non-zero.");

		/* can't do much else */
		return;
	}

	if (fadt->pm1a_evt_blk == 0 && fadt->x_pm1a_evt_blk.address == 0) {
		both_zero = true;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADTPm1aEvtBlkNotSet",
			    "FADT PM1A_EVT_BLK is a required field and must "
			    "have either a 32-bit or 64-bit address set.");
	} else {
		both_zero = false;
		fwts_passed(fw,
			    "FADT required PM1A_EVT_BLK field is non-zero");
	}

	if (fadt->pm1a_evt_blk != 0 && fadt->x_pm1a_evt_blk.address != 0)
		both_nonzero = true;
	else {
		both_nonzero = false;
		if (!both_zero)
			fwts_passed(fw,
				    "FADT one required PM1A_EVT_BLK field "
				    "is non-zero");
	}

	if (both_nonzero &&
	    ((uint64_t)fadt->pm1a_evt_blk == fadt->x_pm1a_evt_blk.address)) {
		fwts_passed(fw,
			    "FADT 32- and 64-bit PM1A_EVT_BLK fields are "
			    "at least equal.");
		fwts_advice(fw,
			    "Both FADT 32- and 64-bit PM1A_EVT_BLK "
			    "fields are being used, but only one should be "
			    "non-zero.  However, they are at least equal so "
			    "the kernel will at least have a usable value.");
	} else {
		if (fwts_get_acpi_version(fw) <= FWTS_ACPI_VERSION_61) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm1aEvtBlkNotSet",
				    "FADT PM1A_EVT_BLK is a required field and must "
				    "have either a 32-bit or 64-bit address set.");
			fwts_advice(fw,
				    "Both FADT 32- and 64-bit PM1A_EVT_BLK "
				    "fields are being used, but only one "
				    "should be non-zero.  Since the fields "
				    "value are not equal the kernel cannot "
				    "unambiguously determine which value "
				    "is the correct one.");
		}
		fwts_passed(fw, "FADT 64-bit X_PM1A_EVT_BLK field is set "
			    "and 32-bit PM1A_EVT_BLK will be ignored.");
	}
}

static void acpi_table_check_fadt_pm1b_evt_blk(fwts_framework *fw)
{
	if (fadt->pm1b_evt_blk == 0 && fadt->header.length < 172) {
		fwts_skipped(fw, "FADT PM1B_EVT_BLK not being used.");
		return;
	}

	if (fadt->pm1b_evt_blk == 0 && fadt->x_pm1b_evt_blk.address == 0) {
		fwts_skipped(fw, "FADT PM1B_EVT_BLK not being used.");
		return;
	}

	if ((fadt->pm1b_evt_blk != 0 && fadt->x_pm1b_evt_blk.address == 0) ||
	    (fadt->pm1b_evt_blk == 0 && fadt->x_pm1b_evt_blk.address != 0))
		fwts_passed(fw,
			    "FADT only one of the 32-bit or 64-bit "
			    "PM1B_EVT_BLK fields is being used.");

	if ((uint64_t)fadt->pm1b_evt_blk == fadt->x_pm1b_evt_blk.address) {
		fwts_passed(fw,
			    "FADT 32- and 64-bit PM1B_EVT_BLK fields are "
			    "at least equal.");
		fwts_advice(fw,
			    "Both FADT 32- and 64-bit PM1B_EVT_BLK "
			    "fields are being used, but only one should be "
			    "non-zero.  However, they are at least equal so "
			    "the kernel will at least have a usable value.");
	} else {
		if (fwts_get_acpi_version(fw) <= FWTS_ACPI_VERSION_61) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm1bEvtBlkNotSet",
				    "FADT PM1B_EVT_BLK is a required field and must "
				    "have either a 32-bit or 64-bit address set.");
			fwts_advice(fw,
				    "Both FADT 32- and 64-bit PM1B_EVT_BLK "
				    "fields are being used, but only one should be "
				    "non-zero.  Since the fields value are not equal "
				    "the kernel cannot unambiguously determine which "
				    "value is the correct one.");
		}
		fwts_passed(fw, "FADT 64-bit X_PM1B_EVT_BLK field is set "
			    "and 32-bit PM1B_EVT_BLK will be ignored.");
	}
}

static void acpi_table_check_fadt_pm1a_cnt_blk(fwts_framework *fw)
{
	if (fadt->header.length < 184) {
		if (fadt->pm1a_cnt_blk == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm1aCntBlkNotSet",
				    "FADT PM1A_CNT_BLK is a required field "
				    "and must have a 32-bit address set.");
		else
			fwts_passed(fw,
				    "FADT required PM1A_EVT_BLK field is "
				    "non-zero.");

		/* can't do much else */
		return;
	}

	if (fadt->pm1a_cnt_blk != 0 || fadt->x_pm1a_cnt_blk.address != 0)
		fwts_passed(fw,
			    "FADT required PM1A_CNT_BLK field is non-zero");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADTPm1aCntBlkNotSet",
			    "FADT PM1A_CNT_BLK is a required field and must "
			    "have either a 32-bit or 64-bit address set.");

	if ((fadt->pm1a_cnt_blk != 0 && fadt->x_pm1a_cnt_blk.address == 0) ||
	    (fadt->pm1a_cnt_blk == 0 && fadt->x_pm1a_cnt_blk.address != 0))
		fwts_passed(fw,
			    "FADT only one of the 32-bit or 64-bit "
			    "PM1A_CNT_BLK fields is being used.");

	if ((uint64_t)fadt->pm1a_cnt_blk == fadt->x_pm1a_cnt_blk.address) {
		fwts_passed(fw,
			    "FADT 32- and 64-bit PM1A_CNT_BLK fields are "
			    "at least equal.");
		fwts_advice(fw,
			    "Both FADT 32- and 64-bit PM1A_CNT_BLK "
			    "fields are being used, but only one should be "
			    "non-zero.  However, they are at least equal so "
			    "the kernel will at least have a usable value.");
	} else {
		if (fwts_get_acpi_version(fw) <= FWTS_ACPI_VERSION_61) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm1aCntBlkNotSet",
				    "FADT PM1A_CNT_BLK is a required field and must "
				    "have either a 32-bit or 64-bit address set.");
			fwts_advice(fw,
				    "Both FADT 32- and 64-bit PM1A_CNT_BLK "
				    "fields are being used, but only one should be "
				    "non-zero.  Since the fields value are not equal "
				    "the kernel cannot unambiguously determine which "
				    "value is the correct one.");
		}
		fwts_passed(fw, "FADT 64-bit X_PM1A_CNT_BLK field is set "
			    "and 32-bit PM1A_CNT_BLK will be ignored.");
	}
}

static void acpi_table_check_fadt_pm1b_cnt_blk(fwts_framework *fw)
{
	if (fadt->pm1b_cnt_blk == 0 && fadt->header.length < 196) {
		fwts_skipped(fw, "FADT PM1B_CNT_BLK not being used.");
		return;
	}

	if (fadt->pm1b_cnt_blk == 0 && fadt->x_pm1b_cnt_blk.address == 0) {
		fwts_skipped(fw, "FADT PM1B_CNT_BLK not being used.");
		return;
	}

	if ((fadt->pm1b_cnt_blk != 0 && fadt->x_pm1b_cnt_blk.address == 0) ||
	    (fadt->pm1b_cnt_blk == 0 && fadt->x_pm1b_cnt_blk.address != 0))
		fwts_passed(fw,
			    "FADT only one of the 32-bit or 64-bit "
			    "PM1B_CNT_BLK fields is being used.");

	if ((uint64_t)fadt->pm1b_cnt_blk == fadt->x_pm1b_cnt_blk.address) {
		fwts_passed(fw,
			    "FADT 32- and 64-bit PM1B_CNT_BLK fields are "
			    "at least equal.");
		fwts_advice(fw,
			    "Both FADT 32- and 64-bit PM1B_CNT_BLK "
			    "fields are being used, but only one should be "
			    "non-zero.  However, they are at least equal so "
			    "the kernel will at least have a usable value.");
	} else {
		if (fwts_get_acpi_version(fw) <= FWTS_ACPI_VERSION_61) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm1bCntBlkNotSet",
				    "FADT PM1B_CNT_BLK is a required field and must "
				    "have either a 32-bit or 64-bit address set.");
			fwts_advice(fw,
				    "Both FADT 32- and 64-bit PM1B_CNT_BLK "
				    "fields are being used, but only one should be "
				    "non-zero.  Since the fields value are not equal "
				    "the kernel cannot unambiguously determine which "
				    "value is the correct one.");
		}
		fwts_passed(fw, "FADT 64-bit X_PM1B_CNT_BLK field is set "
			    "and 32-bit PM1B_CNT_BLK will be ignored.");
	}
}

static void acpi_table_check_fadt_pm2_cnt_blk(fwts_framework *fw)
{
	if (fadt->pm2_cnt_blk == 0 && fadt->header.length < 208) {
		fwts_skipped(fw, "FADT PM2_CNT_BLK not being used.");
		return;
	}

	if (fadt->pm2_cnt_blk == 0 && fadt->x_pm2_cnt_blk.address == 0) {
		fwts_skipped(fw, "FADT PM2_CNT_BLK not being used.");
		return;
	}

	if ((fadt->pm2_cnt_blk != 0 && fadt->x_pm2_cnt_blk.address == 0) ||
	    (fadt->pm2_cnt_blk == 0 && fadt->x_pm2_cnt_blk.address != 0))
		fwts_passed(fw,
			    "FADT only one of the 32-bit or 64-bit "
			    "PM2_CNT_BLK fields is being used.");

	if ((uint64_t)fadt->pm2_cnt_blk == fadt->x_pm2_cnt_blk.address) {
		fwts_passed(fw,
			    "FADT 32- and 64-bit PM2_CNT_BLK fields are "
			    "at least equal.");
		fwts_advice(fw,
			    "Both FADT 32- and 64-bit PM2_CNT_BLK "
			    "fields are being used, but only one should be "
			    "non-zero.  However, they are at least equal so "
			    "the kernel will at least have a usable value.");
	} else {
		if (fwts_get_acpi_version(fw) <= FWTS_ACPI_VERSION_61) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm2CntBlkNotSet",
				    "FADT PM2_CNT_BLK is a required field and must "
				    "have either a 32-bit or 64-bit address set.");
			fwts_advice(fw,
				    "Both FADT 32- and 64-bit PM2_CNT_BLK "
				    "fields are being used, but only one should be "
				    "non-zero.  Since the fields value are not equal "
				    "the kernel cannot unambiguously determine which "
				    "value is the correct one.");
		}
		fwts_passed(fw, "FADT 64-bit X_PM2_CNT_BLK field is set "
			    "and 32-bit PM2_CNT_BLK will be ignored.");
	}
}

static void acpi_table_check_fadt_pm_tmr_blk(fwts_framework *fw)
{
	if (fadt->pm_tmr_blk == 0 && fadt->header.length < 220) {
		fwts_skipped(fw, "FADT PM_TMR_BLK not being used.");
		return;
	}

	if (fadt->pm_tmr_blk == 0 && fadt->x_pm_tmr_blk.address == 0) {
		fwts_skipped(fw, "FADT PM_TMR_BLK not being used.");
		return;
	}

	if ((fadt->pm_tmr_blk != 0 && fadt->x_pm_tmr_blk.address == 0) ||
	    (fadt->pm_tmr_blk == 0 && fadt->x_pm_tmr_blk.address != 0))
		fwts_passed(fw,
			    "FADT only one of the 32-bit or 64-bit "
			    "PM_TMR_BLK fields is being used.");

	if ((uint64_t)fadt->pm_tmr_blk == fadt->x_pm_tmr_blk.address) {
		fwts_passed(fw,
			    "FADT 32- and 64-bit PM_TMR_BLK fields are "
			    "at least equal.");
		fwts_advice(fw,
			    "Both FADT 32- and 64-bit PM_TMR_BLK "
			    "fields are being used, but only one should be "
			    "non-zero.  However, they are at least equal so "
			    "the kernel will at least have a usable value.");
	} else {
		if (fwts_get_acpi_version(fw) <= FWTS_ACPI_VERSION_61) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm2CntBlkNotSet",
				    "FADT PM1A_CNT_BLK is a required field and must "
				    "have either a 32-bit or 64-bit address set.");
			fwts_advice(fw,
				    "Both FADT 32- and 64-bit PM_TMR_BLK "
				    "fields are being used, but only one should be "
				    "non-zero.  Since the fields value are not equal "
				    "the kernel cannot unambiguously determine which "
				    "value is the correct one.");
		}
		fwts_passed(fw, "FADT 64-bit X_PM_TMR_BLK field is set "
			    "and 32-bit PM_TMR_BLK will be ignored.");
	}
}

static void acpi_table_check_fadt_pm1_evt_len(fwts_framework *fw)
{
	if (fadt->pm1_evt_len >= 4)
		fwts_passed(fw, "FADT PM1_EVT_LEN >= 4.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTPm1EvtLenTooSmall",
			    "FADT PM1_EVT_LEN must be >= 4, but is %" PRIu8 ".",
			    fadt->pm1_evt_len);
	return;
}

static void acpi_table_check_fadt_pm1_cnt_len(fwts_framework *fw)
{
	if (fadt->pm1_cnt_len >= 2)
		fwts_passed(fw, "FADT PM1_CNT_LEN >= 2.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTPm1CntLenTooSmall",
			    "FADT PM1_CNT_LEN must be >= 2, but is %" PRIu8 ".",
			    fadt->pm1_cnt_len);
	return;
}

static void acpi_table_check_fadt_pm2_cnt_len(fwts_framework *fw)
{
	if (fadt->pm2_cnt_blk == 0) {
		if (fadt->pm2_cnt_len == 0)
			fwts_passed(fw, "FADT PM2_CNT_LEN is zero and "
				    "PM2_CNT_BLK is not supported.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPm2CntLenInconsistent",
				    "FADT PM2_CNT_LEN must be zero because "
				    "PM2_CNT_BLK is not supported, but is %" PRIu8 ".",
				    fadt->pm2_cnt_len);
		return;
	}

	if (fadt->pm2_cnt_len >= 1)
		fwts_passed(fw, "FADT PM2_CNT_LEN >= 1.");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "FADTPm2CntLenTooSmall",
			    "FADT PM2_CNT_LEN must be >= 1, but is %" PRIu8 ".",
			    fadt->pm2_cnt_len);
	return;
}

static void acpi_table_check_fadt_pm_tmr_len(fwts_framework *fw)
{
	if (fadt->pm_tmr_len == 0) {
		if (fadt->pm_tmr_blk == 0)
			fwts_passed(fw, "FADT PM_TMR_LEN is zero and "
				    "PM_TMR_BLK is not supported.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTPmTmrLenInconsistent",
				    "FADT PM_TMR_LEN must be zero because "
				    "PM_TMR_BLK is not supported, but is %" PRIu8 ".",
				    fadt->pm_tmr_len);
		return;
	}

	if (fadt->pm_tmr_len == 4)
		fwts_passed(fw, "FADT PM_TMR_LEN is 4.");
	else {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"FADTBadPMTMRLEN",
			"FADT PM_TMR_LEN is %" PRIu8
			", should be 4.", fadt->pm_tmr_len);
		fwts_advice(fw,
			"FADT field PM_TMR_LEN defines the number "
			"of bytes decoded by PM_TMR_BLK. "
			"This fields value must be 4. If it is not "
			"the correct size then the kernel will not "
			"request a region for the pm timer block. ");
	}
}

static void acpi_table_check_fadt_gpe0_blk_len(fwts_framework *fw)
{
	if (fadt->gpe0_blk_len & 1) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADTBadGPE0BLKLEN",
			    "FADT GPE0_BLK_LEN is %" PRIu8
			    ", should a multiple of 2.",
			    fadt->gpe0_blk_len);
		fwts_advice(fw,
			    "The FADT GPE0_BLK_LEN should be a multiple of 2. "
			    "Because it isn't, the ACPI driver will not map in "
			    "the GPE0 region. This could mean that General "
			    "Purpose Events will not function correctly (for "
			    "example lid or AC-power events).");
	} else {
		if (fadt->gpe0_blk_len)
			fwts_passed(fw, "FADT GPE0_BLK_LEN non-zero and a "
				    "non-negative multiple of 2: %" PRIu8 ".",
				    fadt->gpe0_blk_len);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTZeroGPE0BlkLen",
				    "FADT GPE0_BLK_LEN is zero, but must be "
				    "set to a non-negative multiple of 2.");

	}
}

static void acpi_table_check_fadt_gpe1_blk_len(fwts_framework *fw)
{
	if (fadt->gpe1_blk_len == 0) {
		if (fadt->gpe1_blk == 0)
			fwts_passed(fw, "FADT GPE1_BLK_LEN is zero and "
				    "GPE1_BLK is not supported.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTGPE1BlkLenInconsistent",
				    "FADT GPE1_BLK_LEN must be zero because "
				    "GPE1_BLK is not supported, but is %" PRIu8 ".",
				    fadt->gpe1_blk_len);
		return;
	}

	if (fadt->gpe1_blk_len & 1) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "FADTBadGPE1BLKLEN",
			    "FADT GPE1_BLK_LEN is %" PRIu8
			    ", should a multiple of 2.",
			    fadt->gpe1_blk_len);
		fwts_advice(fw,
			    "The FADT GPE1_BLK_LEN should be a multiple of 2. "
			    "Because it isn't, the ACPI driver will not map in "
			    "the GPE1 region. This could mean that General "
			    "Purpose Events will not function correctly (for "
			    "example lid or AC-power events).");
	} else {
		if (fadt->gpe1_blk_len)
			fwts_passed(fw, "FADT GPE1_BLK_LEN non-zero and a "
				    "non-negative multiple of 2: %" PRIu8 ".",
				    fadt->gpe1_blk_len);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "FADTZeroGPE1BlkLen",
				    "FADT GPE1_BLK_LEN is zero, but must be "
				    "set to a non-negative multiple of 2.");

	}
}

static void acpi_table_check_fadt_cst_cnt(fwts_framework *fw)
{
	if (fadt->cst_cnt == 0) {
		if (fadt->smi_cmd == 0)
			fwts_passed(fw,
				    "FADT SMI CST_CNT command is zero, "
				    "which is allowed since SMM is not "
				    "supported.");
	} else {
		if (fadt->smi_cmd == 0)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SMMHasExtraCSTCntCmd",
				    "FADT SMI CST_CNT command is "
				    "non-zero, but SMM is not supported.");
		else
			fwts_passed(fw, "FADT SMI CST_CNT command is "
				    "non-zero, and SMM is supported.");
	}

	return;
}

static uint64_t fadt_find_p_blk(fwts_framework *fw)
{
	uint64_t pblk;
	fwts_list *objects;

	pblk = 0;
	objects = fwts_acpi_object_get_names();
	if (objects) {
		fwts_list_link *obj;

		fwts_list_foreach(obj, objects) {
			char *name = fwts_list_data(char*, obj);
			ACPI_OBJECT pr = { 0 };
			ACPI_BUFFER buf = { sizeof(ACPI_OBJECT), &pr };
			ACPI_HANDLE handle;
			ACPI_OBJECT_TYPE type;
			ACPI_STATUS status;

			status = AcpiGetHandle(NULL, name, &handle);
			if (ACPI_FAILURE(status)) {
				fwts_warning(fw, "Failed to get handle for "
					     "object %s.", name);
				continue;
			}
			status = AcpiGetType(handle, &type);
			if (ACPI_FAILURE(status)) {
				fwts_warning(fw, "Failed to get type for "
					     "object %s.", name);
				continue;
			}

			/*
			 * If a CPU is not defined as a Processor object,
			 * we don't care here.  Per section 8.1.2 and 8.1.3,
			 * defining a CPU with a Device object implies that
			 * a _CST method must be defined, and whatever is
			 * in the _CST overrides the P_BLK and P_LVL*_LAT
			 * values.  Since we're only trying to validate the
			 * P_LVL*_LAT values, and they're only used if the
			 * CPUs are defined as Processor objects, we can
			 * ignore any CPUs defined as Device objects.
			 */
			if (type == ACPI_TYPE_PROCESSOR) {
				status = AcpiEvaluateObject(handle, NULL, NULL,
							    &buf);
				if (ACPI_FAILURE(status))
					fwts_warning(fw, "Could not evaluate "
						     "Processor %s", name);
				else {
					if (pr.Processor.PblkAddress)
						pblk = pr.Processor.PblkAddress;
				}
			}
		}
	}

	fwts_log_info(fw, "Using P_BLK address of 0x%" PRIx64, pblk);
	return pblk;
}

static void get_cst_c2c3(fwts_framework *fw, uint16_t *c2, uint16_t *c3)
{
	const size_t name_len = 4;
	fwts_list_link	*item;
	fwts_list *objects;

	if ((objects = fwts_acpi_object_get_names()) == NULL) {
		fwts_log_info(fw, "Cannot find any ACPI objects");
		return;
	}

	fwts_list_foreach(item, objects) {
		char *name = fwts_list_data(char*, item);
		const size_t len = strlen(name);
		if (strncmp("_CST", name + len - name_len, name_len) == 0) {
			ACPI_OBJECT_LIST arg_list;
			ACPI_BUFFER buf;
			ACPI_OBJECT *obj;
			int ret;

			arg_list.Count   = 0;
			arg_list.Pointer = NULL;

			ret = fwts_acpi_object_evaluate(fw, name, &arg_list, &buf);
			if ((ACPI_FAILURE(ret) != AE_OK) || (buf.Pointer == NULL))
				continue;

			obj = buf.Pointer;
			if ((obj->Type == ACPI_TYPE_PACKAGE) &&
			    (obj->Package.Count) &&
			    (obj->Package.Elements[0].Type == ACPI_TYPE_INTEGER) &&
			    (obj->Package.Elements[0].Integer.Value == (obj->Package.Count - 1))) {
				ACPI_OBJECT *pkg = NULL;
				uint32_t i;

				for (i = 1; i < obj->Package.Count; i++){
					if (*c2 == 0 || *c3 == 0) {
						pkg = &obj->Package.Elements[i];
						if ((uint8_t)pkg->Package.Elements[1].Integer.Value == 2)
							*c2 = (uint16_t)pkg->Package.Elements[2].Integer.Value;
						else if ((uint8_t)pkg->Package.Elements[1].Integer.Value == 3)
							*c3 = (uint16_t)pkg->Package.Elements[2].Integer.Value;
					}
				}
			}
			free(buf.Pointer);
		}
	}
	return;
}

static void acpi_table_check_fadt_p_lvl2_lat(fwts_framework *fw, uint64_t pblk, uint16_t c2)
{
	if (pblk || c2) {
		if (fadt->p_lvl2_lat <= 100)
			fwts_passed(fw,
				    "FADT P_LVL2_LAT is within proper range "
				    "at %" PRIu16, fadt->p_lvl2_lat);
		else
			fwts_warning(fw,
				     "FADT P_LVL2_LAT is > 100 (%" PRIu16 ") "
				     "but a P_BLK or _CST C2 is defined. This "
				     "implies a C2 state is not supported, but "
				     "there is a P_BLK register block or _CST "
				     "C2 defined which implies there might be "
				     "a C2 state that works. There is not "
				     "enough information to determine if this "
				     "is expected or not.",
				     fadt->p_lvl2_lat);
	} else {
		if (fadt->p_lvl2_lat <= 100)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "PLvl2LatDefinedButNotUsable",
				    "FADT P_LVL2_LAT is <= 100 (%" PRIu16 ") "
				    "which implies a C2 state is supported "
				    "but there is no P_BLK register block "
				    "or _CST C2 defined to enable the C2 "
				    "transition.",
				    fadt->p_lvl2_lat);
		else
			fwts_passed(fw,
				    "FADT P_LVL2_LAT is > 100 (%" PRIu16 ") "
				    "and no P_BLK or _CST C2 is defined.",
				    fadt->p_lvl2_lat);
	}

	return;
}

static void acpi_table_check_fadt_p_lvl3_lat(fwts_framework *fw, uint64_t pblk, uint16_t c3)
{
	if (pblk || c3) {
		if (fadt->p_lvl3_lat <= 1000)
			fwts_passed(fw,
				    "FADT P_LVL3_LAT is within proper range "
				    "at %" PRIu16, fadt->p_lvl3_lat);
		else
			fwts_warning(fw,
				     "FADT P_LVL3_LAT is > 1000 (%" PRIu16 ") "
				     "but a P_BLK or _CST C3 is defined. This "
				     "implies a C3 state is not supported, but "
				     "there is a P_BLK register block or _CST "
				     "C3 defined which implies there might be "
				     "a C3 state that works. There is not "
				     "enough information to determine if this "
				     "is expected or not.",
				     fadt->p_lvl3_lat);
	} else {
		if (fadt->p_lvl3_lat <= 1000)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "PLvl3LatDefinedButNotUsable",
				    "FADT P_LVL3_LAT is <= 1000 (%" PRIu16 ") "
				    "which implies a C3 state is supported "
				    "but there is no P_BLK register block "
				    "or _CST C3 defined to enable the C3 "
				    "transition.",
				    fadt->p_lvl3_lat);
		else
			fwts_passed(fw,
				    "FADT P_LVL3_LAT is > 1000 (%" PRIu16 ") "
				    "and no P_BLK or _CST C3 is defined.",
				    fadt->p_lvl3_lat);
	}

	return;
}

static void acpi_table_check_fadt_x_gpex_blk(fwts_framework *fw) {

	if (fwts_acpi_is_reduced_hardware(fw))
		return;

	if (fadt->x_gpe0_blk.access_width == 1)
		fwts_passed(fw, "FADT X_GPE0_BLK has correct byte access width.");
	else {
		if (!fwts_acpi_data_zero((const void *) &fadt->x_gpe0_blk, sizeof(fwts_acpi_gas)))
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"X_GPE0_BLKBadAccessWidth",
				"FADT X_GPE0_BLK Access width 0x%2.2" PRIx8
				" but it should be 1 (byte access).",
				fadt->x_gpe0_blk.access_width);
	}

	if (fadt->x_gpe1_blk.access_width == 1)
		fwts_passed(fw, "FADT X_GPE1_BLK has correct byte access width.");
	else {
		if (!fwts_acpi_data_zero((const void *) &fadt->x_gpe1_blk, sizeof(fwts_acpi_gas)))
			fwts_failed(fw, LOG_LEVEL_HIGH,
				"X_GPE1_BLKBadAccessWidth",
				"FADT X_GPE1_BLK Access width 0x%2.2" PRIx8
				" but it should be 1 (byte access).",
				fadt->x_gpe1_blk.access_width);
	}
}

static void acpi_table_check_fadt_sleep_control_reg(fwts_framework *fw)
{
	if (fwts_acpi_is_reduced_hardware(fw)) {
		if (fadt->sleep_control_reg.address == 0)
			fwts_passed(fw, "FADT SLEEP_CONTROL_REG not in use.");
		else {
			if (fadt->sleep_control_reg.register_bit_width == 8 &&
			    fadt->sleep_control_reg.register_bit_offset == 0)
				fwts_passed(fw, "FADT SLEEP_CONTROL_REG is "
					    "in use and well-defined.");
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					    "SleepControlRegHasBadGAS",
					    "FADT SLEEP_CONTROL_REG is "
					    "in use but register width or "
					    "offset is incorrect.");
		}
	} else {
		if (fadt->sleep_control_reg.address == 0)
			fwts_passed(fw, "FADT SLEEP_CONTROL_REG is null and "
				    "not available when not in reduced "
				    "hardware mode.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SleepControlRegNotAllowed",
				    "FADT SLEEP_CONTROL_REG is defined but "
				    "will be ignored when not in reduced hardware mode.");
	}
}

static void acpi_table_check_fadt_sleep_status_reg(fwts_framework *fw)
{
	if (fwts_acpi_is_reduced_hardware(fw)) {
		if (fadt->sleep_status_reg.address == 0)
			fwts_passed(fw, "FADT SLEEP_STATUS_REG not in use.");
		else {
			if (fadt->sleep_status_reg.register_bit_width == 8 &&
			    fadt->sleep_status_reg.register_bit_offset == 0)
				fwts_passed(fw, "FADT SLEEP_STATUS_REG is "
					    "in use and well-defined.");
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					    "SleepStatusRegHasBadGAS",
					    "FADT SLEEP_STATUS_REG is "
					    "in use but register width or "
					    "offset is incorrect.");
		}
	} else {
		if (fadt->sleep_status_reg.address == 0)
			fwts_passed(fw, "FADT SLEEP_STATUS_REG is null and "
				    "not available when not in reduced "
				    "hardware mode.");
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SleepStatusRegNotAllowed",
				    "FADT SLEEP_STATUS_REG is defined but "
				    "will be ignored when not in reduced hardware mode.");
	}
}

static int fadt_test1(fwts_framework *fw)
{
	acpi_table_check_fadt_firmware_ctrl(fw);
	acpi_table_check_fadt_dsdt(fw);
	acpi_table_check_fadt_reserved(fw);
	acpi_table_check_fadt_pm_profile(fw);
	acpi_table_check_fadt_reduced_hardware(fw);

	/*
	 * If a field can be tested, we call a function to do so.  If
	 * any value is reasonable and allowable, we simply log the value.
	 * For example, the SCI_INT is one byte and can be from 0..255, and
	 * there is no other info (as far as this author knows) that can be
	 * used to verify that the value is correct.
	 */
	if (!fwts_acpi_is_reduced_hardware(fw)) {
		fwts_log_info(fw, "FADT SCI_INT is %" PRIu16, fadt->sci_int);
		acpi_table_check_fadt_smi_cmd(fw);
		acpi_table_check_fadt_acpi_enable(fw);
		acpi_table_check_fadt_acpi_disable(fw);
		acpi_table_check_fadt_s4bios_req(fw);
		acpi_table_check_fadt_pstate_cnt(fw);
		acpi_table_check_fadt_pm1a_evt_blk(fw);
		acpi_table_check_fadt_pm1b_evt_blk(fw);
		acpi_table_check_fadt_pm1a_cnt_blk(fw);
		acpi_table_check_fadt_pm1b_cnt_blk(fw);
		acpi_table_check_fadt_pm2_cnt_blk(fw);
		acpi_table_check_fadt_pm_tmr_blk(fw);
		acpi_table_check_fadt_pm1_evt_len(fw);
		acpi_table_check_fadt_pm1_cnt_len(fw);
		acpi_table_check_fadt_pm2_cnt_len(fw);
		acpi_table_check_fadt_pm_tmr_len(fw);
		acpi_table_check_fadt_gpe0_blk_len(fw);
		acpi_table_check_fadt_gpe1_blk_len(fw);
		fwts_log_info(fw, "FADT GPE1_BASE is %" PRIu8, fadt->gpe1_base);
		acpi_table_check_fadt_cst_cnt(fw);

		if (fwts_acpi_init(fw) == FWTS_OK) {
			uint64_t pblk = fadt_find_p_blk(fw);
			uint16_t c2 = 0;
			uint16_t c3 = 0;

			get_cst_c2c3(fw, &c2, &c3);
			acpi_table_check_fadt_p_lvl2_lat(fw, pblk, c2);
			acpi_table_check_fadt_p_lvl3_lat(fw, pblk, c3);
			fwts_acpi_deinit(fw);
		} else {
			fwts_log_error(fw, "Cannot initialize ACPI namespace.");
			fwts_log_info(fw,
				      "Without a namespace, cannot test "
				      "values of P_LVL2_LAT or P_LVL3_LAT "
				      "for correctness.");
		}

		fwts_log_info(fw, "FADT FLUSH_SIZE is %" PRIu16,
			      fadt->flush_size);
		fwts_log_info(fw, "FADT FLUSH_STRIDE is %" PRIu16,
			      fadt->flush_stride);
		fwts_log_info(fw, "FADT DUTY_OFFSET is %" PRIu8,
			      fadt->duty_offset);
		fwts_log_info(fw, "FADT DUTY_WIDTH is %" PRIu8,
			      fadt->duty_width);
		fwts_log_info(fw, "FADT DAY_ALRM is %" PRIu8, fadt->day_alrm);
		fwts_log_info(fw, "FADT MON_ALRM is %" PRIu8, fadt->mon_alrm);
		fwts_log_info(fw, "FADT CENTURY is %" PRIu8, fadt->century);
	}

	acpi_table_check_fadt_x_gpex_blk(fw);
	acpi_table_check_fadt_sleep_control_reg(fw);
	acpi_table_check_fadt_sleep_status_reg(fw);

	/*
	 * Cannot really test the Hypervisor Vendor Identity since
	 * the value is provided by the hypervisor to the OS (as a
	 * sign that the ACPI tables have been fabricated), if it
	 * being used at all.  Or, it's set to zero.
	 */
	fwts_log_info(fw, "FADT Hypervisor Vendor Identity is %" PRIu64,
		      fadt->hypervisor_id);

	return FWTS_OK;
}

static int fadt_test2(fwts_framework *fw)
{
	uint32_t port, width, val32;
	int ret = FWTS_OK;

	if (fwts_acpi_is_reduced_hardware(fw)) {
		fwts_skipped(fw, "In reduced hardware mode, skipping test.");
		return FWTS_OK;
	}

	port = fadt->pm1a_cnt_blk;
	width = fadt->pm1_cnt_len * 8;	/* In bits */

	if ((fadt->header.revision > 1) || (fadt->header.length >= 244)) {
		/*  Sanity check sizes with extended address variants */
		fwts_log_info(fw, "FADT is greater than ACPI version 1.0");
		if ((uint64_t)port != fadt->x_pm1a_cnt_blk.address)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"FADTPM1CNTAddrMismatch",
				"32 and 64 bit versions of FADT pm1_cnt address do not match (0x%8.8x vs 0x%16.16" PRIx64 ").",
				port, fadt->x_pm1a_cnt_blk.address);
		if (width != fadt->x_pm1a_cnt_blk.register_bit_width)
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"FADTPM1CNTSizeMismatch",
				"32 and 64 bit versions of FADT pm1_cnt size do not match (0x%x vs 0x%x).",
				width, fadt->x_pm1a_cnt_blk.register_bit_width);

		port = fadt->x_pm1a_cnt_blk.address;
		width = fadt->x_pm1a_cnt_blk.register_bit_width;
	}

	switch (width) {
	case 8:
		if (ioperm(port, width/8, 1) < 0)
			ret = FWTS_ERROR;
		else {
			uint8_t val8;

			ret = fwts_inb(port, &val8);
			val32 = val8;
			ioperm(port, width/8, 0);
		}
		break;
	case 16:
		if (ioperm(port, width/8, 1) < 0)
			ret = FWTS_ERROR;
		else {
			uint16_t val16;

			ret = fwts_inw(port, &val16);
			val32 = val16;
			ioperm(port, width/8, 0);
		}
		break;
	case 32:
		if (ioperm(port, width/8, 1) < 0)
			ret = FWTS_ERROR;
		else {
			ret = fwts_inl(port, &val32);
			ioperm(port, width/8, 0);
		}
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_HIGH, "FADTPM1AInvalidWidth",
			"FADT pm1a register has invalid bit width of %" PRIu32 ".",
			width);
		return FWTS_OK;
	}

	if (ret != FWTS_OK) {
		fwts_log_error(fw, "Cannot read FADT PM1A_CNT_BLK port 0x%" PRIx32 ".", port);
		return FWTS_ERROR;
	}

	if (val32 & 0x01)
		fwts_passed(fw, "SCI_EN bit in PM1a Control Register Block is enabled.");
	else
		fwts_failed(fw, LOG_LEVEL_HIGH, "SCI_ENNotEnabled",
			"SCI_EN bit in PM1a Control Register Block is not enabled.");

	return FWTS_OK;
}

static int fadt_test3(fwts_framework *fw)
{
	if (fwts_acpi_is_reduced_hardware(fw)) {
		fwts_skipped(fw, "In reduced hardware mode, skipping test.");
		return FWTS_OK;
	}

	if ((fadt->header.revision == 1) || (fadt->header.length < 244)) {
		fwts_skipped(fw, "Header size indicates an ACPI 1.0 FADT, skipping test.");
		return FWTS_SKIP;
	}
	/*
 	 * Sanity check Reset reg, c.f. ACPICA
	 * commit 57bdfbee675cd2c0618c66882d68a6bdf9f8efc4
	 */
	if (fadt->flags & FADT_RESET_SUPPORTED) {
		if (fadt->reset_reg.register_bit_width != 8) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "FadtResetRegWidth",
				"FADT reset register is not 8 bits wide, got instead: %" PRIi8 ".",
				fadt->reset_reg.register_bit_width);
			fwts_advice(fw,
				"Section 5.2.9 Fixed ACPI Description Table (Table 5-34) of the ACPI "
				"specification states that the FADT register width should "
				"be 8 bits wide. However, recent versions of ACPICA will ignore this "
				"and default to 8 bits.");
		} else
			fwts_passed(fw, "FADT reset register width is 8 bits wide as expected.");

		if (fadt->reset_reg.register_bit_offset != 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "FadtResetRegBitOffset",
				"FADT reset register bit offset is not 0, got instead: %" PRIi8 ".",
				fadt->reset_reg.register_bit_offset);
			fwts_advice(fw,
				"Section 5.2.9 Fixed ACPI Description Table (Table 5-34) of the ACPI "
				"specification states that the FADT register bit offset should be 0.");
		} else
			fwts_passed(fw, "FADT register bit offset is 0 as expected.");

		if (fadt->header.length >= 129) {
			if ((fadt->reset_reg.address_space_id != 0) &&
					(fadt->reset_reg.address_space_id != 1) &&
					(fadt->reset_reg.address_space_id != 2)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"FADTBadRESETREG",
					"FADT RESET_REG address space ID was %"
					PRIu8 ", must be System Memory space (0), "
					"System I/O space (1), or PCI configuration "
					"space (2).",
					fadt->reset_reg.address_space_id);
				fwts_advice(fw,
					"If the FADT RESET_REG address space ID is "
					"not set correctly then ACPI writes "
					"to this register *may* nor work correctly, "
					"meaning a reboot via this mechanism may not work.");
			}
			if ((fadt->reset_value == 0) &&
					(fadt->reset_reg.address != 0))
				fwts_warning(fw, "FADT RESET_VALUE is zero, which "
					"may be incorrect, it is usually non-zero.");
		}
	} else {
		fwts_skipped(fw, "FADT flags indicates reset register not supported, skipping test.");
		return FWTS_SKIP;
	}

	return FWTS_OK;
}

static fwts_framework_minor_test fadt_tests[] = {
	{ fadt_info, "ACPI FADT Description Table flag info." },
	{ fadt_checksum, "FADT checksum test." },
	{ fadt_revision, "FADT revision test." },
	{ fadt_test1, "ACPI FADT Description Table tests." },
	{ fadt_test2, "Test FADT SCI_EN bit is enabled." },
	{ fadt_test3, "Test FADT reset register." },
	{ NULL, NULL }
};

static fwts_framework_ops fadt_ops = {
	.description = "FADT Fixed ACPI Description Table tests.",
	.init        = fadt_init,
	.minor_tests = fadt_tests
};

FWTS_REGISTER("fadt", &fadt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH |
	      FWTS_FLAG_ROOT_PRIV | FWTS_FLAG_ACPI | FWTS_FLAG_COMPLIANCE_ACPI)
#endif
