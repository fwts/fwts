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
#include "fwts_tpm.h"

#include <glib.h>

/*
 *  fwts_tpm_data_hexdump
 *	hex dump of a tpm event log data
 */
void fwts_tpm_data_hexdump(
	fwts_framework *fw,
	const uint8_t *data,
	const size_t size,
	const char *str)
{
	size_t i;

	fwts_log_info_verbatim(fw, "%s: ", str);
	for (i = 0; i < size; i += 16) {
		char buffer[128];
		const size_t left = size - i;

		fwts_dump_raw_data(buffer, sizeof(buffer), data + i, i, left > 16 ? 16 : left);
		fwts_log_info_verbatim(fw, "%s", buffer + 2);
	}
}

/*
 *  fwts_tpm_extend_pcr
 *	extend a PCR hash given the supplied event data
 */
bool fwts_tpm_extend_pcr(uint8_t *pcr, const size_t pcr_len,
	TPM2_ALG_ID alg, const uint8_t *data)
{
	GChecksum *hasher;
	gsize hash_len;

	hash_len = fwts_tpm_get_hash_size(alg);
	if (hash_len > pcr_len)
		return false;

	switch (alg) {
	case TPM2_ALG_SHA256:
		hasher = g_checksum_new(G_CHECKSUM_SHA256);
		break;
	case TPM2_ALG_SHA384:
		hasher = g_checksum_new(G_CHECKSUM_SHA384);
		break;
	case TPM2_ALG_SHA512:
		hasher = g_checksum_new(G_CHECKSUM_SHA512);
		break;
	default:
		return false;
	}

	g_checksum_update(hasher, pcr, hash_len);
	g_checksum_update(hasher, data, hash_len);
	g_checksum_get_digest(hasher, pcr, &hash_len);
	g_checksum_free(hasher);

	return true;
}

/*
 *  fwts_tpm_evlog_type_to_string
 *	get hash size
 */
uint8_t fwts_tpm_get_hash_size(const TPM2_ALG_ID hash)
{
	uint8_t sz;

	switch (hash) {
	case TPM2_ALG_SHA1:
		sz = TPM2_SHA1_DIGEST_SIZE;
		break;
	case TPM2_ALG_SHA256:
		sz = TPM2_SHA256_DIGEST_SIZE;
		break;
	case TPM2_ALG_SHA384:
		sz = TPM2_SHA384_DIGEST_SIZE;
		break;
	case TPM2_ALG_SHA512:
		sz = TPM2_SHA512_DIGEST_SIZE;
		break;
	default:
		sz = 0;
		break;
	}

	return sz;
}
