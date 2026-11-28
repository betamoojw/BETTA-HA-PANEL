/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief HTTP-task-owned account and session state with read-only async leases.
 *
 * Background tasks validate copied leases, not the live account/session table.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "settings/user_password.h"

#define USER_AUTH_MAX_USERS       16
#define USER_AUTH_NAME_MAX        32
#define USER_AUTH_TOKEN_LEN       64
#define USER_AUTH_SESSION_SECONDS (8 * 60 * 60)

/** @brief Ordered permission levels used by the HTTP route policy. */
typedef enum
{
    USER_ROLE_NONE,
    USER_ROLE_VIEWER,
    USER_ROLE_OPERATOR,
    USER_ROLE_INSTALLER,
    USER_ROLE_ADMIN
} user_role_t;

/** @brief Distinguish unconfigured accounts from storage failures. */
typedef enum
{
    USER_AUTH_OPEN,
    USER_AUTH_READY,
    USER_AUTH_UNAVAILABLE
} user_auth_state_t;

/** @brief Account metadata safe to copy without password material. */
typedef struct
{
    char username[USER_AUTH_NAME_MAX + 1];
    user_role_t role;
    bool enabled;
} user_auth_info_t;

/** @return Account storage state after loading or checking the legacy credential. */
user_auth_state_t user_auth_state(void);
/** @return True when a valid persistent account database replaces the legacy credential. */
bool user_auth_migrated(void);
/** @return Static role name for serialization. */
const char *user_auth_role_name(user_role_t role);
/** @return Role parsed from its stored name, or USER_ROLE_NONE if unrecognized. */
user_role_t user_auth_role_parse(const char *name);
/** @return True when the proposed account name fits the stored syntax and capacity. */
bool user_auth_name_valid(const char *name);
/** @return Number of accounts exposed by the loaded database, including disabled entries. */
int user_auth_count(void);
/** @return True when the requested index has metadata, copied into info on success. */
bool user_auth_info(int index, user_auth_info_t *info);
/** @brief Check an enabled user's credential and copy metadata only on success. */
bool user_auth_verify(const char *name, const char *password, user_auth_info_t *info);
/** @brief Verify a Basic authorization value using the account database. */
bool user_auth_basic(const char *header, user_auth_info_t *info);
/** @brief Create or update one account and preserve an enabled administrator.
 * An empty password retains the credential for an existing account only.
 */
esp_err_t user_auth_update(const char *name, const char *password, user_role_t role, bool enabled, bool create);
/** @brief Remove one account if the resulting database remains valid. */
esp_err_t user_auth_delete(const char *name);
/** @brief Create a bounded, fixed-lifetime session for a verified enabled account.
 * @return False when no slot is free; an existing live session is not evicted.
 */
bool user_auth_session_create(const user_auth_info_t *info, char token[USER_AUTH_TOKEN_LEN + 1]);
/** @brief Check an unexpired session and copy its current account metadata without extending it. */
bool user_auth_session_check(const char *token, user_auth_info_t *info);
/** @brief Revoke the session and any leases bound to its slot. */
void user_auth_logout(const char *token);
/** @brief Revoke all sessions and previously captured leases. */
void user_auth_revoke_all(void);
/** @return Current atomic revocation generation. */
uint32_t user_auth_generation(void);

/** @brief Immutable authorization lease copied to an asynchronous worker. */
typedef struct
{
    uint32_t generation, session_epoch;
    int session_slot; /**< Cookie session slot, or -1 for an authorized non-cookie request. */
    int64_t expires;  /**< Monotonic expiry in microseconds. */
} user_auth_lease_t;

/** @brief Capture the identity lifetime of an already authorized HTTP request.
 * @pre Apply route authorization first; this function is not a route guard.
 */
bool user_auth_lease_capture(const char *token, user_auth_lease_t *lease);
/** @return True for an unexpired lease with matching revocation counters, without I/O. */
bool user_auth_lease_valid(const user_auth_lease_t *lease);

/** @return Minimum role for the path and method string. Unmapped routes require administrator. */
user_role_t user_auth_required_role(const char *uri, const char *method);
