/*
 * Copyright (C) 2020-2026 Canonical
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

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fwts_tpm.h"

#define FWTS_TPM_LOG_DIR_PATH	"/sys/kernel/security"

static char *tpmevlogdump_evtype_to_string(const fwts_tpmlog_event_type event_type)
{
	char *str;

	switch (event_type) {
	case EV_PREBOOT_CERT:
		str = "EV_PREBOOT_CERT";
		break;
	case EV_POST_CODE:
		str = "EV_POST_CODE";
		break;
	case EV_UNUSED:
		str = "EV_UNUSED";
		break;
	case EV_NO_ACTION:
		str = "EV_NO_ACTION";
		break;
	case EV_SEPARATOR:
		str = "EV_SEPARATOR";
		break;
	case EV_ACTION:
		str = "EV_ACTION";
		break;
	case EV_EVENT_TAG:
		str = "EV_EVENT_TAG";
		break;
	case EV_S_CRTM_CONTENTS:
		str = "EV_S_CRTM_CONTENTS";
		break;
	case EV_S_CRTM_VERSION:
		str = "EV_S_CRTM_VERSION";
		break;
	case EV_CPU_MICROCODE:
		str = "EV_CPU_MICROCODE";
		break;
	case EV_PLATFORM_CONFIG_FLAGS:
		str = "EV_PLATFORM_CONFIG_FLAGS";
		break;
	case EV_TABLE_OF_DEVICES:
		str = "EV_TABLE_OF_DEVICES";
		break;
	case EV_IPL:
		str = "EV_IPL";
		break;
	case EV_IPL_PARTITION_DATA:
		str = "EV_IPL_PARTITION_DATA";
		break;
	case EV_NONHOST_CODE:
		str = "EV_NONHOST_CODE";
		break;
	case EV_NONHOST_CONFIG:
		str = "EV_NONHOST_CONFIG";
		break;
	case EV_NONHOST_INFO:
		str = "EV_NONHOST_INFO";
		break;
	case EV_OMIT_BOOT_DEVICE_EVENTS:
		str = "EV_OMIT_BOOT_DEVICE_EVENTS";
		break;
	case EV_EFI_EVENT_BASE:
		str = "EV_EFI_EVENT_BASE";
		break;
	case EV_EFI_VARIABLE_DRIVER_CONFIG:
		str = "EV_EFI_VARIABLE_DRIVER_CONFIG";
		break;
	case EV_EFI_VARIABLE_BOOT:
		str = "EV_EFI_VARIABLE_BOOT";
		break;
	case EV_EFI_BOOT_SERVICES_APPLICATION:
		str = "EV_EFI_BOOT_SERVICES_APPLICATION";
		break;
	case EV_EFI_BOOT_SERVICES_DRIVER:
		str = "EV_EFI_BOOT_SERVICES_DRIVER";
		break;
	case EV_EFI_RUNTIME_SERVICES_DRIVER:
		str = "EV_EFI_RUNTIME_SERVICES_DRIVER";
		break;
	case EV_EFI_GPT_EVENT:
		str = "EV_EFI_GPT_EVENT";
		break;
	case EV_EFI_ACTION:
		str = "EV_EFI_ACTION";
		break;
	case EV_EFI_PLATFORM_FIRMWARE_BLOB:
		str = "EV_EFI_PLATFORM_FIRMWARE_BLOB";
		break;
	case EV_EFI_HANDOFF_TABLES:
		str = "EV_EFI_HANDOFF_TABLES";
		break;
	case EV_EFI_PLATFORM_FIRMWARE_BLOB2:
		str = "EV_EFI_PLATFORM_FIRMWARE_BLOB2";
		break;
	case EV_EFI_HANDOFF_TABLES2:
		str = "EV_EFI_HANDOFF_TABLES2";
		break;
	case EV_EFI_VARIABLE_BOOT2:
		str = "EV_EFI_VARIABLE_BOOT2";
		break;
	case EV_EFI_HCRTM_EVENT:
		str = "EV_EFI_HCRTM_EVENT";
		break;
	case EV_EFI_VARIABLE_AUTHORITY:
		str = "EV_EFI_VARIABLE_AUTHORITY";
		break;
	case EV_EFI_SPDM_FIRMWARE_BLOB:
		str = "EV_EFI_SPDM_FIRMWARE_BLOB";
		break;
	case EV_EFI_SPDM_FIRMWARE_CONFIG:
		str = "EV_EFI_SPDM_FIRMWARE_CONFIG";
		break;
	case EV_EFI_SPDM_DEVICE_POLICY:
		str = "EV_EFI_SPDM_DEVICE_POLICY";
		break;
	case EV_EFI_SPDM_DEVICE_AUTHORITY:
		str = "EV_EFI_SPDM_DEVICE_AUTHORITY";
		break;
	case EV_COMPACT_HASH:
		str = "EV_COMPACT_HASH";
		break;
	default:
		str = "Unknown";
		break;
	}

	return str;
}

static char *tpmevlogdump_hash_to_string(const TPM2_ALG_ID hash)
{
	char *str;

	switch (hash) {
	case TPM2_ALG_SHA1:
		str = "SHA1";
		break;
	case TPM2_ALG_SHA256:
		str = "SHA256";
		break;
	case TPM2_ALG_SHA384:
		str = "SHA384";
		break;
	case TPM2_ALG_SHA512:
		str = "SHA512";
		break;
	default:
		str = "Unknown";
		break;
	}

	return str;
}

static char *tpmevlogdump_pcrindex_to_string(const uint32_t pcr)
{
	char *str;

	switch (pcr) {
	case 0:
		str = "SRTM, BIOS, Host Platform Extensions, Embedded Option ROMs and PI Drivers";
		break;
	case 1:
		str = "Host Platform Configuration";
		break;
	case 2:
		str = "UEFI driver and application Code";
		break;
	case 3:
		str = "UEFI driver and application Configuration and Data";
		break;
	case 4:
		str = "UEFI Boot Manager Code and Boot Attempts";
		break;
	case 5:
		str = "Boot Manager Code Configuration and Data and GPT/Partition Table";
		break;
	case 6:
		str = "Host Platform Manufacturer Specific";
		break;
	case 7:
		str = "Secure Boot Policy";
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		str = "Defined for use by the Static OS";
		break;
	case 16:
		str = "Debug";
		break;
	case 23:
		str = "Application Support";
		break;
	default:
		str = "Unknown";
		break;
	}

	return str;
}

static size_t tpmevlogdump_specid_event_dump(
	fwts_framework *fw,
	uint8_t *data,
	const size_t len)
{
	uint32_t i;
	size_t len_remain = len;
	uint8_t *pdata = data;
	uint8_t vendor_info_size;
	char *str_info;
	fwts_pc_client_pcr_event *pc_event;
	fwts_efi_spec_id_event *specid_evcent;
	fwts_spec_id_event_alg_sz *alg_sz;

	/* check the data length for dumping */
	if (len_remain < sizeof(fwts_pc_client_pcr_event)) {
		fwts_log_info(fw, "Cannot get enough length for dumping data.");
		return 0;
	}
	pc_event = (fwts_pc_client_pcr_event *)pdata;
	str_info = tpmevlogdump_pcrindex_to_string(pc_event->pcr_index);
	fwts_log_info_verbatim(fw, "PCRIndex:                    0x%8.8" PRIx32 "(%s)", pc_event->pcr_index, str_info);
	str_info = tpmevlogdump_evtype_to_string(pc_event->event_type);
	fwts_log_info_verbatim(fw, "EventType:                   0x%8.8" PRIx32 "(%s)", pc_event->event_type, str_info);
	fwts_tpm_data_hexdump(fw, pc_event->digest, sizeof(pc_event->digest), "Digest");
	fwts_log_info_verbatim(fw, "EventSize:                   0x%8.8" PRIx32, pc_event->event_data_size);

	pdata += sizeof(fwts_pc_client_pcr_event);
	len_remain -= sizeof(fwts_pc_client_pcr_event);

	/* check the data length for dumping */
	if (len_remain < sizeof(fwts_efi_spec_id_event)) {
		fwts_log_info(fw, "Cannot get enough length for dumping data.");
		return 0;
	}

	specid_evcent = (fwts_efi_spec_id_event *)pdata;
	fwts_log_info_verbatim(fw, "EfiSpecIdEvent:");
	fwts_log_info_verbatim(fw, "  Signature:                 %s", (char *)specid_evcent->signature);
	fwts_log_info_verbatim(fw, "  platformClass:             0x%8.8" PRIx32, specid_evcent->platform_class);
	fwts_log_info_verbatim(fw, "  specVersionMinor:          0x%" PRIx8, specid_evcent->spec_version_minor);
	fwts_log_info_verbatim(fw, "  specVersionMajor:          0x%" PRIx8, specid_evcent->spec_version_major);
	fwts_log_info_verbatim(fw, "  specErrata:                0x%" PRIx8, specid_evcent->spec_errata);
	fwts_log_info_verbatim(fw, "  uintnSize:                 0x%" PRIx8, specid_evcent->uintn_size);
	fwts_log_info_verbatim(fw, "  NumberOfAlgorithms:        0x%8.8" PRIx32, specid_evcent->number_of_alg);

	pdata += sizeof(fwts_efi_spec_id_event);
	len_remain -= sizeof(fwts_efi_spec_id_event);
	alg_sz = (fwts_spec_id_event_alg_sz *)pdata;
	for (i = 0; i < specid_evcent->number_of_alg; i++) {
		/* check the data length for dumping */
		if (len_remain < sizeof(fwts_spec_id_event_alg_sz)) {
			fwts_log_info(fw, "Cannot get enough length for dumping data.");
			return 0;
		}
		str_info = tpmevlogdump_hash_to_string(alg_sz->algorithm_id);
		fwts_log_info_verbatim(fw, "  digestSizes[%d].AlgId:      0x%4.4" PRIx16 "(%s)", i, alg_sz->algorithm_id, str_info);
		fwts_log_info_verbatim(fw, "  digestSizes[%d].DigestSize: %" PRIu16, i, alg_sz->digest_size);
		pdata += sizeof(fwts_spec_id_event_alg_sz);
		len_remain -= sizeof(fwts_spec_id_event_alg_sz);
		alg_sz = (fwts_spec_id_event_alg_sz *)pdata;
	}

	vendor_info_size = *(uint8_t *)pdata;
	fwts_log_info_verbatim(fw, "  vendorInfoSize:            0x%" PRIx8, vendor_info_size);
	pdata += sizeof(vendor_info_size);
	len_remain -= sizeof(vendor_info_size);
	if (vendor_info_size > 0) {
		/* check the data length for dumping */
		if (len_remain < vendor_info_size) {
			fwts_log_info(fw, "Cannot get enough length for dumping data.");
			return 0;
		}
		fwts_tpm_data_hexdump(fw, pdata, vendor_info_size, "  vendorInfo");
		len_remain -= vendor_info_size;
	}

	return len_remain;
}

