/*
 * Copyright (C) 2013-2026 Canonical
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

#if defined(FWTS_HAS_UEFI)

#include <stddef.h>
#include <inttypes.h>
#include <sys/ioctl.h>

#include "fwts_uefi.h"
#include "sbkeydefs.h"

#include "fwts_efi_runtime.h"
#include "fwts_efi_module.h"

static int fd;

typedef void (*securebootcert_func)(fwts_framework *fw, fwts_uefi_var *var, char *varname);

typedef struct {
	char *description;		/* UEFI var */
	securebootcert_func	func;	/* Function to dump this variable */
} securebootcert_info;

typedef struct _EFI_SIGNATURE_LIST {
	EFI_GUID	SignatureType;
	uint32_t	SignatureListSize;
	uint32_t	SignatureHeaderSize;
	uint32_t	SignatureSize;
} __attribute__((packed)) EFI_SIGNATURE_LIST;

#define VAR_SECUREBOOT_FOUND	(1 << 0)
#define VAR_SETUPMODE_FOUND	(1 << 1)
#define VAR_DB_FOUND		(1 << 2)
#define VAR_KEK_FOUND		(1 << 3)
#define VAR_AUDITMODE_FOUND	(1 << 4)
#define VAR_DEPLOYEDMODE_FOUND	(1 << 5)

#define EFI_IMAGE_SECURITY_DATABASE_GUID \
{ \
	0xd719b2cb, 0x3d3a, 0x4596, { 0xa3, 0xbc, 0xda, \
						0xd0, 0x0e, 0x67, 0x65, 0x6f} \
}

static uint8_t var_found;
static bool securebooted = false;
static bool deployed = false;

static EFI_GUID global_guid = EFI_GLOBAL_VARIABLE;

static uint16_t varauditmode[] = {'A', 'u', 'd', 'i', 't', 'M', 'o', 'd', 'e', '\0'};
static uint16_t vardeploymode[] = {'D', 'e', 'p', 'l', 'o', 'y', 'e', 'd', 'M', 'o', 'd', 'e', '\0'};

static bool compare_guid(EFI_GUID *guid1, uint8_t *guid2)
{
	bool ident = true;
	uint32_t data1 = guid2[0] + (guid2[1] << 8) + (guid2[2] << 16) + (guid2[3] << 24);
	uint16_t data2 = guid2[4] + (guid2[5] << 8);
	uint16_t data3 = guid2[6] + (guid2[7] << 8);

	if ((guid1->Data1 != data1) || (guid1->Data2 != data2) || (guid1->Data3 != data3))
		ident = false;
	else {
		int i;

		for (i = 0; i < 8; i++) {
			if (guid1->Data4[i] != guid2[i+8])
				ident = false;
		}
	}
	return ident;
}

static void securebootcert_secure_boot(fwts_framework *fw, fwts_uefi_var *var, char *varname)
{
	bool ident = false;
	EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;

	if (strcmp(varname, "SecureBoot"))
		return;

	var_found |= VAR_SECUREBOOT_FOUND;
	ident = compare_guid(&global_var_guid, var->guid);

	if (!ident) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableGUIDInvalid",
			"The secure boot variable %s GUID invalid.", varname);
		return;
	}
	if (var->datalen != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableSizeInvalid",
			"The secure boot variable %s size invalid.", varname);
		return;
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = " (Secure Boot Mode Off)";
			break;
		case 1:
			mode = " (Secure Boot Mode On)";
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableDataInvalid",
				"The secure boot variable data invalid.");
			return;
		}
		if (value == 1)
			securebooted = true;
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2x%s.", value, mode);
		fwts_passed(fw, "Secure boot relative variable %s check passed.", varname);
	}
}

static void securebootcert_setup_mode(fwts_framework *fw, fwts_uefi_var *var, char *varname)
{

	bool ident = false;
	EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;

	if (strcmp(varname, "SetupMode"))
		return;

	var_found |= VAR_SETUPMODE_FOUND;
	ident = compare_guid(&global_var_guid, var->guid);

	if (!ident) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableGUIDInvalid",
			"The secure boot variable %s GUID invalid.", varname);
		return;
	}
	if (var->datalen != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableSizeInvalid",
			"The secure boot variable %s size invalid.", varname);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = "";
			break;
		case 1:
			mode = " (Setup Mode)";
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableDataInvalid",
				"The secure boot variable %s data invalid.", varname);
			return;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2" PRIx8 "%s.", value, mode);
		fwts_passed(fw, "Secure boot relative variable %s check passed.", varname);
	}
}

