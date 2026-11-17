/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "settings/user_auth.h"
#include "settings/admin_auth.h"
#include "settings/user_password.h"
#include "settings/user_auth_memory.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <strings.h>

/* Byte-only versioned record: independent of ABI padding, one atomic NVS blob. */
typedef struct {
    char username[USER_AUTH_NAME_MAX + 1];
    uint8_t role, enabled, algorithm, salt[16], hash[32];
} account_t;
typedef struct {
    uint8_t version, count;
    account_t accounts[USER_AUTH_MAX_USERS];
} accounts_t;
_Static_assert(sizeof(account_t) == 84, "credential record layout changed");
static bool s_loaded, s_migrated;
static user_auth_state_t s_state = USER_AUTH_UNAVAILABLE;
static atomic_uint s_generation = 1;

#define SESSION_COUNT 16
typedef struct {
    char token[USER_AUTH_TOKEN_LEN + 1];
    char username[USER_AUTH_NAME_MAX + 1];
    int64_t expires;
} session_t;
typedef struct {
    accounts_t db;
    session_t sessions[SESSION_COUNT];
} user_auth_storage_t;
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
static user_auth_storage_t *s_storage;
#else
static user_auth_storage_t s_internal_storage;
static user_auth_storage_t *const s_storage = &s_internal_storage;
#endif
/* Async request leases read only these atomics, never the PSRAM records. */
static atomic_uint s_session_epochs[SESSION_COUNT];

static bool storage_ready(void)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    if (!s_storage) {
        s_storage = user_auth_memory_alloc(sizeof(*s_storage));
        if (!s_storage) return false;
        memset(s_storage, 0, sizeof(*s_storage));
    }
#endif
    return true;
}

const char *user_auth_role_name(user_role_t role)
{
    static const char *const names[] = {"none", "viewer", "operator", "installer", "administrator"};
    return role >= USER_ROLE_NONE && role <= USER_ROLE_ADMIN ? names[role] : "none";
}

user_role_t user_auth_role_parse(const char *name)
{
    for (int i = USER_ROLE_VIEWER; name && i <= USER_ROLE_ADMIN; ++i)
        if (strcmp(name, user_auth_role_name(i)) == 0) return i;
    return USER_ROLE_NONE;
}

bool user_auth_name_valid(const char *name)
{
    if (!name) return false;
    const size_t n = strlen(name);
    if (n < 3 || n > USER_AUTH_NAME_MAX) return false;
    for (size_t i = 0; i < n; ++i) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static int lookup(const accounts_t *db, const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < db->count; ++i)
        if (strcmp(db->accounts[i].username, name) == 0) return i;
    return -1;
}

static bool valid_database(const accounts_t *db)
{
    if (db->version != 1 || db->count == 0 || db->count > USER_AUTH_MAX_USERS) return false;
    unsigned admins = 0;
    for (unsigned i = 0; i < db->count; ++i) {
        const account_t *a = &db->accounts[i];
        if (!memchr(a->username, 0, sizeof(a->username)) || !user_auth_name_valid(a->username) ||
            a->role < USER_ROLE_VIEWER || a->role > USER_ROLE_ADMIN || a->enabled > 1 ||
            (a->algorithm != 1 && a->algorithm != USER_PASSWORD_PBKDF2)) return false;
        if (lookup(db, a->username) != (int)i) return false;
        admins += a->enabled && a->role == USER_ROLE_ADMIN;
    }
    return admins > 0;
}

user_auth_state_t user_auth_state(void)
{
    if (!storage_ready()) return USER_AUTH_UNAVAILABLE;
    if (s_loaded) return s_state;
    nvs_handle_t h;
    esp_err_t err = nvs_open("betta_auth", NVS_READONLY, &h);
    if (err == ESP_OK) {
        size_t length = sizeof(s_storage->db);
        err = nvs_get_blob(h, "users", &s_storage->db, &length);
        nvs_close(h);
        if (err == ESP_OK) {
            s_state = length == sizeof(s_storage->db) && valid_database(&s_storage->db) ? USER_AUTH_READY : USER_AUTH_UNAVAILABLE;
            s_migrated = s_loaded = s_state == USER_AUTH_READY;
            return s_state;
        }
    }
    s_migrated = false;
    s_state = USER_AUTH_UNAVAILABLE;
    memset(&s_storage->db, 0, sizeof(s_storage->db));
    if (err != ESP_ERR_NVS_NOT_FOUND) return s_state;
    s_storage->db.version = 1;
    const admin_auth_state_t legacy = admin_auth_state();
    if (legacy == ADMIN_AUTH_UNSET) s_state = USER_AUTH_OPEN;
    else if (legacy == ADMIN_AUTH_SET) {
        account_t *a = &s_storage->db.accounts[0];
        strcpy(a->username, "admin");
        a->role = USER_ROLE_ADMIN;
        a->enabled = a->algorithm = 1;
        if (admin_auth_export(a->salt, a->hash)) {
            s_storage->db.count = 1;
            s_state = USER_AUTH_READY;
        }
    }
    /* Until migration, re-read legacy state so existing password changes take effect. */
    return s_state;
}

