/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/* Link with the real PSA crypto implementation, not the auth test doubles.
 * Vectors independently generated with Python hashlib (OpenSSL). */
#include "settings/user_password.h"
#include "psa/crypto.h"
#include <stdio.h>
#include <string.h>

static int check(const char *password, unsigned algorithm, const char *expected)
{
    uint8_t salt[16], hash[32]; char hex[65];
    for (unsigned i = 0; i < sizeof(salt); ++i) salt[i] = i;
    if (!user_password_derive(password, salt, hash, algorithm)) return 1;
    for (unsigned i = 0; i < sizeof(hash); ++i) snprintf(hex + i * 2, 3, "%02x", hash[i]);
    if (strcmp(hex, expected)) return 1;
    uint8_t changed[32]; memcpy(changed, hash, sizeof(hash)); changed[31] ^= 1;
    if (!user_password_equal(hash, hash, sizeof(hash)) || user_password_equal(hash, changed, sizeof(hash))) return 1;
    user_password_wipe(hash, sizeof(hash));
    for (unsigned i = 0; i < sizeof(hash); ++i) if (hash[i]) return 1;
    return 0;
}
int main(void)
{
    if (psa_crypto_init() != PSA_SUCCESS) return 1;
    if (check("password1", 1, "7697636df40beea53978a71fb79d5202b224dccc840afa42b0002910ee2a9566") ||
        check("password1", USER_PASSWORD_PBKDF2, "b3dec641523319c4c18cf5bac2faded5820e3f267ed62c64defe2c13b5bdf243") ||
        check("\xe5\xaf\x86\xe7\xa0\x81\xe5\xae\x89\xe5\x85\xa8", USER_PASSWORD_PBKDF2,
              "9dfad73689bd4c3c94b1251462f07ee92d0d2245fbdd63f8d43be917b090d32e")) return 1;
    uint8_t salt[16] = {0}, hash[32];
    if (user_password_derive("password1", salt, hash, 99)) return 1;
    puts("Real PSA password derivation: 3 independent vectors, comparison, wipe and algorithm rejection passed");
    return 0;
}