static void securebootcert_audit_mode(fwts_framework *fw, fwts_uefi_var *var, char *varname)
{

	bool ident = false;
	EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;

	if (strcmp(varname, "AuditMode"))
		return;

	var_found |= VAR_AUDITMODE_FOUND;
	ident = compare_guid(&global_var_guid, var->guid);

	if (!ident) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableGUIDInvalid",
			"The secure boot variable %s GUID invalid.", varname);
		return;
	}
	if (var->datalen != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableSizeInvalid",
			"The secure boot variable %s size invalid.", varname);
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = "";
			break;
		case 1:
			mode = " (Audit Mode)";
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableDataInvalid",
				"The secure boot variable %s data invalid.", varname);
			return;
		}
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2" PRIx8 "%s.", value, mode);
		fwts_passed(fw, "Secure boot relative variable %s check passed.", varname);
	}
}

static void securebootcert_deployed_mode(fwts_framework *fw, fwts_uefi_var *var, char *varname)
{
	bool ident = false;
	EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;

	if (strcmp(varname, "DeployedMode"))
		return;

	var_found |= VAR_DEPLOYEDMODE_FOUND;
	ident = compare_guid(&global_var_guid, var->guid);

	if (!ident) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableGUIDInvalid",
			"The secure boot variable %s GUID invalid.", varname);
		return;
	}
	if (var->datalen != 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableSizeInvalid",
			"The secure boot variable %s size invalid.", varname);
		return;
	} else {
		char *mode;
		uint8_t value = (uint8_t)var->data[0];

		switch (value) {
		case 0:
			mode = "";
			break;
		case 1:
			mode = " (Deployed Mode On)";
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableDataInvalid",
				"The secure boot variable data invalid.");
			return;
		}
		if (value == 1)
			deployed = true;
		fwts_log_info_verbatim(fw, "  Value: 0x%2.2x%s.", value, mode);
		fwts_passed(fw, "Secure boot relative variable %s check passed.", varname);
	}
}

static inline bool check_addr_overflow(
	const void *var_data_addr,
	const size_t len)
{
        return (len > ~(uintptr_t)0 - (uintptr_t)var_data_addr);
}

static bool check_sigdb_presence(
	uint8_t *var_data,
	const size_t datalen,
	const uint8_t *key,
	const uint32_t key_len)
{
	uint8_t *var_data_addr;
	EFI_SIGNATURE_LIST siglist;
	size_t i = 0;
	EFI_GUID cert_x509_guid = EFI_CERT_X509_GUID;
	bool key_found = false;

	if (datalen < sizeof(siglist))
		return key_found;

	for (var_data_addr = var_data; var_data_addr < var_data + datalen; ) {
		siglist = *((EFI_SIGNATURE_LIST *)var_data_addr);

		/* check for potential overflow */
		if (check_addr_overflow(var_data_addr, siglist.SignatureListSize))
			break;

		if (var_data_addr + siglist.SignatureListSize > var_data + datalen)
			break;

		if (siglist.SignatureHeaderSize > siglist.SignatureListSize) {
			var_data_addr += siglist.SignatureListSize;
			continue;
		}

		if (memcmp(&siglist.SignatureType, &cert_x509_guid, sizeof(EFI_GUID)) != 0) {
			var_data_addr += siglist.SignatureListSize;
			continue;
		}

		var_data_addr += sizeof(siglist) + siglist.SignatureHeaderSize;

		if (key_len != (siglist.SignatureSize - sizeof(EFI_GUID))) {
			var_data_addr += siglist.SignatureSize;
			continue;
		}

		var_data_addr += sizeof(EFI_GUID);

		for (i = 0; i < key_len; i++) {
			if (*((uint8_t *)var_data_addr+i) != key[i])
				break;
		}
		var_data_addr += siglist.SignatureSize;

		if (i == key_len) {
			key_found = true;
			return key_found;
		}
	}
	return key_found;
}

static void securebootcert_data_base(fwts_framework *fw, fwts_uefi_var *var, char *varname)
{

	bool ident = false;
	EFI_GUID image_security_var_guid = EFI_IMAGE_SECURITY_DATABASE_GUID;

	if (strcmp(varname, "db"))
		return;

	var_found |= VAR_DB_FOUND;
	ident = compare_guid(&image_security_var_guid, var->guid);

	if (!ident) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableGUIDInvalid",
			"The secure boot variable %s GUID invalid.", varname);
		return;
	}

	fwts_log_info_verbatim(fw, "Check Microsoft UEFI CA certificate presence in %s", varname);
	if (check_sigdb_presence(var->data, var->datalen, ms_uefi_ca_2011_key, ms_uefi_ca_2011_key_len))
		fwts_passed(fw, "MS UEFI CA 2011 key check passed.");
	else {
		fwts_warning(fw, "The Microsoft UEFI CA certificate not found.");
		fwts_advice(fw,
			"Most Linux distributions use shim as a first stage "
			"bootloader which was signed by Microsoft UEFI CA. "
			"Without Microsoft UEFI CA certificate in DB, the "
			"system might not be able to boot up with secure boot "
			"enabled on these distributions.");
	}
}

