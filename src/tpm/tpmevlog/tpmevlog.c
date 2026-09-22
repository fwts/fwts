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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "fwts.h"

#include <sys/types.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fwts_tpm.h"
#include "fwts_uefi.h"

#define FWTS_TPM_DEVICE_PATH	"/sys/class/tpm"
#define FWTS_TPM_LOG_DIR_PATH	"/sys/kernel/security"
#define TPM2_MAX_DIGEST_SIZE	TPM2_SHA512_DIGEST_SIZE
#define TPM2_PCR_NAME_SIZE	12

struct tpm_hash_algorithm {
	TPM2_ALG_ID algorithm;
	uint16_t digest_size;
};

static const struct tpm_hash_algorithm tpmevlog_hash_algorithms[] = {
	{ TPM2_ALG_SHA1, TPM2_SHA1_DIGEST_SIZE },
	{ TPM2_ALG_SHA256, TPM2_SHA256_DIGEST_SIZE },
	{ TPM2_ALG_SHA384, TPM2_SHA384_DIGEST_SIZE },
	{ TPM2_ALG_SHA512, TPM2_SHA512_DIGEST_SIZE },
	{ TPM2_ALG_SHA256_192, TPM2_SHA256_192_DIGEST_SIZE },
	{ TPM2_ALG_SM3_256, TPM2_SM3_256_DIGEST_SIZE },
	{ TPM2_ALG_SHA3_256, TPM2_SHA3_256_DIGEST_SIZE },
	{ TPM2_ALG_SHA3_384, TPM2_SHA3_384_DIGEST_SIZE },
	{ TPM2_ALG_SHA3_512, TPM2_SHA3_512_DIGEST_SIZE },
	{ TPM2_ALG_SHAKE256_192, TPM2_SHAKE256_192_DIGEST_SIZE },
	{ TPM2_ALG_SHAKE256_256, TPM2_SHAKE256_256_DIGEST_SIZE },
	{ TPM2_ALG_SHAKE256_512, TPM2_SHAKE256_512_DIGEST_SIZE },
};

struct tpm_pcr_bank {
	TPM2_ALG_ID algorithm;
	const char *name;
	size_t digest_size;
	bool advertised;
	uint8_t pcr[TPM2_FIRMWARE_PCR_COUNT][TPM2_MAX_DIGEST_SIZE];
};

static struct tpm_pcr_bank tpmevlog_banks[] = {
	{ .algorithm = TPM2_ALG_SHA1, .name = "sha1", .digest_size = TPM2_SHA1_DIGEST_SIZE },
	{ .algorithm = TPM2_ALG_SHA256, .name = "sha256", .digest_size = TPM2_SHA256_DIGEST_SIZE },
	{ .algorithm = TPM2_ALG_SHA384, .name = "sha384", .digest_size = TPM2_SHA384_DIGEST_SIZE },
	{ .algorithm = TPM2_ALG_SHA512, .name = "sha512", .digest_size = TPM2_SHA512_DIGEST_SIZE },
};

static bool tpmevlog_parsed = false;
static const char *tpmevlog_event_log_path;
static const char *tpmevlog_pcrs_path;
static char tpmevlog_device_path[PATH_MAX];

static struct tpm_pcr_bank *tpmevlog_find_bank(const TPM2_ALG_ID algorithm)
{
	size_t i;

	for (i = 0; i < FWTS_ARRAY_SIZE(tpmevlog_banks); i++) {
		if (tpmevlog_banks[i].algorithm == algorithm)
			return &tpmevlog_banks[i];
	}

	return NULL;
}

static uint16_t tpmevlog_hash_digest_size(const TPM2_ALG_ID algorithm)
{
	size_t i;

	for (i = 0; i < FWTS_ARRAY_SIZE(tpmevlog_hash_algorithms); i++) {
		if (tpmevlog_hash_algorithms[i].algorithm == algorithm)
			return tpmevlog_hash_algorithms[i].digest_size;
	}

	return 0;
}

