/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define USER_AUTH_PASSWORD_MIN 8
#define USER_AUTH_PASSWORD_MAX 64
#define USER_PASSWORD_PBKDF2 2
#define USER_PASSWORD_ROUNDS 10000
bool user_password_derive(const char *password, const uint8_t salt[16], uint8_t hash[32], uint8_t algorithm);
void user_password_wipe(void *data, unsigned size);
bool user_password_equal(const void *a, const void *b, unsigned size);
