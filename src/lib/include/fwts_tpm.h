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

#ifndef __FWTS_TPM_H__
#define __FWTS_TPM_H__

PRAGMA_PUSH
PRAGMA_PACK_WARN_OFF

#define FWTS_TPM_EVENTLOG_V2_SIGNATURE	"Spec ID Event03"

/* define from tpm2-tss */
#define TPM2_SHA_DIGEST_SIZE     20
#define TPM2_SHA1_DIGEST_SIZE    20
#define TPM2_SHA256_DIGEST_SIZE  32
#define TPM2_SHA384_DIGEST_SIZE  48
#define TPM2_SHA512_DIGEST_SIZE  64

/* Number of PCRs used for firmware, rather than OS, measurements */
#define TPM2_FIRMWARE_PCR_COUNT 8

typedef uint16_t TPM2_ALG_ID;
#define TPM2_ALG_ERROR               ((TPM2_ALG_ID) 0x0000)
#define TPM2_ALG_RSA                 ((TPM2_ALG_ID) 0x0001)
#define TPM2_ALG_TDES                ((TPM2_ALG_ID) 0x0003)
#define TPM2_ALG_SHA                 ((TPM2_ALG_ID) 0x0004)
#define TPM2_ALG_SHA1                ((TPM2_ALG_ID) 0x0004)
#define TPM2_ALG_HMAC                ((TPM2_ALG_ID) 0x0005)
#define TPM2_ALG_AES                 ((TPM2_ALG_ID) 0x0006)
#define TPM2_ALG_MGF1                ((TPM2_ALG_ID) 0x0007)
#define TPM2_ALG_KEYEDHASH           ((TPM2_ALG_ID) 0x0008)
#define TPM2_ALG_XOR                 ((TPM2_ALG_ID) 0x000A)
#define TPM2_ALG_SHA256              ((TPM2_ALG_ID) 0x000B)
#define TPM2_ALG_SHA384              ((TPM2_ALG_ID) 0x000C)
#define TPM2_ALG_SHA512              ((TPM2_ALG_ID) 0x000D)
#define TPM2_ALG_NULL                ((TPM2_ALG_ID) 0x0010)
#define TPM2_ALG_SM3_256             ((TPM2_ALG_ID) 0x0012)
#define TPM2_ALG_SM4                 ((TPM2_ALG_ID) 0x0013)
#define TPM2_ALG_RSASSA              ((TPM2_ALG_ID) 0x0014)
#define TPM2_ALG_RSAES               ((TPM2_ALG_ID) 0x0015)
#define TPM2_ALG_RSAPSS              ((TPM2_ALG_ID) 0x0016)
#define TPM2_ALG_OAEP                ((TPM2_ALG_ID) 0x0017)
#define TPM2_ALG_ECDSA               ((TPM2_ALG_ID) 0x0018)
#define TPM2_ALG_ECDH                ((TPM2_ALG_ID) 0x0019)
#define TPM2_ALG_ECDAA               ((TPM2_ALG_ID) 0x001A)
#define TPM2_ALG_SM2                 ((TPM2_ALG_ID) 0x001B)
#define TPM2_ALG_ECSCHNORR           ((TPM2_ALG_ID) 0x001C)
#define TPM2_ALG_ECMQV               ((TPM2_ALG_ID) 0x001D)
#define TPM2_ALG_KDF1_SP800_56A      ((TPM2_ALG_ID) 0x0020)
#define TPM2_ALG_KDF2                ((TPM2_ALG_ID) 0x0021)
#define TPM2_ALG_KDF1_SP800_108      ((TPM2_ALG_ID) 0x0022)
#define TPM2_ALG_ECC                 ((TPM2_ALG_ID) 0x0023)
#define TPM2_ALG_SYMCIPHER           ((TPM2_ALG_ID) 0x0025)
#define TPM2_ALG_CAMELLIA            ((TPM2_ALG_ID) 0x0026)
#define TPM2_ALG_CMAC                ((TPM2_ALG_ID) 0x003F)
#define TPM2_ALG_CTR                 ((TPM2_ALG_ID) 0x0040)
#define TPM2_ALG_SHA3_256            ((TPM2_ALG_ID) 0x0027)
#define TPM2_ALG_SHA3_384            ((TPM2_ALG_ID) 0x0028)
#define TPM2_ALG_SHA3_512            ((TPM2_ALG_ID) 0x0029)
#define TPM2_ALG_OFB                 ((TPM2_ALG_ID) 0x0041)
#define TPM2_ALG_CBC                 ((TPM2_ALG_ID) 0x0042)
#define TPM2_ALG_CFB                 ((TPM2_ALG_ID) 0x0043)
#define TPM2_ALG_ECB                 ((TPM2_ALG_ID) 0x0044)

/*
 * define from TCG PC Client Platform Firmware Profile Specification
 * https://trustedcomputinggroup.org/resource/pc-client-specific-platform-firmware-profile-specification/
 */