static int tpmevlog_read_pcr_bank(
	fwts_framework *fw,
	const int device_fd,
	const struct tpm_pcr_bank *bank,
	uint8_t pcrs[TPM2_FIRMWARE_PCR_COUNT][TPM2_MAX_DIGEST_SIZE])
{
	char path[PATH_MAX];
	char buffer[TPM2_MAX_DIGEST_SIZE * 2 + 2];
	int path_len;
	int bank_fd;
	int i;

	path_len = snprintf(path, sizeof(path), "pcr-%s", bank->name);
	if (path_len < 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCRBankRead",
			"Cannot format TPM %s PCR bank path: %s.", bank->name, strerror(errno));
		return FWTS_ERROR;
	}
	if ((size_t)path_len >= sizeof(path)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCRBankRead",
			"Cannot construct TPM %s PCR bank path: path is too long.", bank->name);
		return FWTS_ERROR;
	}
	bank_fd = openat(device_fd, path, O_RDONLY | O_DIRECTORY);
	if (bank_fd < 0) {
		if (errno == ENOENT)
			return FWTS_SKIP;
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCRBankRead",
			"Cannot open TPM %s PCR bank at %s: %s.",
			bank->name, path, strerror(errno));
		return FWTS_ERROR;
	}

	for (i = 0; i < TPM2_FIRMWARE_PCR_COUNT; i++) {
		char pcr_name[TPM2_PCR_NAME_SIZE];
		ssize_t n;
		size_t j;
		int fd;

		snprintf(pcr_name, sizeof(pcr_name), "%d", i);
		fd = openat(bank_fd, pcr_name, O_RDONLY);
		if (fd < 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "PCRValueRead",
				"Cannot open TPM %s PCR%d value: %s.", bank->name, i, strerror(errno));
			goto err;
		}

		do {
			n = read(fd, buffer, sizeof(buffer));
		} while (n < 0 && errno == EINTR);
		if (n < 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "PCRValueRead",
				"Cannot read TPM %s PCR%d value: %s.", bank->name, i, strerror(errno));
			(void)close(fd);
			goto err;
		}
		(void)close(fd);
		if (n != (ssize_t)(bank->digest_size * 2 + 1) || buffer[n - 1] != '\n') {
			fwts_failed(fw, LOG_LEVEL_HIGH, "PCRValueFormat",
				"TPM %s PCR%d value must contain %zu hexadecimal digits followed by a newline.",
				bank->name, i, bank->digest_size * 2);
			goto err;
		}

		for (j = 0; j < bank->digest_size * 2; j++) {
			const int c = toupper((unsigned char)buffer[j]);
			uint8_t value;

			if (!isxdigit(c)) {
				fwts_failed(fw, LOG_LEVEL_HIGH, "PCRValueFormat",
					"TPM %s PCR%d value contains a non-hexadecimal digit.", bank->name, i);
				goto err;
			}
			value = isdigit(c) ? c - '0' : c - 'A' + 10;
			if (j & 1)
				pcrs[i][j >> 1] |= value;
			else
				pcrs[i][j >> 1] = value << 4;
		}
	}

	(void)close(bank_fd);
	return FWTS_OK;
err:
	(void)close(bank_fd);
	return FWTS_ERROR;
}