static size_t tpmevlogdump_event_v2_dump(
	fwts_framework *fw,
	uint8_t *data,
	const size_t len)
{
	uint32_t i;
	uint32_t event_size;
	uint8_t *pdata = data;
	size_t len_remain = len;
	char *str_info;
	fwts_tcg_pcr_event2 *pcr_event2;

	/* check the data length for dumping */
	if (len_remain < sizeof(fwts_tcg_pcr_event2)) {
		fwts_log_info(fw, "Cannot get enough length for dumping data.");
		return 0;
	}
	pcr_event2 = (fwts_tcg_pcr_event2 *)pdata;
	str_info = tpmevlogdump_pcrindex_to_string(pcr_event2->pcr_index);
	fwts_log_info_verbatim(fw, "PCRIndex:           0x%8.8" PRIx32 "(%s)", pcr_event2->pcr_index, str_info);
	str_info = tpmevlogdump_evtype_to_string(pcr_event2->event_type);
	fwts_log_info_verbatim(fw, "EventType:          0x%8.8" PRIx32 "(%s)", pcr_event2->event_type, str_info);
	fwts_log_info_verbatim(fw, "Digests Count :     0x%8.8" PRIx32, pcr_event2->digests_count);
	pdata += sizeof(fwts_tcg_pcr_event2);
	len_remain -= sizeof(fwts_tcg_pcr_event2);

	for (i = 0; i < pcr_event2->digests_count; i++) {
		uint8_t hash_size = 0;
		const TPM2_ALG_ID alg_id = *(TPM2_ALG_ID *)pdata;

		/* check the data length for dumping */
		if (len_remain < sizeof(TPM2_ALG_ID)) {
			fwts_log_info(fw, "Cannot get enough length for dumping data.");
			return 0;
		}
		str_info = tpmevlogdump_hash_to_string(alg_id);
		fwts_log_info_verbatim(fw, "  Digests[%d].AlgId: 0x%4.4" PRIx16 "(%s)", i, alg_id, str_info);
		hash_size = fwts_tpm_get_hash_size(alg_id);
		if (!hash_size) {
			fwts_log_info(fw, "Unknown hash algorithm. Aborted.");
			return 0;
		}
		pdata += sizeof(TPM2_ALG_ID);
		len_remain -= sizeof(TPM2_ALG_ID);
		/* check the data length for dumping */
		if (len_remain < sizeof(TPM2_ALG_ID)) {
			fwts_log_info(fw, "Cannot get enough length for dumping data.");
			return 0;
		}
		fwts_tpm_data_hexdump(fw, pdata, hash_size, "  Digest");
		pdata += hash_size;
		len_remain -= hash_size;
	}

	event_size = *(uint32_t *)pdata;
	/* check the data length for dumping */
	if (len_remain < event_size + sizeof(event_size)) {
		fwts_log_info(fw, "Cannot get enough length for dumping data.");
		return 0;
	}
	pdata += sizeof(event_size);
	len_remain -= sizeof(event_size);

	fwts_log_info_verbatim(fw, "  EventSize:        %" PRIu32, event_size);
	if (event_size > 0) {
		fwts_tpm_data_hexdump(fw, pdata, event_size, "  Event");
		len_remain -= event_size;
	}

	return len_remain;
}