static void securebootcert_key_ex_key(fwts_framework *fw, fwts_uefi_var *var, char *varname)
{

	bool ident = false;
	fwts_release *release;
	EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;

	if (strcmp(varname, "KEK"))
		return;

	var_found |= VAR_KEK_FOUND;
	ident = compare_guid(&global_var_guid, var->guid);

	if (!ident) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableGUIDInvalid",
			"The secure boot variable %s GUID invalid.", varname);
		return;
	}

	release = fwts_release_get();
	if (release == NULL) {
		fwts_skipped(fw, "Cannot determine system, stop checking the Master CA certificate.");
		return;
	}

	if (strcmp(release->distributor, "Ubuntu") != 0) {
		fwts_skipped(fw, "Not a Ubuntu system, skipping the Ubuntu Master CA certificate check.");
		fwts_release_free(release);
		return;
	}
	fwts_release_free(release);

	fwts_log_info_verbatim(fw, "Check Ubuntu master CA certificate presence in %s", varname);
	if (check_sigdb_presence(var->data, var->datalen, ubuntu_key, ubuntu_key_len))
		fwts_passed(fw, "Ubuntu UEFI CA 2011 key check passed.");
	else {
		fwts_log_info_verbatim(fw, "No Ubuntu master CA certificate presence in %s", varname);
		fwts_infoonly(fw);
	}
}

static const securebootcert_info securebootcert_info_table[] = {
	{ "SecureBoot",		securebootcert_secure_boot },
	{ "SetupMode",		securebootcert_setup_mode },
	{ "db",			securebootcert_data_base },
	{ "KEK",		securebootcert_key_ex_key },
	{ "AuditMode",		securebootcert_audit_mode },
	{ "DeployedMode",	securebootcert_deployed_mode },
	{ NULL, NULL }
};

static void securebootcert_var(fwts_framework *fw, fwts_uefi_var *var)
{
	char varname[512];
	char guid_str[37];
	const securebootcert_info *info;

	fwts_uefi_get_varname(varname, sizeof(varname), var);

	for (info = securebootcert_info_table; info->description != NULL; info++) {
		if (strcmp(varname, info->description) == 0) {
			fwts_log_info_verbatim(fw, "The %s variable check.", varname);
			fwts_guid_buf_to_str(var->guid, guid_str, sizeof(guid_str));
			fwts_log_info_verbatim(fw, "  GUID: %s", guid_str);
			fwts_log_info_verbatim(fw, "  Attr: 0x%x (%s).", var->attributes,
							fwts_uefi_attribute_info(var->attributes));
			info->func(fw, var, varname);
			fwts_log_nl(fw);
			return;
		}
	}
}

static int securebootcert_init(fwts_framework *fw)
{

	if (fwts_lib_efi_runtime_module_init(fw, &fd) == FWTS_ABORTED)
		return FWTS_ABORTED;

	if (!fwts_uefi_efivars_iface_exist()) {
		fwts_log_info(fw, "Cannot detect efivars interface. Aborted.");
		return FWTS_ABORTED;
	}

	return FWTS_OK;
}

static int securebootcert_deinit(fwts_framework *fw)
{
	fwts_lib_efi_runtime_close(fd);
	fwts_lib_efi_runtime_unload_module(fw);

	return FWTS_OK;
}