static int tpmevlog_pcrindex_value_check(fwts_framework *fw, const uint32_t pcr)
{
	/*
	 * Current PCRs defined from 0 to 16, and 23 is for application support,
	 * defined from TCG PC Client Platform Firmware Profile Specification
	 * https://trustedcomputinggroup.org/resource/pc-client-specific-platform-firmware-profile-specification/
	 * 2.3.4 PCR Usage
	 */
	if ((pcr > 16) && (pcr != 23)) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCRIndexValue",
			"The PCR Index value is undefined, 0x%8.8" PRIx32 ".",
			pcr);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int tpmevlog_eventtype_check(fwts_framework *fw, fwts_list *unknown_event_types,
	const fwts_tpmlog_event_type event_type)
{
	fwts_list_link *item;
	uint32_t *value;

	switch (event_type) {
	case EV_PREBOOT_CERT:
	case EV_POST_CODE:
	case EV_UNUSED:
	case EV_NO_ACTION:
	case EV_SEPARATOR:
	case EV_ACTION:
	case EV_EVENT_TAG:
	case EV_S_CRTM_CONTENTS:
	case EV_S_CRTM_VERSION:
	case EV_CPU_MICROCODE:
	case EV_PLATFORM_CONFIG_FLAGS:
	case EV_TABLE_OF_DEVICES:
	case EV_IPL:
	case EV_IPL_PARTITION_DATA:
	case EV_NONHOST_CODE:
	case EV_NONHOST_CONFIG:
	case EV_NONHOST_INFO:
	case EV_OMIT_BOOT_DEVICE_EVENTS:
	case EV_EFI_EVENT_BASE:
	case EV_EFI_VARIABLE_DRIVER_CONFIG:
	case EV_EFI_VARIABLE_BOOT:
	case EV_EFI_BOOT_SERVICES_APPLICATION:
	case EV_EFI_BOOT_SERVICES_DRIVER:
	case EV_EFI_RUNTIME_SERVICES_DRIVER:
	case EV_EFI_GPT_EVENT:
	case EV_EFI_ACTION:
	case EV_EFI_PLATFORM_FIRMWARE_BLOB:
	case EV_EFI_HANDOFF_TABLES:
	case EV_EFI_PLATFORM_FIRMWARE_BLOB2:
	case EV_EFI_HANDOFF_TABLES2:
	case EV_EFI_VARIABLE_BOOT2:
	case EV_EFI_HCRTM_EVENT:
	case EV_EFI_VARIABLE_AUTHORITY:
	case EV_EFI_SPDM_FIRMWARE_BLOB:
	case EV_EFI_SPDM_FIRMWARE_CONFIG:
	case EV_EFI_SPDM_DEVICE_POLICY:
	case EV_EFI_SPDM_DEVICE_AUTHORITY:
	case EV_COMPACT_HASH:
		return FWTS_OK;
	default:
		break;
	}

	fwts_list_foreach(item, unknown_event_types) {
		const uint32_t *seen = fwts_list_data(const uint32_t *, item);

		if (*seen == event_type)
			return FWTS_OK;
	}
	value = malloc(sizeof(*value));
	if (!value)
		goto err;
	*value = event_type;
	if (!fwts_list_append(unknown_event_types, value)) {
		free(value);
		goto err;
	}
	fwts_warning(fw,
		"Event type 0x%8.8" PRIx32 " is not recognized by FWTS; continuing.", event_type);
	return FWTS_OK;
err:
	fwts_failed(fw, LOG_LEVEL_HIGH, "PCREventTypeTracking",
		"Cannot record unrecognized event type 0x%8.8" PRIx32 ".", event_type);
	return FWTS_ERROR;
}

static const fwts_spec_id_event_alg_sz *tpmevlog_find_algorithm(
	const fwts_spec_id_event_alg_sz *algorithms,
	const uint32_t algorithm_count,
	const TPM2_ALG_ID algorithm)
{
	uint32_t i;

	for (i = 0; i < algorithm_count; i++) {
		if (algorithms[i].algorithm_id == algorithm)
			return &algorithms[i];
	}

	return NULL;
}

static int tpmevlog_algid_check(fwts_framework *fw, const TPM2_ALG_ID hash)
{
	if (tpmevlog_hash_digest_size(hash) != 0)
		return FWTS_OK;

	fwts_failed(fw, LOG_LEVEL_HIGH, "AlgorithmID",
		"Algorithm 0x%4.4" PRIx16 " is not a TPM hash algorithm.", hash);
	return FWTS_ERROR;
}

static int tpmevlog_pcr_type_event_check(
	fwts_framework *fw,
	const uint32_t pcr,
	const fwts_tpmlog_event_type event_type,
	uint32_t event_size,
	uint8_t *event)
{

	uefi_image_load_event *ev_image_load = (uefi_image_load_event *)event;

	if (pcr == 4 && event_type == EV_EFI_BOOT_SERVICES_APPLICATION) {
		if (event_size < sizeof(uefi_image_load_event)) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "ImageLoadEventLength",
					"The length of the event is %" PRIu32 " which"
					" is smaller than the UEFI Image Load Event "
					"structure that contains DevicePath "
					"of PE/COFF image for PCR4 and type "
					"EV_EFI_BOOT_SERVICES_APPLICATION.",
					event_size);
			return FWTS_ERROR;
		}
		if (ev_image_load->length_of_device_path &&
		    ev_image_load->length_of_device_path <= sizeof(fwts_uefi_dev_path)) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "ImageLoadDevicePathLength",
					"The length of the device path is %" PRIu64
					" is smaller than DevicePath of PE/COFF image "
					"for PCR4 and type EV_EFI_BOOT_SERVICES_APPLICATION.",
					ev_image_load->length_of_device_path);
			return FWTS_ERROR;
		}
	}

	return FWTS_OK;
}

