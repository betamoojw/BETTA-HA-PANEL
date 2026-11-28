/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Credential derivation, comparison and temporary buffer cleanup.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define USER_AUTH_PASSWORD_MIN 8
#define USER_AUTH_PASSWORD_MAX 64
#define USER_PASSWORD_PBKDF2   2
#define USER_PASSWORD_ROUNDS   10000
/** @brief Derive the stored credential representation selected by the record version.
 * @param password Borrowed NUL-terminated password.
 * @param salt Borrowed 16-byte salt.
 * @param[out] hash Destination for the 32-byte result; ignore it on failure.
 * @param algorithm Persisted derivation algorithm ID.
 * @return True only when a complete digest was produced.
 */
bool user_password_derive(const char *password, const uint8_t salt[16], uint8_t hash[32], uint8_t algorithm);
/** @brief Clear the specified writable bytes through volatile stores. */
void user_password_wipe(void *data, unsigned size);
/** @return True when the buffers match; compares all size bytes without an early mismatch exit. */
bool user_password_equal(const void *a, const void *b, unsigned size);