static void tpmevlogdump_parser(
	fwts_framework *fw,
	uint8_t *data,
	const size_t len)
{
	size_t t_len = len;
	size_t len_remain = 0;
	uint8_t *pdata = data;

	len_remain = tpmevlogdump_specid_event_dump(fw, pdata, t_len);
	fwts_log_nl(fw);
	pdata += (t_len - len_remain);
	while (len_remain > 0) {
		t_len = len_remain;
		len_remain = tpmevlogdump_event_v2_dump(fw, pdata, t_len);
		pdata += (t_len - len_remain);
		fwts_log_nl(fw);
	}
	return;
}

static void tpmevlogdump_event_dump(
	fwts_framework *fw,
	uint8_t *data,
	size_t len)
{
	uint8_t *pdata = data;
	fwts_pc_client_pcr_event *pc_event = NULL;

	while (len > 0) {
		char *str_info;

		/* check the data length for dumping */
		if (len < sizeof(fwts_pc_client_pcr_event)) {
			fwts_log_info(fw,
				"Log event data is too small (%zd bytes) "
				"than a TCG PC Client PCR event structure "
				"(%zd bytes).",
				len, sizeof(fwts_pc_client_pcr_event));
			return;
		}

		pc_event = (fwts_pc_client_pcr_event *)pdata;

		str_info = tpmevlogdump_pcrindex_to_string(pc_event->pcr_index);
		fwts_log_info_verbatim(fw, "PCRIndex:	0x%8.8" PRIx32 "(%s)", pc_event->pcr_index, str_info);
		str_info = tpmevlogdump_evtype_to_string(pc_event->event_type);
		fwts_log_info_verbatim(fw, "EventType:	0x%8.8" PRIx32 "(%s)", pc_event->event_type, str_info);
		fwts_tpm_data_hexdump(fw, pc_event->digest, sizeof(pc_event->digest), "Digest");
		fwts_log_info_verbatim(fw, "EventSize:	0x%8.8" PRIx32, pc_event->event_data_size);
		if (pc_event->event_data_size > 0)
			fwts_tpm_data_hexdump(fw, pc_event->event, pc_event->event_data_size, "Event");
		pdata += (sizeof(fwts_pc_client_pcr_event) + pc_event->event_data_size);
		len -= (sizeof(fwts_pc_client_pcr_event) + pc_event->event_data_size);
	}
	return;
}