static int tpmevlog_v2_check(
	fwts_framework *fw,
	fwts_list *unknown_event_types,
	uint8_t *data,
	const size_t len)
{
	int ret = FWTS_OK;
	size_t len_remain = len;
	uint8_t *pdata = data;
	uint32_t i = 0;
	uint8_t vendor_info_size = 0;
	fwts_pc_client_pcr_event *pc_event;
	fwts_efi_spec_id_event *specid_evcent;
	fwts_spec_id_event_alg_sz *alg_sz;
	const fwts_spec_id_event_alg_sz *algorithms;
	bool separator_seen[TPM2_FIRMWARE_PCR_COUNT] = { false };
	bool startuplocality_seen = false;
	bool hcrtm_seen = false;

	/* specid_event_check */
	if (len < sizeof(fwts_pc_client_pcr_event)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
				"The length of the Specid event is %zd bytes "
				"is smaller than the PCClientPCREvent %zd bytes.",
				len_remain,
				sizeof(fwts_pc_client_pcr_event));
		return FWTS_ERROR;
	}

	pc_event = (fwts_pc_client_pcr_event *)pdata;
	ret = tpmevlog_pcrindex_value_check(fw, pc_event->pcr_index);
	if (ret != FWTS_OK)
		return ret;
	ret = tpmevlog_eventtype_check(fw, unknown_event_types, pc_event->event_type);
	if (ret != FWTS_OK)
		return ret;
	for (i = 0; i < TPM2_SHA1_DIGEST_SIZE; i++) {
		if (pc_event->digest[i] != 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvDigest",
				"The digest filed of SpecId event should be all zero.");
			fwts_tpm_data_hexdump(fw, pc_event->digest, sizeof(pc_event->digest), "Digest");
		}
	}

	pdata += sizeof(fwts_pc_client_pcr_event);
	len_remain -= sizeof(fwts_pc_client_pcr_event);

	if (len_remain < pc_event->event_data_size) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
			"Only %zu bytes remain for a %" PRIu32 "-byte SpecId event.",
			len_remain, pc_event->event_data_size);
		return FWTS_ERROR;
	}
	len_remain = pc_event->event_data_size;

	/* check the data length specid event */
	if (len_remain < sizeof(fwts_efi_spec_id_event)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
				"The length of the Specid event is %zd bytes "
				"is smaller than the SpecId event %zd bytes.",
				len_remain,
				sizeof(fwts_efi_spec_id_event));
		return FWTS_ERROR;
	}

	specid_evcent = (fwts_efi_spec_id_event *)pdata;
	if (strcmp((char *)specid_evcent->signature, FWTS_TPM_EVENTLOG_V2_SIGNATURE) != 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvSignature",
			"The signature of SpecId event is not the same as expected "
			"Spec ID Event03, got %s.",
			(char *)specid_evcent->signature);
		return FWTS_ERROR;
	}

	/*
	 * Check the platform class value which defined in TCG ACPI Specification,
	 * 0 for client platforms, 1 for server platforms.
	 */
	if (specid_evcent->platform_class > 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvPlatformClass",
			"The PlatformClass value of SpecId event is unexpected "
			"0 for client platforms, 1 for server platforms, "
			"got 0x%8.8" PRIx32 ".", specid_evcent->platform_class);
		return FWTS_ERROR;
	}

	if (specid_evcent->uintn_size < 1 || specid_evcent->uintn_size > 2) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvUINTNFields",
			"The size of the UINTN fieldsof SpecId event is unexpected "
			"0x01 indicates UINT32 and 0x02 indicates UINT64, "
			"got 0x%" PRIx8 ".", specid_evcent->uintn_size);
		return FWTS_ERROR;
	}

	if (specid_evcent->number_of_alg < 1) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvAlgNumber",
			"The number of Hash algorithms of SpecId event must "
			"be set to a value of 0x01 or greater "
			"got 0x%" PRIx32 ".", specid_evcent->number_of_alg);
		return FWTS_ERROR;
	}

	pdata += sizeof(fwts_efi_spec_id_event);
	len_remain -= sizeof(fwts_efi_spec_id_event);
	if (specid_evcent->number_of_alg >
	    len_remain / sizeof(fwts_spec_id_event_alg_sz)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
			"The SpecId event does not contain all %" PRIu32
			" advertised algorithm entries.", specid_evcent->number_of_alg);
		return FWTS_ERROR;
	}

	for (i = 0; i < FWTS_ARRAY_SIZE(tpmevlog_banks); i++) {
		tpmevlog_banks[i].advertised = false;
		memset(tpmevlog_banks[i].pcr, 0, sizeof(tpmevlog_banks[i].pcr));
	}
	algorithms = (fwts_spec_id_event_alg_sz *)pdata;
	alg_sz = (fwts_spec_id_event_alg_sz *)pdata;
	for (i = 0; i < specid_evcent->number_of_alg; i++) {
		struct tpm_pcr_bank *bank;
		uint16_t expected_digest_size;

		ret = tpmevlog_algid_check(fw, alg_sz->algorithm_id);
		if (ret != FWTS_OK)
			return ret;
		if (alg_sz->digest_size == 0) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvDigestSize",
				"Algorithm 0x%4.4" PRIx16 " has a zero-byte digest.",
				alg_sz->algorithm_id);
			return FWTS_ERROR;
		}
		expected_digest_size = tpmevlog_hash_digest_size(alg_sz->algorithm_id);
		if (alg_sz->digest_size != expected_digest_size) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvDigestSize",
				"Algorithm 0x%4.4" PRIx16 " has digest size %" PRIu16
				" rather than the expected %" PRIu16 " bytes.",
				alg_sz->algorithm_id, alg_sz->digest_size, expected_digest_size);
			return FWTS_ERROR;
		}
		if (tpmevlog_find_algorithm(algorithms, i, alg_sz->algorithm_id)) {
			fwts_failed(fw, LOG_LEVEL_HIGH, "SpecIdEvDuplicateAlg",
				"Algorithm 0x%4.4" PRIx16 " occurs more than once in the SpecId event.",
				alg_sz->algorithm_id);
			return FWTS_ERROR;
		}

		bank = tpmevlog_find_bank(alg_sz->algorithm_id);
		if (bank)
			bank->advertised = true;

		pdata += sizeof(fwts_spec_id_event_alg_sz);
		len_remain -= sizeof(fwts_spec_id_event_alg_sz);
		alg_sz = (fwts_spec_id_event_alg_sz *)pdata;
	}

	if (len_remain < sizeof(vendor_info_size)) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
			"The SpecId event does not contain VendorInfoSize.");
		return FWTS_ERROR;
	}
	vendor_info_size = *(uint8_t *)pdata;
	pdata += sizeof(vendor_info_size);
	len_remain -= sizeof(vendor_info_size);
	if (vendor_info_size > 0) {
		if (len_remain < vendor_info_size) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
					"The remain length of the Specid event is "
					"is too small (%zd bytes) for "
					"vendor info size.",
					len_remain);
			return FWTS_ERROR;
		}
		len_remain -= vendor_info_size;
		pdata += vendor_info_size;
	}

	if (len_remain != 0) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "SpecidEventLength",
			"The SpecId event contains %zu unexpected trailing bytes.", len_remain);
		return FWTS_ERROR;
	}
	len_remain = len - sizeof(fwts_pc_client_pcr_event) - pc_event->event_data_size;

	/* Check the Crypto agile log format event */
	while (len_remain > 0) {
		uint32_t event_size;
		fwts_tcg_pcr_event2 *pcr_event2;

		if (len_remain < sizeof(fwts_tcg_pcr_event2)) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2Length",
					"The length of the event2 is %zd bytes "
					"is smaller than the tcg pcr event2 %zd bytes.",
					len_remain,
					sizeof(fwts_tcg_pcr_event2));
			return FWTS_ERROR;
		}

		pcr_event2 = (fwts_tcg_pcr_event2 *)pdata;
		ret = tpmevlog_pcrindex_value_check(fw, pcr_event2->pcr_index);
		if (ret != FWTS_OK)
			return ret;
		ret = tpmevlog_eventtype_check(fw, unknown_event_types, pcr_event2->event_type);
		if (ret != FWTS_OK)
			return ret;

		pdata += sizeof(fwts_tcg_pcr_event2);
		len_remain -= sizeof(fwts_tcg_pcr_event2);
		for (i = 0; i < pcr_event2->digests_count; i++) {
			const fwts_spec_id_event_alg_sz *algorithm;
			struct tpm_pcr_bank *bank;
			TPM2_ALG_ID alg_id;
			uint16_t hash_size;

			if (len_remain < sizeof(TPM2_ALG_ID)) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2Length",
					"The event does not contain digest algorithm %" PRIu32 ".", i + 1);
				return FWTS_ERROR;
			}
			alg_id = *(TPM2_ALG_ID *)pdata;

			ret = tpmevlog_algid_check(fw, alg_id);
			if (ret != FWTS_OK)
				return ret;

			pdata += sizeof(TPM2_ALG_ID);
			len_remain -= sizeof(TPM2_ALG_ID);

			algorithm = tpmevlog_find_algorithm(
				algorithms, specid_evcent->number_of_alg, alg_id);
			if (!algorithm) {
				fwts_failed(fw, LOG_LEVEL_HIGH, "EventV2UnadvertisedAlg",
					"Event digest algorithm 0x%4.4" PRIx16
					" was not advertised by the SpecId event.", alg_id);
				return FWTS_ERROR;
			}
			hash_size = algorithm->digest_size;
			if (len_remain < hash_size) {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2HashSize",
					"Only %zu bytes remain for a %" PRIu16 "-byte event digest.",
					len_remain, hash_size);
				return FWTS_ERROR;
			}

			/*
			 * Extend the current PCR hash with the new hash from the log
			 */
			bank = tpmevlog_find_bank(alg_id);
			if (bank &&
			    pcr_event2->event_type != EV_NO_ACTION &&
			    pcr_event2->pcr_index < TPM2_FIRMWARE_PCR_COUNT) {
				/* H-CRTM reseeds PCR[0] before its digest is extended. */
				if (pcr_event2->event_type == EV_EFI_HCRTM_EVENT &&
				    pcr_event2->pcr_index == 0) {
					bank->pcr[0][bank->digest_size - 1] = TPM2_HCRTM_PCR0_LOCALITY;
					hcrtm_seen = true;
				}
				if (!fwts_tpm_extend_pcr(bank->pcr[pcr_event2->pcr_index],
						bank->digest_size, alg_id, pdata)) {
					fwts_failed(fw, LOG_LEVEL_HIGH, "EventV2ReplayAlg",
						"Cannot replay event digest algorithm 0x%4.4" PRIx16 ".", alg_id);
					return FWTS_ERROR;
				}
			}

			pdata += hash_size;
			len_remain -= hash_size;
		}

		if (len_remain < sizeof(event_size)) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2Length",
				"The event does not contain EventSize.");
			return FWTS_ERROR;
		}
		event_size = *(uint32_t *)pdata;
		pdata += sizeof(event_size);
		len_remain -= sizeof(event_size);
		if (len_remain < event_size) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2Length",
				"Only %zu bytes remain for a %" PRIu32 "-byte event payload.",
				len_remain, event_size);
			return FWTS_ERROR;
		}

		ret = tpmevlog_pcr_type_event_check(fw, pcr_event2->pcr_index,
				pcr_event2->event_type,
				event_size, pdata);
		if (ret != FWTS_OK)
			return ret;

		if ((pcr_event2->pcr_index < TPM2_FIRMWARE_PCR_COUNT) &&
		    (pcr_event2->event_type == EV_SEPARATOR))
			separator_seen[pcr_event2->pcr_index] = true;

		if (!hcrtm_seen &&
		    pcr_event2->event_type == EV_NO_ACTION &&
		    pcr_event2->pcr_index == 0 &&
		    event_size == 17 &&
		    memcmp(pdata, "StartupLocality", sizeof("StartupLocality")) == 0) {
			if (!startuplocality_seen) {
				size_t bank_index;

				for (bank_index = 0; bank_index < FWTS_ARRAY_SIZE(tpmevlog_banks);
				     bank_index++) {
					struct tpm_pcr_bank *bank = &tpmevlog_banks[bank_index];

					if (bank->advertised)
						bank->pcr[0][bank->digest_size - 1] =
							pdata[sizeof("StartupLocality")];
				}
				startuplocality_seen = true;
			} else {
				fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2StartupLocalitySeenTwice",
					"Event log contained more than one StartupLocality event");
				return FWTS_ERROR;
			}
		}

		pdata += event_size;
		len_remain -= event_size;
	}

	for (i = 0; i < TPM2_FIRMWARE_PCR_COUNT; i++) {
		if (!separator_seen[i]) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventV2SeparatorSeen",
				"PCR %" PRIu32 " did not have EV_SEPARATOR measured into it at "
				"Platform Firmware handover.", i);
			return FWTS_ERROR;
		}
	}

	tpmevlog_parsed = true;
	fwts_passed(fw, "Check TPM crypto agile event log test passed.");
	return FWTS_OK;
}

