/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/* Reuse real account auth/CSRF/rate guard plus its HTTP transport stubs. */
#define LICENSE_HTTP_REUSE
#define httpd_req_recv account_http_recv
#define httpd_resp_sendstr account_http_send
#include "test_user_http.c"
#undef httpd_req_recv
#undef httpd_resp_sendstr
static bool receive_failure, send_failure;
int httpd_req_recv(httpd_req_t *r, char *out, size_t n) {
    return receive_failure ? -1 : account_http_recv(r, out, n);
}
esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *text) {
    return send_failure ? ESP_FAIL : account_http_send(r, text);
}
#include "api/api_license.c"
static license_status_t license_snapshot = {.device_id = "test-device", .key_state = LICENSE_MISSING, .trial_seconds = 3600};
static esp_err_t license_error;
static unsigned imports, removals;
static esp_err_t license_result;
void license_manager_status(license_status_t *out) { *out = license_snapshot; }
esp_err_t license_manager_import(const void *data, size_t n) {
    CHECK(data && n >= 200 && n <= 202); ++imports;
    if (!license_error) license_snapshot.key_state = LICENSE_VALID;
    return license_error;
}
esp_err_t license_manager_remove(void) {
    ++removals;
    if (!license_error) license_snapshot.key_state = LICENSE_MISSING;
    return license_error;
}
static httpd_req_t license_request(int method, const char *body, const char *cookie, const char *origin) {
    clock_us += 1000000;
    httpd_req_t r = {.uri = "/api/license", .method = method, .body = body,
        .content_len = body ? (int)strlen(body) : 0, .cookie = cookie, .origin = origin, .marker = "1", .content_type = "application/octet-stream"};
    strcpy(r.status, "200 OK");
    if (method == HTTP_GET) license_result = guarded_license_get(&r);
    else if (method == HTTP_POST) license_result = guarded_license_post(&r);
    else license_result = guarded_license_delete(&r);
    return r;
}
int main(void) {
    fresh(); CHECK(api_license_register((void *)1) == ESP_OK); CHECK(route_count == 3);
    CHECK(user_auth_update("owner", "password1", USER_ROLE_ADMIN, true, true) == ESP_OK);
    CHECK(user_auth_update("reader", "password1", USER_ROLE_VIEWER, true, true) == ESP_OK);
    CHECK(user_auth_update("installer", "password1", USER_ROLE_INSTALLER, true, true) == ESP_OK);
    char owner[96], reader[96], installer[96]; login("owner", owner); login("reader", reader); login("installer", installer);
    char file[203]; memset(file, 'k', 200); file[200] = 0;
    expect(license_request(HTTP_GET, NULL, NULL, NULL), 401);
    httpd_req_t r = license_request(HTTP_GET, NULL, reader, NULL); expect(r, 200);
    CHECK(strstr(r.response, "\"status_only\":true")); CHECK(!strstr(r.response, file));
    expect(license_request(HTTP_POST, file, reader, "http://panel.local"), 403);
    expect(license_request(HTTP_POST, file, installer, "http://panel.local"), 403);
    expect(license_request(HTTP_DELETE, NULL, installer, "http://panel.local"), 403);
    expect(license_request(HTTP_POST, file, owner, "http://evil.example"), 403); CHECK(imports == 0);
    expect(license_request(HTTP_POST, "short", owner, "http://panel.local"), 400); CHECK(imports == 0);
    r = license_request(HTTP_POST, file, owner, "http://panel.local"); expect(r, 200);
    CHECK(imports == 1 && r.consumed == 200 && strstr(r.response, "valid")); CHECK(!strstr(r.response, file));
    license_error = ESP_ERR_INVALID_ARG; expect(license_request(HTTP_POST, file, owner, "http://panel.local"), 400);
    license_error = ESP_FAIL; expect(license_request(HTTP_POST, file, owner, "http://panel.local"), 503);
    expect(license_request(HTTP_DELETE, NULL, owner, "http://panel.local"), 503);
    license_error = 0; expect(license_request(HTTP_DELETE, NULL, owner, "http://panel.local"), 200); CHECK(removals == 2);
    /* Expiration is informational: ordinary control routes retain normal role permissions. */
    license_snapshot.trial_seconds = 0;
    expect(request(HTTP_POST, "/api/navigate", "{}", owner, "http://panel.local", "1"), 200);
    expect(request(HTTP_PUT, "/api/layout", "{}", installer, "http://panel.local", "1"), 200);
    unsigned before = imports;
    receive_failure = true; license_request(HTTP_POST, file, owner, "http://panel.local");
    CHECK(license_result == ESP_FAIL && imports == before); receive_failure = false;
    send_failure = true; license_request(HTTP_POST, file, owner, "http://panel.local");
    CHECK(license_result == ESP_FAIL && imports == before + 1); send_failure = false;
    /* Lost response does not undo a durable write; a fresh GET recovers its status. */
    r = license_request(HTTP_GET, NULL, owner, NULL); expect(r, 200); CHECK(strstr(r.response, "valid"));
    puts("license HTTP: PASS (real roles/CSRF, bounded upload, error mapping, status-only expiry)");
}
