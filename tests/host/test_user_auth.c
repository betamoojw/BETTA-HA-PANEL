/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/* Real account/session/policy code; deterministic crypto and fault-injected NVS.
 * Cryptographic strength is not claimed by this storage/lifecycle test. */
#include <assert.h>
#include <stdio.h>
#include "settings/user_auth.c"
#include "settings/user_auth_policy.c"

static accounts_t disk;
static bool exists;
static esp_err_t open_error, read_error, write_error, commit_error;
static admin_auth_state_t legacy;
static int64_t clock_us = 1000000;
static unsigned checks, derives;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int64_t esp_timer_get_time(void) { return clock_us; }
void esp_fill_random(void *out, size_t n) {
    static unsigned seed = 1234;
    uint8_t *p = out;
    while (n--) { seed = seed * 1664525u + 1013904223u; *p++ = seed >> 24; }
}
void user_password_wipe(void *out, unsigned n) { volatile uint8_t *p = out; while (n--) *p++ = 0; }
bool user_password_equal(const void *a, const void *b, unsigned n) { return memcmp(a, b, n) == 0; }
bool user_password_derive(const char *password, const uint8_t salt[16], uint8_t out[32], uint8_t alg) {
    ++derives;
    if (!password || strlen(password) > 64) return false;
    memset(out, alg, 32);
    for (unsigned i = 0; i < 16; ++i) out[i] ^= salt[i];
    for (unsigned i = 0; password[i]; ++i) out[i % 32] ^= (uint8_t)password[i];
    return true;
}
admin_auth_state_t admin_auth_state(void) { return legacy; }
bool admin_auth_export(uint8_t salt[16], uint8_t hash[32]) {
    memset(salt, 17, 16); return user_password_derive("old-admin-password", salt, hash, 1);
}
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *h) {
    (void)mode; CHECK(!strcmp(name, "betta_auth")); *h = 1; return open_error;
}
void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *n) {
    (void)h; CHECK(!strcmp(key, "users"));
    if (read_error) return read_error;
    if (!exists) return ESP_ERR_NVS_NOT_FOUND;
    CHECK(*n >= sizeof(disk)); memcpy(out, &disk, sizeof(disk)); *n = sizeof(disk); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *in, size_t n) {
    (void)h; CHECK(!strcmp(key, "users")); CHECK(n == sizeof(disk));
    if (write_error) return write_error;
    memcpy(&disk, in, sizeof(disk)); exists = true; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return commit_error; }