static int tpmevlog_check(fwts_framework *fw, fwts_list *unknown_event_types,
	uint8_t *data, size_t len)
{
	uint8_t *pdata = data;
	fwts_pc_client_pcr_event *pc_event = NULL;

	do {
		int ret;

		if (len < sizeof(fwts_pc_client_pcr_event)) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventLength",
					"The length of the event is %zd bytes "
					"is smaller than the PCClientPCREvent %zd bytes.",
					len,
					sizeof(fwts_pc_client_pcr_event));
			return FWTS_ERROR;
		}

		pc_event = (fwts_pc_client_pcr_event *)pdata;
		ret = tpmevlog_pcrindex_value_check(fw, pc_event->pcr_index);
		if (ret != FWTS_OK)
			return ret;
		ret = tpmevlog_eventtype_check(fw, unknown_event_types, pc_event->event_type);
		if (ret != FWTS_OK)
			return ret;

		if ((len - sizeof(fwts_pc_client_pcr_event)) < pc_event->event_data_size) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM, "EventLength",
					"The remain length of the event is %zd bytes "
					"is smaller than required event length "
					"%" PRIu32 " bytes.",
					len - sizeof(fwts_pc_client_pcr_event),
					pc_event->event_data_size);
			return FWTS_ERROR;
		}

		pdata += (sizeof(fwts_pc_client_pcr_event) + pc_event->event_data_size);
		len -= (sizeof(fwts_pc_client_pcr_event) + pc_event->event_data_size);
	} while (len > 0);

	fwts_passed(fw, "Check TPM event SHA1 log test passed.");
	return FWTS_OK;
}