static int securebootcert_test1(fwts_framework *fw)
{
	fwts_list name_list;

	if (fwts_uefi_get_variable_names(&name_list) == FWTS_ERROR) {
		fwts_log_info(fw, "Cannot find any UEFI variables.");
	} else {
		fwts_list_link *item;

		fwts_list_foreach(item, &name_list) {
			fwts_uefi_var var;
			char *name = fwts_list_data(char *, item);

			if (fwts_uefi_get_variable(name, &var) == FWTS_OK) {
				securebootcert_var(fw, &var);
				fwts_uefi_free_variable(&var);
			}
		}
	}

	/* check all the secure boot variables be found */
	if (!(var_found & VAR_SECUREBOOT_FOUND))
		fwts_warning(fw, "The secure boot variable SecureBoot not found.");
	if (!(var_found & VAR_SETUPMODE_FOUND))
		fwts_warning(fw, "The secure boot variable SetupMode not found.");
	if (!(var_found & VAR_AUDITMODE_FOUND)) {
		fwts_warning(fw, "The secure boot variable AuditMode not found.");
		fwts_advice(fw,
			"AuditMode global variable is defined in the UEFI "
			"Specification 2.6 for new secure boot architecture. "
			"It may because the firmware hasn't been updated to "
			"support the UEFI Specification 2.6.");
	}
	if (!(var_found & VAR_DEPLOYEDMODE_FOUND)) {
		fwts_warning(fw, "The secure boot variable DeployedMode not found.");
		fwts_advice(fw,
			"DeployedMode global variable is defined in the UEFI "
			"Specification 2.6 for new secure boot architecture. "
			"It may because the firmware hasn't been updated to "
			"support the UEFI Specification 2.6.");
	}
	if (securebooted || deployed) {
		if (!(var_found & VAR_DB_FOUND))
			fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableNotFound",
				"Secureboot or deployed mode on, but the variable DB not found.");
		if (!(var_found & VAR_KEK_FOUND))
			fwts_failed(fw, LOG_LEVEL_HIGH, "SecureBootCertVariableNotFound",
				"Secureboot or deployed mode on, but the variable KEK not found.");
	} else {
		if (!(var_found & VAR_DB_FOUND))
			fwts_log_info(fw, "Not in readiness for secureboot, variable DB not found.");
		if (!(var_found & VAR_KEK_FOUND))
			fwts_log_info(fw, "Not in readiness for secureboot, variable KEK not found.");
	}

	fwts_uefi_free_variable_names(&name_list);

	return FWTS_OK;
}

static int securebootcert_setvar(
	fwts_framework *fw,
	const uint32_t attributes,
	uint16_t *varname,
	EFI_GUID *guid,
	uint8_t *data)
{
	long ioret;
	struct efi_setvariable setvariable;

	uint64_t status = ~0ULL;
	uint64_t datasize = 1;

	setvariable.VariableName = varname;
	setvariable.VendorGuid = guid;
	setvariable.Attributes = attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = &status;
	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	if (ioret == -1) {
		if (status == EFI_OUT_OF_RESOURCES) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"Run out of resources for SetVariable "
				"UEFI runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware may reclaim some resources "
				"after rebooting. Reboot and test "
				"again may be helpful to continue "
				"the test.");
			return FWTS_SKIP;
		}
	}

	if (status == EFI_SUCCESS) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFISecurebooCertVar",
			"Variable is ready only, return status of setvariable "
			"should not EFI_SUCCESS.");
	} else
		fwts_passed(fw, "Variable read-only test passed.");
	return FWTS_OK;
}

static int securebootcert_test2(fwts_framework *fw)
{
	int ret;
	uint8_t data = 0;
	static uint32_t attributes = FWTS_UEFI_VAR_BOOTSERVICE_ACCESS |
					FWTS_UEFI_VAR_RUNTIME_ACCESS;

	if (!(var_found & VAR_AUDITMODE_FOUND)) {
		fwts_skipped(fw,
			"No AuditMode variable found, skip the variable test.");
		return FWTS_SKIP;
	}

	fwts_log_info(fw, "AuditMode variable read-only test, set to 0.");
	ret = securebootcert_setvar(fw, attributes, varauditmode, &global_guid, &data);
	if (ret != FWTS_OK)
		return ret;

	data = 1;
	fwts_log_info(fw, "AuditMode variable read-only test, set to 1.");
	ret = securebootcert_setvar(fw, attributes, varauditmode, &global_guid, &data);
	if (ret != FWTS_OK)
		return ret;

	if (!(var_found & VAR_AUDITMODE_FOUND)) {
		fwts_skipped(fw,
			"No DeployedMode variable found, skip the variable test.");
		return FWTS_SKIP;
	}

	data = 0;
	fwts_log_info(fw, "DeployedMode variable read-only test, set to 0.");
	ret = securebootcert_setvar(fw, attributes, vardeploymode, &global_guid, &data);
	if (ret != FWTS_OK)
		return ret;

	data = 1;
	fwts_log_info(fw, "DeployedMode variable read-only test, set to 1.");
	ret = securebootcert_setvar(fw, attributes, vardeploymode, &global_guid, &data);
	if (ret != FWTS_OK)
		return ret;

	return FWTS_OK;
}

static fwts_framework_minor_test securebootcert_tests[] = {
	{ securebootcert_test1, "UEFI secure boot test." },
	{ securebootcert_test2, "UEFI secure boot variable test." },
	{ NULL, NULL }
};

static fwts_framework_ops securebootcert_ops = {
	.description = "UEFI secure boot test.",
	.init        = securebootcert_init,
	.deinit	     = securebootcert_deinit,
	.minor_tests = securebootcert_tests
};

FWTS_REGISTER("securebootcert", &securebootcert_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UEFI | FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV)

#endif