bool user_auth_migrated(void) { (void)user_auth_state(); return s_migrated; }
int user_auth_count(void) { return user_auth_state() == USER_AUTH_READY ? s_storage->db.count : 0; }
uint32_t user_auth_generation(void) { return atomic_load(&s_generation); }

static void copy_info(const account_t *a, user_auth_info_t *info)
{
    memset(info, 0, sizeof(*info));
    memcpy(info->username, a->username, sizeof(info->username));
    info->role = a->role;
    info->enabled = a->enabled != 0;
}

bool user_auth_info(int index, user_auth_info_t *info)
{
    if (!info || index < 0 || index >= user_auth_count()) return false;
    copy_info(&s_storage->db.accounts[index], info);
    return true;
}

bool user_auth_verify(const char *name, const char *password, user_auth_info_t *info)
{
    if (user_auth_state() != USER_AUTH_READY || !password || !info || strlen(password) > USER_AUTH_PASSWORD_MAX) return false;
    int index = lookup(&s_storage->db, name);
    /* Missing/disabled users still pay the password derivation cost. */
    const account_t *a = &s_storage->db.accounts[index >= 0 ? index : 0];
    uint8_t actual[32];
    bool ok = user_password_derive(password, a->salt, actual, a->algorithm);
    ok = ok && user_password_equal(actual, a->hash, sizeof(actual)) && index >= 0 && a->enabled;
    user_password_wipe(actual, sizeof(actual));
    if (ok) copy_info(a, info);
    return ok;
}

bool user_auth_basic(const char *header, user_auth_info_t *info)
{
    if (!header || strncasecmp(header, "Basic ", 6)) return false;
    const char *text = header + 6;
    size_t n = strlen(text), used = 0;
    if (!n || n % 4 || n > 132) return false;
    char decoded[USER_AUTH_NAME_MAX + USER_AUTH_PASSWORD_MAX + 2];
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned acc = 0, bits = 0, pads = 0;
    bool ok = true;
    for (size_t i = 0; i < n && ok; ++i) {
        char c = text[i];
        if (c == '=') { ++pads; if (pads > 2 || i < n - 2) ok = false; continue; }
        const char *found = strchr(alphabet, c);
        if (pads || !found) { ok = false; break; }
        acc = (acc << 6) | (unsigned)(found - alphabet);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (used >= sizeof(decoded) - 1 || ((acc >> bits) & 255) == 0) { ok = false; break; }
            decoded[used++] = (char)((acc >> bits) & 255);
        }
    }
    decoded[used] = 0;
    ok = ok && pads == (bits == 4 ? 2u : bits == 2 ? 1u : 0u) && (acc & ((1u << bits) - 1u)) == 0;
    char *colon = strchr(decoded, ':');
    if (ok && colon) { *colon = 0; ok = user_auth_verify(decoded, colon + 1, info); }
    else ok = false;
    user_password_wipe(decoded, sizeof(decoded));
    return ok;
}

void user_auth_revoke_all(void)
{
    atomic_fetch_add(&s_generation, 1);
    if (s_storage) user_password_wipe(s_storage->sessions, sizeof(s_storage->sessions));
}