static uint8_t *tpmevlog_load_file(const int fd, size_t *length)
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

static int tpmevlog_parse_file(fwts_framework *fw, const char *path)
{
	fwts_list unknown_event_types = FWTS_LIST_INIT;
	uint8_t *data;
	size_t length;
	int fd;
	int ret;

	tpmevlog_parsed = false;
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fwts_log_info(fw, "Cannot open TPM event log %s: %s.", path, strerror(errno));
		return FWTS_ABORTED;
	}
	data = tpmevlog_load_file(fd, &length);
	(void)close(fd);
	if (!data) {
		fwts_log_info(fw, "Cannot load TPM event log %s. Aborted.", path);
		return FWTS_ABORTED;
	}

	if (length >= sizeof(fwts_pc_client_pcr_event) +
		      sizeof(FWTS_TPM_EVENTLOG_V2_SIGNATURE) &&
	    memcmp(data + sizeof(fwts_pc_client_pcr_event),
		   FWTS_TPM_EVENTLOG_V2_SIGNATURE,
		   sizeof(FWTS_TPM_EVENTLOG_V2_SIGNATURE)) == 0) {
		fwts_log_info_verbatim(fw, "Crypto agile log format (TPM2.0):");
		ret = tpmevlog_v2_check(fw, &unknown_event_types, data, length);
	} else {
		fwts_log_info_verbatim(fw, "SHA1 log format (TPM1.2):");
		ret = tpmevlog_check(fw, &unknown_event_types, data, length);
	}

	fwts_list_free_items(&unknown_event_types, free);
	free(data);
	if (ret != FWTS_OK)
		tpmevlog_parsed = false;
	return ret;
}

