/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "settings/license_crypto.h"
#include "psa/crypto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Public test vector signed by tests/tools via cryptography, P-256 scalar 1.
 * This known test key MUST NOT be configured as a production issuer. */
static const char public_key[] = "046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5";
static const char device[] = "ee8c86d7ed0f4b0deb8926e48d93d2ddd9e316feed18aaa0222e6a3101525880";
static const char signed_key[] = "BETTA1:ee8c86d7ed0f4b0deb8926e48d93d2ddd9e316feed18aaa0222e6a3101525880:d0dbf3fd1efcdc905a04c2261ca0156f02344eeee606a79eeffa6c04872e16b2bfc251e29d2072257e0c95d0c092b4f4c7883d74d4a89053d8815702e7244424";
int main(void) {
    assert(psa_crypto_init() == PSA_SUCCESS);
    char id[65]; const uint8_t mac[6] = {1, 1, 1, 1, 1, 1};
    assert(license_device_id(mac, id)); assert(!strcmp(id, device));
    assert(license_public_key_valid(public_key));
    assert(!license_public_key_valid("")); assert(!license_public_key_valid("04"));
    assert(strlen(signed_key) == LICENSE_FILE_LENGTH);
    assert(license_key_verify(public_key, id, signed_key, 200) == 1);
    const char *other_issuer = "047cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc4766997807775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1";
    assert(license_public_key_valid(other_issuer));
    assert(license_key_verify(other_issuer, id, signed_key, 200) == 0);
    assert(license_key_verify("", id, signed_key, 200) == -1);
    char bad_public[131]; memset(bad_public, '0', 130); bad_public[130] = 0;
    assert(!license_public_key_valid(bad_public));
    bad_public[1] = '4'; assert(!license_public_key_valid(bad_public));
    char file[204]; memcpy(file, signed_key, 200); file[200] = '\n';
    assert(license_key_verify(public_key, id, file, 201) == 1);
    file[200] = '\r'; file[201] = '\n';
    assert(license_key_verify(public_key, id, file, 202) == 1);
    file[202] = 'x'; assert(license_key_verify(public_key, id, file, 203) == 0);
    file[200] = 0; assert(license_key_verify(public_key, id, file, 201) == 0);
    file[72] = 0; assert(license_key_verify(public_key, id, file, 200) == 0); file[72] = signed_key[72];
    assert(license_key_verify(public_key, id, file, 199) == 0);
    id[0] = 'f'; assert(license_key_verify(public_key, id, file, 200) == 0); id[0] = device[0];
    for (size_t i = 0; i < 200; ++i) {
        memcpy(file, signed_key, 200); file[i] = file[i] == '0' ? '1' : '0';
        assert(license_key_verify(public_key, id, file, 200) == 0);
    }
    /* Repeated import/destroy catches exhausted PSA key slots. */
    for (unsigned i = 0; i < 100; ++i) assert(license_key_verify(public_key, id, signed_key, 200) == 1);
    puts("license PSA crypto: PASS (cross-language vector, 200 mutations, 100 verifications)");
}