typedef enum {
	EV_PREBOOT_CERT				= 0x00000000,
	EV_POST_CODE				= 0x00000001,
	EV_UNUSED				= 0x00000002,
	EV_NO_ACTION				= 0x00000003,
	EV_SEPARATOR				= 0x00000004,
	EV_ACTION				= 0x00000005,
	EV_EVENT_TAG				= 0x00000006,
	EV_S_CRTM_CONTENTS			= 0x00000007,
	EV_S_CRTM_VERSION			= 0x00000008,
	EV_CPU_MICROCODE			= 0x00000009,
	EV_PLATFORM_CONFIG_FLAGS		= 0x0000000a,
	EV_TABLE_OF_DEVICES			= 0x0000000b,
	EV_COMPACT_HASH				= 0x0000000c,
	EV_IPL					= 0x0000000d,
	EV_IPL_PARTITION_DATA			= 0x0000000e,
	EV_NONHOST_CODE				= 0x0000000f,
	EV_NONHOST_CONFIG			= 0x00000010,
	EV_NONHOST_INFO				= 0x00000011,
	EV_OMIT_BOOT_DEVICE_EVENTS		= 0x00000012,
	EV_EFI_EVENT_BASE			= 0x80000000,
	EV_EFI_VARIABLE_DRIVER_CONFIG		= 0x80000001,
	EV_EFI_VARIABLE_BOOT			= 0x80000002,
	EV_EFI_BOOT_SERVICES_APPLICATION	= 0x80000003,
	EV_EFI_BOOT_SERVICES_DRIVER		= 0x80000004,
	EV_EFI_RUNTIME_SERVICES_DRIVER		= 0x80000005,
	EV_EFI_GPT_EVENT			= 0x80000006,
	EV_EFI_ACTION				= 0x80000007,
	EV_EFI_PLATFORM_FIRMWARE_BLOB		= 0x80000008,
	EV_EFI_HANDOFF_TABLES			= 0x80000009,
	EV_EFI_PLATFORM_FIRMWARE_BLOB2		= 0x8000000a,
	EV_EFI_HANDOFF_TABLES2			= 0x8000000b,
	EV_EFI_VARIABLE_BOOT2			= 0x8000000c,
	EV_EFI_HCRTM_EVENT			= 0x80000010,
	EV_EFI_VARIABLE_AUTHORITY		= 0x800000e0,
	EV_EFI_SPDM_FIRMWARE_BLOB		= 0x800000e1,
	EV_EFI_SPDM_FIRMWARE_CONFIG		= 0x800000e2,
	EV_EFI_SPDM_DEVICE_POLICY		= 0x800000e3,
	EV_EFI_SPDM_DEVICE_AUTHORITY		= 0x800000e4,
} fwts_tpmlog_event_type;

/*
 * H-CRTM resets PCR[0] to the locality-4 startup value (0...04, locality in
 * the last octet) before extending the H-CRTM data; see TPM 2.0 Library
 * Part 1, "H-CRTM before TPM2_Startup()".
 */
#define TPM2_HCRTM_PCR0_LOCALITY	0x04


typedef struct {
	uint32_t pcr_index;
	uint32_t event_type;
	uint8_t digest[20];
	uint32_t event_data_size;
	uint8_t event[0];
} __attribute__ ((packed)) fwts_pc_client_pcr_event;

typedef struct {
uint16_t algorithm_id;
uint16_t digest_size;
} __attribute__ ((packed)) fwts_spec_id_event_alg_sz;

typedef struct {
	uint8_t				signature[16];
	uint32_t			platform_class;
	uint8_t				spec_version_minor;
	uint8_t				spec_version_major;
	uint8_t				spec_errata;
	uint8_t				uintn_size;
	uint32_t			number_of_alg;
/*
 * Below items are not fix sizes, skip to define them
 * but it should be calculated when evaluate the
 * struct size.
 */

/*
	fwts_spec_id_event_alg_sz	digest_sizes[0];
	uint8_t				vendor_info_size;
	uint8_t				vendor_info[0];
*/
} __attribute__ ((packed)) fwts_efi_spec_id_event;


typedef struct {
	uint32_t	pcr_index;
	uint32_t	event_type;
	uint32_t	digests_count;
/*
 * Below items are not fix size, skip to define them
 * but it should be calculated when evaluate the
 * struct size.
 */

/*
	uint8_t		digests[0];
	uint32_t	event_size;
	uint8_t		event[eventSize];
*/
} __attribute__ ((packed)) fwts_tcg_pcr_event2;

typedef uint64_t uefi_physical_address;
typedef struct {
    uefi_physical_address	image_location_in_memory;
    uint64_t			image_len_in_memory;
    uint64_t			image_link_time_address;
    uint64_t			length_of_device_path;
    uint8_t			device_path[0];
} __attribute__ ((packed)) uefi_image_load_event;

void fwts_tpm_data_hexdump(fwts_framework *fw, const uint8_t *data,
	const size_t size, const char *str);
bool fwts_tpm_extend_pcr(uint8_t *pcr, const size_t pcr_len,
	TPM2_ALG_ID alg, const uint8_t *data);
uint8_t fwts_tpm_get_hash_size(const TPM2_ALG_ID hash);

PRAGMA_POP

#endif
