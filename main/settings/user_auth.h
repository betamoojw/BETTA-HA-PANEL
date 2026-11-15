/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "settings/user_password.h"

#define USER_AUTH_MAX_USERS 16
#define USER_AUTH_NAME_MAX 32
#define USER_AUTH_TOKEN_LEN 64
#define USER_AUTH_SESSION_SECONDS (8 * 60 * 60)

typedef enum {
    USER_ROLE_NONE, USER_ROLE_VIEWER, USER_ROLE_OPERATOR, USER_ROLE_INSTALLER, USER_ROLE_ADMIN
} user_role_t;
typedef enum { USER_AUTH_OPEN, USER_AUTH_READY, USER_AUTH_UNAVAILABLE } user_auth_state_t;
typedef struct {
    char username[USER_AUTH_NAME_MAX + 1];
    user_role_t role;
    bool enabled;
} user_auth_info_t;

/* Owned by the HTTP task. No LVGL/HA/MQTT task may mutate accounts or sessions. */
user_auth_state_t user_auth_state(void);
bool user_auth_migrated(void);
const char *user_auth_role_name(user_role_t role);
user_role_t user_auth_role_parse(const char *name);
bool user_auth_name_valid(const char *name);
int user_auth_count(void);
bool user_auth_info(int index, user_auth_info_t *info);
bool user_auth_verify(const char *name, const char *password, user_auth_info_t *info);
bool user_auth_basic(const char *header, user_auth_info_t *info);
/* Single-account transactional edits; existing blank passwords retain the hash. */
esp_err_t user_auth_update(const char *name, const char *password, user_role_t role, bool enabled, bool create);
esp_err_t user_auth_delete(const char *name);
bool user_auth_session_create(const user_auth_info_t *info, char token[USER_AUTH_TOKEN_LEN + 1]);
bool user_auth_session_check(const char *token, user_auth_info_t *info);
void user_auth_logout(const char *token);
void user_auth_revoke_all(void);
uint32_t user_auth_generation(void);

/* Immutable ticket for async image workers. Validation only reads atomics. */
typedef struct {
    uint32_t generation, session_epoch;
    int session_slot;
    int64_t expires;
} user_auth_lease_t;
bool user_auth_lease_capture(const char *token, user_auth_lease_t *lease);
bool user_auth_lease_valid(const user_auth_lease_t *lease);

/* Exact path policy, independent of ESP HTTP method numbers. Unknown routes are admin-only. */
user_role_t user_auth_required_role(const char *uri, const char *method);
