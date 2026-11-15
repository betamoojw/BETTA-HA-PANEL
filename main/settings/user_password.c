/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "settings/user_password.h"
#include "psa/crypto.h"
#include <string.h>

void user_password_wipe(void *data, unsigned size)
{
    volatile uint8_t *p = data;
    while (size--) *p++ = 0;
}

bool user_password_equal(const void *a, const void *b, unsigned size)
{
    const uint8_t *x = a, *y = b;
    uint8_t diff = 0;
    for (unsigned i = 0; i < size; ++i) diff |= x[i] ^ y[i];
    return diff == 0;
}

bool user_password_derive(const char *password, const uint8_t salt[16], uint8_t hash[32], uint8_t algorithm)
{
    if (!password || strlen(password) > USER_AUTH_PASSWORD_MAX) return false;
    if (algorithm == 1) {
        /* Read-only compatibility with the original admin credential format. */
        uint8_t input[16 + USER_AUTH_PASSWORD_MAX];
        size_t got = 0, length = strlen(password);
        memcpy(input, salt, 16);
        memcpy(input + 16, password, length);
        bool ok = psa_hash_compute(PSA_ALG_SHA_256, input, length + 16, hash, 32, &got) == PSA_SUCCESS;
        for (unsigned i = 0; ok && i < 5000; ++i) {
            memcpy(input, hash, 32);
            memcpy(input + 32, salt, 16);
            ok = psa_hash_compute(PSA_ALG_SHA_256, input, 48, hash, 32, &got) == PSA_SUCCESS;
        }
        user_password_wipe(input, sizeof(input));
        return ok && got == 32;
    }
    if (algorithm != USER_PASSWORD_PBKDF2) return false;
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status = psa_key_derivation_setup(&op, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    if (status == PSA_SUCCESS) status = psa_key_derivation_input_integer(&op, PSA_KEY_DERIVATION_INPUT_COST, USER_PASSWORD_ROUNDS);
    if (status == PSA_SUCCESS) status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, 16);
    if (status == PSA_SUCCESS) status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_PASSWORD,
        (const uint8_t *)password, strlen(password));
    if (status == PSA_SUCCESS) status = psa_key_derivation_output_bytes(&op, hash, 32);
    psa_key_derivation_abort(&op);
    return status == PSA_SUCCESS;
}