static esp_err_t persist(accounts_t *candidate)
{
    if (!valid_database(candidate)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open("betta_auth", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, "users", candidate, sizeof(*candidate));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    user_auth_revoke_all();
    if (err == ESP_OK) {
        s_storage->db = *candidate;
        s_state = USER_AUTH_READY;
        s_loaded = s_migrated = true;
    } else {
        /* A failed commit can have reached storage; never retain stale permissions. */
        s_loaded = false;
        s_state = USER_AUTH_UNAVAILABLE;
    }
    return err;
}

esp_err_t user_auth_update(const char *name, const char *password, user_role_t role, bool enabled, bool create)
{
    user_auth_state_t state = user_auth_state();
    if (state == USER_AUTH_UNAVAILABLE) return ESP_ERR_INVALID_STATE;
    if (!user_auth_name_valid(name) || !password || role < USER_ROLE_VIEWER || role > USER_ROLE_ADMIN) return ESP_ERR_INVALID_ARG;
    size_t length = strlen(password);
    if ((length && length < USER_AUTH_PASSWORD_MIN) || length > USER_AUTH_PASSWORD_MAX || (create && !length)) return ESP_ERR_INVALID_SIZE;
    int index = lookup(&s_storage->db, name);
    if (create ? index >= 0 || s_storage->db.count == USER_AUTH_MAX_USERS : index < 0) return ESP_ERR_INVALID_ARG;
    accounts_t *candidate = user_auth_memory_alloc(sizeof(*candidate));
    if (!candidate) return ESP_ERR_NO_MEM;
    *candidate = s_storage->db;
    if (create) index = candidate->count++;
    account_t *a = &candidate->accounts[index];
    strcpy(a->username, name);
    a->role = role;
    a->enabled = enabled;
    esp_err_t err = ESP_OK;
    if (length) {
        a->algorithm = USER_PASSWORD_PBKDF2;
        esp_fill_random(a->salt, sizeof(a->salt));
        if (!user_password_derive(password, a->salt, a->hash, a->algorithm)) err = ESP_FAIL;
    }
    if (err == ESP_OK) err = persist(candidate);
    user_password_wipe(candidate, sizeof(*candidate));
    user_auth_memory_free(candidate);
    return err;
}

esp_err_t user_auth_delete(const char *name)
{
    if (user_auth_state() != USER_AUTH_READY) return ESP_ERR_INVALID_STATE;
    int index = lookup(&s_storage->db, name);
    if (index < 0) return ESP_ERR_NOT_FOUND;
    accounts_t *candidate = user_auth_memory_alloc(sizeof(*candidate));
    if (!candidate) return ESP_ERR_NO_MEM;
    *candidate = s_storage->db;
    memmove(&candidate->accounts[index], &candidate->accounts[index + 1],
        (candidate->count - index - 1) * sizeof(account_t));
    memset(&candidate->accounts[--candidate->count], 0, sizeof(account_t));
    esp_err_t err = persist(candidate);
    user_password_wipe(candidate, sizeof(*candidate));
    user_auth_memory_free(candidate);
    return err;
}

bool user_auth_session_create(const user_auth_info_t *info, char token[USER_AUTH_TOKEN_LEN + 1])
{
    if (user_auth_state() != USER_AUTH_READY || !info || !token) return false;
    int index = lookup(&s_storage->db, info->username);
    if (index < 0 || !s_storage->db.accounts[index].enabled) return false;
    const int64_t now = esp_timer_get_time();
    session_t *slot = NULL;
    for (int i = 0; i < SESSION_COUNT; ++i)
        if (s_storage->sessions[i].expires <= now) { slot = &s_storage->sessions[i]; break; }
    if (!slot) return false; /* Never silently evict another active session. */
    atomic_fetch_add(&s_session_epochs[slot - s_storage->sessions], 1);
    uint8_t bytes[32];
    esp_fill_random(bytes, sizeof(bytes));
    static const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < sizeof(bytes); ++i) {
        slot->token[i * 2] = hex[bytes[i] >> 4];
        slot->token[i * 2 + 1] = hex[bytes[i] & 15];
    }
    slot->token[USER_AUTH_TOKEN_LEN] = 0;
    strcpy(slot->username, info->username);
    slot->expires = now + (int64_t)USER_AUTH_SESSION_SECONDS * 1000000;
    strcpy(token, slot->token);
    user_password_wipe(bytes, sizeof(bytes));
    return true;
}

bool user_auth_session_check(const char *token, user_auth_info_t *info)
{
    if (!token || !info || strlen(token) != USER_AUTH_TOKEN_LEN || user_auth_state() != USER_AUTH_READY) return false;
    const int64_t now = esp_timer_get_time();
    for (int i = 0; i < SESSION_COUNT; ++i) {
        session_t *s = &s_storage->sessions[i];
        if (s->expires > now && user_password_equal(token, s->token, USER_AUTH_TOKEN_LEN)) {
            int index = lookup(&s_storage->db, s->username);
            if (index < 0 || !s_storage->db.accounts[index].enabled) return false;
            copy_info(&s_storage->db.accounts[index], info);
            return true;
        }
    }
    return false;
}

void user_auth_logout(const char *token)
{
    if (!s_storage || !token || strlen(token) != USER_AUTH_TOKEN_LEN) return;
    for (int i = 0; i < SESSION_COUNT; ++i)
        if (user_password_equal(token, s_storage->sessions[i].token, USER_AUTH_TOKEN_LEN)) {
            atomic_fetch_add(&s_session_epochs[i], 1);
            user_password_wipe(&s_storage->sessions[i], sizeof(s_storage->sessions[i]));
        }
}

bool user_auth_lease_capture(const char *token, user_auth_lease_t *lease)
{
    if (!lease || user_auth_state() == USER_AUTH_UNAVAILABLE) return false;
    *lease = (user_auth_lease_t){.generation = user_auth_generation(), .session_slot = -1,
        .expires = esp_timer_get_time() + (int64_t)USER_AUTH_SESSION_SECONDS * 1000000};
    if (!token || !*token) return true;
    user_auth_info_t info;
    if (!user_auth_session_check(token, &info)) return false;
    for (int i = 0; i < SESSION_COUNT; ++i) {
        if (user_password_equal(token, s_storage->sessions[i].token, USER_AUTH_TOKEN_LEN)) {
            lease->session_slot = i;
            lease->session_epoch = atomic_load(&s_session_epochs[i]);
            lease->expires = s_storage->sessions[i].expires;
            return true;
        }
    }
    return false;
}

bool user_auth_lease_valid(const user_auth_lease_t *lease)
{
    if (!lease || lease->generation != user_auth_generation() || esp_timer_get_time() >= lease->expires) return false;
    return lease->session_slot == -1 || (lease->session_slot >= 0 && lease->session_slot < SESSION_COUNT &&
        lease->session_epoch == atomic_load(&s_session_epochs[lease->session_slot]));
}
