/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "settings/license_crypto.h"
#include "psa/crypto.h"
#include <stdio.h>
#include <string.h>

static int unhex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool decode_hex(const char *in, uint8_t *out, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        int hi = unhex(in[2 * i]), lo = unhex(in[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool license_device_id(const uint8_t mac[6], char out[65])
{
    char seed[40]; uint8_t hash[32]; size_t length = 0;
    /* Stable chip eFuse identity, independent of C6 radio, room, flash and IDF calibration. */
    snprintf(seed, sizeof(seed), "BETTA-HA-PANEL/v1:%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (psa_hash_compute(PSA_ALG_SHA_256, (const uint8_t *)seed, strlen(seed),
            hash, sizeof(hash), &length) != PSA_SUCCESS || length != sizeof(hash)) return false;
    static const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < sizeof(hash); ++i) {
        out[2 * i] = hex[hash[i] >> 4]; out[2 * i + 1] = hex[hash[i] & 15];
    }
    out[64] = 0;
    return true;
}

static psa_status_t import_public(const char *hex, psa_key_id_t *id)
{
    uint8_t public_key[65];
    if (!hex || strlen(hex) != 130 || !decode_hex(hex, public_key, sizeof(public_key)) || public_key[0] != 4)
        return PSA_ERROR_INVALID_ARGUMENT;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_status_t status = psa_import_key(&attributes, public_key, sizeof(public_key), id);
    psa_reset_key_attributes(&attributes);
    return status;
}

bool license_public_key_valid(const char *public_hex)
{
    psa_key_id_t id = 0;
    if (import_public(public_hex, &id) != PSA_SUCCESS) return false;
    return psa_destroy_key(id) == PSA_SUCCESS;
}

int license_key_verify(const char *public_hex, const char device_id[65], const void *key, size_t length)
{
    if (!device_id || strlen(device_id) != 64 || !key) return 0;
    const char *bytes = key;
    if (length == 202 && bytes[200] == '\r' && bytes[201] == '\n') length = 200;
    else if (length == 201 && bytes[200] == '\n') length = 200;
    if (length != LICENSE_FILE_LENGTH || memcmp(bytes, "BETTA1:", 7) || bytes[71] != ':' ||
        memcmp(bytes + 7, device_id, 64)) return 0;
    uint8_t signature[64];
    if (!decode_hex(bytes + 72, signature, sizeof(signature))) return 0;
    /* Domain separation and signed entitlement; never sign an unqualified device ID. */
    char message[112];
    int n = snprintf(message, sizeof(message), "BETTA-HA-PANEL-LICENSE/v1\n%s\nperpetual\n", device_id);
    psa_key_id_t id = 0;
    if (import_public(public_hex, &id) != PSA_SUCCESS) return -1;
    psa_status_t status = psa_verify_message(id, PSA_ALG_ECDSA(PSA_ALG_SHA_256),
        (const uint8_t *)message, (size_t)n, signature, sizeof(signature));
    psa_destroy_key(id);
    if (status == PSA_SUCCESS) return 1;
    return status == PSA_ERROR_INVALID_SIGNATURE ? 0 : -1;
}