static int tpmevlog_test1(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *tpmdir;
	bool tpm_logfile_found = false;
	int ret = FWTS_OK;

	tpmevlog_parsed = false;
	tpmevlog_device_path[0] = '\0';

	if (tpmevlog_event_log_path) {
		fwts_log_info_verbatim(fw, "%s", tpmevlog_event_log_path);
		return tpmevlog_parse_file(fw, tpmevlog_event_log_path);
	}

	if (!(dir = opendir(FWTS_TPM_LOG_DIR_PATH))) {
		fwts_log_info(fw, "Cannot find the TPM event log. Aborted.");
		return FWTS_ABORTED;
	}

	do {
		tpmdir = readdir(dir);
		if (tpmdir && strstr(tpmdir->d_name, "tpm")) {
			char path[PATH_MAX];

			fwts_log_nl(fw);
			fwts_log_info_verbatim(fw, "%s", tpmdir->d_name);

			snprintf(path, sizeof(path), FWTS_TPM_LOG_DIR_PATH "/%s/binary_bios_measurements", tpmdir->d_name);

			tpm_logfile_found = true;
			ret = tpmevlog_parse_file(fw, path);
			if (ret != FWTS_OK)
				break;
			snprintf(tpmevlog_device_path, sizeof(tpmevlog_device_path),
				FWTS_TPM_DEVICE_PATH "/%s", tpmdir->d_name);
		}
	} while (tpmdir);

	(void)closedir(dir);

	if (!tpm_logfile_found) {
		fwts_log_info(fw, "Cannot find the TPM event log. Aborted.");
		return FWTS_ABORTED;
	}
	return ret;
}