static void boot(void) {
    s_loaded = s_migrated = false; s_state = USER_AUTH_UNAVAILABLE;
    memset(&s_accounts, 0, sizeof(s_accounts)); user_auth_revoke_all();
}
static void fresh(void) {
    exists = false; legacy = ADMIN_AUTH_UNSET;
    open_error = read_error = write_error = commit_error = ESP_OK;
    boot();
}
static user_auth_info_t verify(const char *name, const char *password) {
    user_auth_info_t info = {0}; CHECK(user_auth_verify(name, password, &info)); return info;
}
int main(void) {
    fresh(); CHECK(user_auth_state() == USER_AUTH_OPEN);
    CHECK(user_auth_update("owner", "password1", USER_ROLE_VIEWER, true, true) != ESP_OK);
    CHECK(user_auth_state() == USER_AUTH_OPEN);
    CHECK(user_auth_update("owner", "short", USER_ROLE_ADMIN, true, true) != ESP_OK);
    CHECK(user_auth_update("bad name", "password1", USER_ROLE_ADMIN, true, true) != ESP_OK);
    CHECK(user_auth_update("owner", "password1", USER_ROLE_ADMIN, true, true) == ESP_OK);
    CHECK(user_auth_migrated()); CHECK(user_auth_count() == 1);
    user_auth_info_t info = verify("owner", "password1"); CHECK(info.role == USER_ROLE_ADMIN);
    CHECK(!user_auth_verify("owner", "wrong", &info));
    unsigned before = derives; CHECK(!user_auth_verify("absent", "password1", &info)); CHECK(derives > before);
    CHECK(user_auth_delete("owner") != ESP_OK);
    CHECK(user_auth_update("owner", "", USER_ROLE_VIEWER, true, false) != ESP_OK);
    CHECK(user_auth_update("owner", "", USER_ROLE_ADMIN, false, false) != ESP_OK);
    CHECK(user_auth_update("owner", "", USER_ROLE_ADMIN, true, true) != ESP_OK);
    CHECK(user_auth_update("viewer", "password2", USER_ROLE_VIEWER, true, true) == ESP_OK);
    char token[65]; info = verify("viewer", "password2"); CHECK(user_auth_session_create(&info, token));
    CHECK(user_auth_session_check(token, &info)); CHECK(info.role == USER_ROLE_VIEWER);
    user_auth_lease_t lease; CHECK(user_auth_lease_capture(token, &lease)); CHECK(user_auth_lease_valid(&lease));
    CHECK(user_auth_update("viewer", "", USER_ROLE_OPERATOR, false, false) == ESP_OK);
    CHECK(!user_auth_session_check(token, &info)); CHECK(!user_auth_lease_valid(&lease));
    CHECK(!user_auth_verify("viewer", "password2", &info));
    CHECK(user_auth_update("viewer", "", USER_ROLE_OPERATOR, true, false) == ESP_OK);
    boot(); info = verify("viewer", "password2"); CHECK(info.role == USER_ROLE_OPERATOR);
    CHECK(user_auth_session_create(&info, token)); CHECK(user_auth_lease_capture(token, &lease));
    user_auth_logout(token); CHECK(!user_auth_session_check(token, &info)); CHECK(!user_auth_lease_valid(&lease));
    CHECK(user_auth_session_create(&info, token)); CHECK(user_auth_lease_capture(token, &lease));
    clock_us += (int64_t)USER_AUTH_SESSION_SECONDS * 1000000;
    CHECK(!user_auth_session_check(token, &info)); CHECK(!user_auth_lease_valid(&lease));
    for (int i = 0; i < 16; ++i) CHECK(user_auth_session_create(&info, token));
    CHECK(!user_auth_session_create(&info, token));
    user_auth_revoke_all(); CHECK(user_auth_session_create(&info, token));
    CHECK(user_auth_update("viewer", "newpass22", USER_ROLE_OPERATOR, true, false) == ESP_OK);
    CHECK(!user_auth_verify("viewer", "password2", &info)); verify("viewer", "newpass22");
    CHECK(!user_auth_session_check(token, &info));
    for (int i = 2; i < 16; ++i) { char name[33]; snprintf(name, sizeof(name), "user%d", i); CHECK(user_auth_update(name, "password1", USER_ROLE_VIEWER, true, true) == ESP_OK); }
    CHECK(user_auth_update("overflow", "password1", USER_ROLE_VIEWER, true, true) != ESP_OK);
    CHECK(user_auth_delete("user5") == ESP_OK); CHECK(user_auth_count() == 15);
    boot(); CHECK(user_auth_count() == 15);
    CHECK(!user_auth_verify("user5", "password1", &info));
    CHECK(user_auth_basic("Basic b3duZXI6cGFzc3dvcmQx", &info)); CHECK(info.role == USER_ROLE_ADMIN);
    CHECK(!user_auth_basic("Basic b3duZXI6cGFzc3dvcmQxAA==", &info));
    CHECK(!user_auth_basic("Basic b3duZXI6cGFzc3dvcmQx====", &info));
    CHECK(!user_auth_basic("Basic ========", &info)); CHECK(!user_auth_basic("Bearer abc", &info));
    write_error = ESP_FAIL; CHECK(user_auth_update("owner", "newpass11", USER_ROLE_ADMIN, true, false) == ESP_FAIL);
    write_error = ESP_OK; verify("owner", "password1");
    commit_error = ESP_FAIL; CHECK(user_auth_update("owner", "newpass11", USER_ROLE_ADMIN, true, false) == ESP_FAIL);
    commit_error = ESP_OK; verify("owner", "newpass11"); /* Read whichever atomic record reached NVS. */
    boot(); read_error = ESP_FAIL; CHECK(user_auth_state() == USER_AUTH_UNAVAILABLE);
    CHECK(!user_auth_verify("owner", "newpass11", &info)); read_error = ESP_OK; verify("owner", "newpass11");
    disk.accounts[0].enabled = 0; boot(); CHECK(user_auth_state() == USER_AUTH_UNAVAILABLE);
    fresh(); legacy = ADMIN_AUTH_SET; CHECK(user_auth_state() == USER_AUTH_READY);
    verify("admin", "old-admin-password"); CHECK(!user_auth_migrated());
    CHECK(user_auth_update("reader", "password1", USER_ROLE_VIEWER, true, true) == ESP_OK);
    CHECK(user_auth_migrated()); boot(); verify("admin", "old-admin-password");
    CHECK(user_auth_update("admin", "replacement", USER_ROLE_ADMIN, true, false) == ESP_OK);
    CHECK(disk.accounts[0].algorithm == USER_PASSWORD_PBKDF2); boot(); verify("admin", "replacement");
    fresh(); legacy = ADMIN_AUTH_UNAVAILABLE; CHECK(user_auth_state() == USER_AUTH_UNAVAILABLE);
    legacy = ADMIN_AUTH_UNSET; open_error = ESP_FAIL; CHECK(user_auth_state() == USER_AUTH_UNAVAILABLE);
    open_error = ESP_OK; CHECK(user_auth_state() == USER_AUTH_OPEN);
    struct { const char *method, *uri; user_role_t role; } cases[] = {
        {"GET", "/", USER_ROLE_NONE}, {"GET", "/api/version", USER_ROLE_NONE},
        {"GET", "/api/version?tasks=1", USER_ROLE_ADMIN}, {"GET", "/api/auth/session", USER_ROLE_NONE},
        {"GET", "/api/license/status", USER_ROLE_NONE}, {"GET", "/api/license/status?refresh=1", USER_ROLE_NONE},
        {"GET", "/api/license", USER_ROLE_VIEWER}, {"GET", "/api/license/status/extra", USER_ROLE_ADMIN},
        {"POST", "/api/license/status", USER_ROLE_ADMIN}, {"DELETE", "/api/license/status", USER_ROLE_ADMIN},
        {"GET", "/api/layout?limits=1", USER_ROLE_VIEWER}, {"PUT", "/api/layout", USER_ROLE_INSTALLER},
        {"POST", "/api/navigate", USER_ROLE_OPERATOR}, {"POST", "/api/users", USER_ROLE_ADMIN},
        {"GET", "/api/users", USER_ROLE_ADMIN}, {"POST", "/api/factory-reset", USER_ROLE_ADMIN},
        {"GET", "/api/coredump?id=123", USER_ROLE_ADMIN}, {"POST", "/api/ota/upload", USER_ROLE_ADMIN},
        {"PUT", "/api/settings", USER_ROLE_ADMIN}, {"POST", "/api/knx/programming", USER_ROLE_ADMIN},
        {"GET", "/api/layout/extra", USER_ROLE_ADMIN}, {"DELETE", "/api/themes/custom", USER_ROLE_INSTALLER},
        {"POST", "/api/display/remote/session", USER_ROLE_VIEWER}, {"GET", "/api/new-extension", USER_ROLE_ADMIN},
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(*cases); ++i)
        CHECK(user_auth_required_role(cases[i].uri, cases[i].method) == cases[i].role);
    printf("User authentication: %u checks passed\n", checks);
    return 0;
}
