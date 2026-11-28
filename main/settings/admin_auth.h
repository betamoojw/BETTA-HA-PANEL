/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Optional admin password for the web editor and the HTTP API (HTTP Basic, user "admin").
 * Off by default. The password is stored as a salted, iterated SHA-256 hash in NVS, as one record ("cred": version, salt, hash).
 */
/** @file
 * @brief Legacy administrator authentication lifecycle and migration contract.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define ADMIN_AUTH_USER    "admin"
#define ADMIN_AUTH_MIN_LEN 6
#define ADMIN_AUTH_MAX_LEN 64

/** @brief Distinguish an open setup from an unreadable credential record. */
typedef enum
{
    ADMIN_AUTH_UNSET = 0,   /**< No legacy password is configured. */
    ADMIN_AUTH_SET,         /**< The stored credential is usable. */
    ADMIN_AUTH_UNAVAILABLE, /**< Storage or record error; do not open protected routes. */
} admin_auth_state_t;

/** @brief Legacy guard decision after considering the credential state and route policy. */
typedef enum
{
    ADMIN_AUTH_ALLOW = 0,        /* serve the request */
    ADMIN_AUTH_CHECK,            /* serve it only with correct credentials */
    ADMIN_AUTH_DENY_UNAVAILABLE, /* refuse: the credentials cannot be read (recovery: erase the settings over USB) */
} admin_auth_decision_t;

/** @brief Initialize at boot; repeated calls can retry an unreadable record.
 * @return ESP_OK. Report an unreadable record through admin_auth_state(), not boot failure.
 */
esp_err_t admin_auth_init(void);

/** @return Current legacy credential state, retrying storage while unavailable. */
admin_auth_state_t admin_auth_state(void);

/** @return True for a set or unreadable credential; unreadable does not disable protection. */
bool admin_auth_enabled(void);

/** @return Guard decision for this route; an explicitly public route remains reachable. */
admin_auth_decision_t admin_auth_decide(admin_auth_state_t state, bool public_route);

/** @brief Store a new legacy password, or clear legacy protection with NULL or an empty string. */
esp_err_t admin_auth_set_password(const char *password);

/** @brief Check the borrowed Basic authorization value against the legacy state.
 * @return True for a matching protected request or an unset password; false when unavailable.
 */
bool admin_auth_check_basic(const char *header_value);

/** @brief Copy legacy credential material only for internal account migration.
 * @return True when the material was copied; never expose it in an API response.
 */
bool admin_auth_export(uint8_t salt[16], uint8_t hash[32]);