static int tpmevlog_test2(fwts_framework *fw)
{
	uint8_t pcrs_actual[TPM2_FIRMWARE_PCR_COUNT][TPM2_MAX_DIGEST_SIZE];
	const char *device_path = tpmevlog_pcrs_path;
	size_t bank_index;
	size_t banks_checked = 0;
	int device_fd;
	int i;
	int ret = FWTS_OK;

	if (!tpmevlog_parsed) {
		fwts_skipped(fw, "No TPM 2.0 event log has been parsed, skipping.");
		return FWTS_SKIP;
	}
	if (!device_path) {
		if (tpmevlog_event_log_path) {
			fwts_skipped(fw, "No PCR directory was supplied for the saved event log, skipping.");
			return FWTS_SKIP;
		}
		device_path = tpmevlog_device_path;
	}

	device_fd = open(device_path, O_RDONLY | O_DIRECTORY);
	if (device_fd < 0) {
		fwts_failed(fw, LOG_LEVEL_HIGH, "PCRDirectoryRead",
			"Cannot open TPM PCR directory %s: %s.", device_path, strerror(errno));
		return FWTS_ERROR;
	}
	fwts_log_info(fw, "Comparing PCR banks in %s.", device_path);

	for (bank_index = 0; bank_index < FWTS_ARRAY_SIZE(tpmevlog_banks); bank_index++) {
		const struct tpm_pcr_bank *replayed = &tpmevlog_banks[bank_index];

		if (!replayed->advertised)
			continue;
		ret = tpmevlog_read_pcr_bank(fw, device_fd, replayed, pcrs_actual);
		if (ret == FWTS_SKIP) {
			fwts_skipped(fw, "TPM %s PCR bank is absent from %s, skipping comparison.",
				replayed->name, device_path);
			continue;
		}
		if (ret != FWTS_OK)
			goto out;
		for (i = 0; i < TPM2_FIRMWARE_PCR_COUNT; i++) {
			if (memcmp(pcrs_actual[i], replayed->pcr[i], replayed->digest_size) != 0) {
				fwts_failed(fw, LOG_LEVEL_HIGH, "PCRsMatchEvLog",
					"%s PCR %d differs between actual value and log.", replayed->name, i);
				ret = FWTS_ERROR;
				goto out;
			}
		}
		fwts_passed(fw, "TPM %s PCRs 0-%d match the event log.",
			replayed->name, TPM2_FIRMWARE_PCR_COUNT - 1);
		banks_checked++;
	}

	if (!banks_checked) {
		fwts_skipped(fw, "No advertised PCR banks could be compared.");
		ret = FWTS_SKIP;
	} else {
		ret = FWTS_OK;
	}
out:
	(void)close(device_fd);
	return ret;
}

static int tpmevlog_init(fwts_framework *fw)
{
	if (!tpmevlog_event_log_path)
		return fwts_check_root_euid(fw, true);
	return FWTS_OK;
}

static int tpmevlog_options_handler(fwts_framework *fw, int argc,
	char * const argv[], int option_char, int long_index)
{
	FWTS_UNUSED(fw);
	FWTS_UNUSED(argc);
	FWTS_UNUSED(argv);
	FWTS_UNUSED(option_char);

	switch (long_index) {
	case 0:
		tpmevlog_event_log_path = optarg;
		break;
	case 1:
		tpmevlog_pcrs_path = optarg;
		break;
	}

	return FWTS_OK;
}

static fwts_option tpmevlog_options[] = {
	{ "tpm-event-log", "", 1, "Read a TPM event log from the specified file." },
	{ "tpm-pcrs", "", 1, "Read pcr-<algorithm>/<index> files from the specified directory." },
	{ NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test tpmevlog_tests[] = {
	{ tpmevlog_test1, "Sanity check TPM event log." },
	{ tpmevlog_test2, "Check TPM event log matches TPM PCRs." },
	{ NULL, NULL }
};

static fwts_framework_ops tpmevlog_ops = {
	.description = "Sanity check TPM event log.",
	.init = tpmevlog_init,
	.minor_tests = tpmevlog_tests,
	.options = tpmevlog_options,
	.options_handler = tpmevlog_options_handler
};

FWTS_REGISTER("tpmevlog", &tpmevlog_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)