static uint8_t *tpmevlogdump_load_file(const int fd, size_t *length)
{
	uint8_t *ptr = NULL, *tmp;
	size_t size = 0;
	char buffer[4096];

	*length = 0;

	for (;;) {
		const ssize_t n = read(fd, buffer, sizeof(buffer));

		if (n == 0)
			break;
		if (n < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				free(ptr);
				return NULL;
			}
			continue;
		}
		if (n > (ssize_t)sizeof(buffer))
			goto err;
		if (size + n > 0xffffffff)
			goto err;

		if ((tmp = (uint8_t*)realloc(ptr, size + n + 1)) == NULL) {
			free(ptr);
			return NULL;
		}
		ptr = tmp;
		memcpy(ptr + size, buffer, n);
		size += n;
	}

	if (!ptr || !size)
		goto err_no_data;

	*length = size;
	return ptr;

err:
	free(ptr);
err_no_data:
	*length = 0;
	return NULL;
}

static int tpmevlogdump_test1(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *tpmdir;
	bool tpm_logfile_found = false;

	if (!(dir = opendir(FWTS_TPM_LOG_DIR_PATH))) {
		fwts_log_info(fw, "Cannot find the tpm event log. Aborted.");
		return FWTS_ABORTED;
	}

	do {
		tpmdir = readdir(dir);
		if (tpmdir && strstr(tpmdir->d_name, "tpm")) {
			char path[PATH_MAX];
			int fd;
			size_t length;

			fwts_log_nl(fw);
			fwts_log_info_verbatim(fw, "%s", tpmdir->d_name);

			snprintf(path, sizeof(path), FWTS_TPM_LOG_DIR_PATH "/%s/binary_bios_measurements", tpmdir->d_name);

			if ((fd = open(path, O_RDONLY)) >= 0) {
				uint8_t *data;

				data = tpmevlogdump_load_file(fd, &length);
				tpm_logfile_found = true;
				if (data == NULL) {
					fwts_log_info(fw, "Cannot load the tpm event logs. Aborted.");
					(void)closedir(dir);
					(void)close(fd);
					return FWTS_ABORTED;
				} else {
					/* check if the TPM2 eventlog */
					if (strstr((char *)(data + sizeof(fwts_pc_client_pcr_event)), FWTS_TPM_EVENTLOG_V2_SIGNATURE)) {
						fwts_log_info_verbatim(fw, "Crypto agile log format (TPM2.0):");
						tpmevlogdump_parser(fw, data, length);
					} else {
						fwts_log_info_verbatim(fw, "SHA1 log format (TPM1.2):");
						(void)tpmevlogdump_event_dump(fw, data, length);
					}
					free(data);
				}
				(void)close(fd);
			}
		}
	} while (tpmdir);

	(void)closedir(dir);

	if (!tpm_logfile_found) {
		fwts_log_info(fw, "Cannot find the tpm event log. Aborted.");
		return FWTS_ABORTED;
	}
	return FWTS_OK;
}

static fwts_framework_minor_test tpmevlogdump_tests[] = {
	{ tpmevlogdump_test1, "Dump Tpm2 Event Log." },
	{ NULL, NULL }
};

static fwts_framework_ops tpmevlogdump_ops = {
	.description = "Dump Tpm2 Event Log.",
	.minor_tests = tpmevlogdump_tests
};

FWTS_REGISTER("tpmevlogdump", &tpmevlogdump_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_UTILS | FWTS_FLAG_ROOT_PRIV)
