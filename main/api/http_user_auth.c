/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Local account sessions, role checks and response filtering.
 *
 * The HTTP task owns the current identity and token. An asynchronous worker
 * must capture and recheck a lease instead of borrowing request-local state.
 */
#include "api/http_user_auth.h"
#include "settings/user_password.h"
#include "settings/user_auth_memory.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Identity exposed to a synchronous handler after the route policy passes. */
static user_auth_info_t s_current;
static char s_current_token[USER_AUTH_TOKEN_LEN + 1];

const user_auth_info_t *http_user_auth_current(void)
{
    return &s_current;
}

const char *http_user_auth_token(void)
{
    return s_current_token;
}

void http_user_auth_filter_settings(cJSON *root)
{
    if (s_current.role == USER_ROLE_ADMIN)
        return;
    /* Allowlist: new connection/configuration fields stay private. */
    cJSON *item = root ? root->child : NULL;
    while (item)
    {
        cJSON *next = item->next;
        if (strcmp(item->string, "ok") && strcmp(item->string, "ui") && strcmp(item->string, "topbar") && strcmp(item->string, "hardware"))
            cJSON_DeleteItemFromObjectCaseSensitive(root, item->string);
        item = next;
    }
}

static esp_err_t reply(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t send_object(httpd_req_t *req, cJSON *root)
{
    char *json = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!json)
        return httpd_resp_send_500(req);
    esp_err_t err = reply(req, "200 OK", json);
    cJSON_free(json);
    return err;
}

static bool path_is(httpd_req_t *req, const char *path)
{
    size_t n = strlen(path);
    return !strncmp(req->uri, path, n) && (req->uri[n] == 0 || req->uri[n] == '?');
}

/* Per-address backoff plus a global derivation budget. Rotating addresses cannot
 * turn the bounded login table into an unbounded password-hashing workload. */
typedef struct
{
    uint32_t key;
    unsigned failures;
    int64_t until, seen;
} attempt_t;

#define ATTEMPT_COUNT 16
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
static attempt_t *s_attempts;
#else
static attempt_t s_attempts[ATTEMPT_COUNT];
#endif
static int64_t s_window;
static unsigned s_checks;

static uint32_t client_key(httpd_req_t *req)
{
    struct sockaddr_storage peer = {0};
    socklen_t length = sizeof(peer);
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr *) &peer, &length))
        return 0;
    const uint8_t *bytes = NULL;
    size_t n = 0;
    if (peer.ss_family == AF_INET)
    {
        bytes = (const uint8_t *) &((struct sockaddr_in *) &peer)->sin_addr;
        n = 4;
    }
#if LWIP_IPV6
    else if (peer.ss_family == AF_INET6)
    {
        bytes = (const uint8_t *) &((struct sockaddr_in6 *) &peer)->sin6_addr;
        n = 16;
    }
#endif
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < n; ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static attempt_t *attempt(httpd_req_t *req)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    if (!s_attempts)
    {
        s_attempts = user_auth_memory_alloc(ATTEMPT_COUNT * sizeof(*s_attempts));
        if (!s_attempts)
            return NULL;
        memset(s_attempts, 0, ATTEMPT_COUNT * sizeof(*s_attempts));
    }
#endif
    uint32_t key = client_key(req);
    attempt_t *oldest = &s_attempts[0];
    for (unsigned i = 0; i < ATTEMPT_COUNT; ++i)
    {
        if (s_attempts[i].key == key && s_attempts[i].seen)
            return &s_attempts[i];
        if (s_attempts[i].seen < oldest->seen)
            oldest = &s_attempts[i];
    }
    *oldest = (attempt_t) {.key = key};
    return oldest;
}

static bool attempt_allowed(httpd_req_t *req)
{
    int64_t now = esp_timer_get_time();
    attempt_t *a = attempt(req);
    if (!a)
        return false;
    if (a->until > now)
        return false;
    if (a->until)
        a->failures = 0;
    a->until = 0;
    a->seen = now;
    if (now - s_window >= 60000000)
    {
        s_window = now;
        s_checks = 0;
    }
    return ++s_checks <= 30;
}

static void attempt_result(httpd_req_t *req, bool ok)
{
    attempt_t *a = attempt(req);
    if (!a)
        return;
    a->seen = esp_timer_get_time();
    if (ok)
        a->failures = 0;
    else if (++a->failures >= 6)
        a->until = a->seen + 60000000;
}

static esp_err_t throttled(httpd_req_t *req)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    if (!s_attempts)
        return reply(req, "503 Service Unavailable", "{\"error\":\"Authentication memory unavailable. Try again later.\"}");
#endif
    httpd_resp_set_hdr(req, "Retry-After", "60");
    return reply(req, "429 Too Many Requests", "{\"error\":\"Too many login attempts. Wait 60 seconds.\"}");
}

static bool read_cookie(httpd_req_t *req, char token[USER_AUTH_TOKEN_LEN + 1])
{
    char cookies[1024];
    token[0] = 0;
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookies, sizeof(cookies)) != ESP_OK)
        return false;
    bool found = false;
    char *p = cookies;
    while (*p)
    {
        while (*p == ' ' || *p == ';')
            ++p;
        char *end = strchr(p, ';');
        if (!end)
            end = p + strlen(p);
        if (!strncmp(p, "betta_session=", 14))
        {
            if (found || end - p != 14 + USER_AUTH_TOKEN_LEN)
            {
                token[0] = 0;
                return false;
            }
            memcpy(token, p + 14, USER_AUTH_TOKEN_LEN);
            token[USER_AUTH_TOKEN_LEN] = 0;
            found = true;
        }
        p = *end ? end + 1 : end;
    }
    return found;
}

static bool same_origin_or_tool(httpd_req_t *req, bool cookie_auth)
{
    char origin[192], host[160], expected[192], marker[8];
    size_t length = httpd_req_get_hdr_value_len(req, "Origin");
    if (length)
    {
        if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) != ESP_OK ||
            httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK)
            return false;
        snprintf(expected, sizeof(expected), "http://%s", host);
        if (strcmp(origin, expected))
            return false;
    }
    /* Simple forms cannot supply this header; fetch must deliberately opt in.
     * Basic-auth command-line clients retain the existing JSON API contract. */
    return !cookie_auth || (httpd_req_get_hdr_value_str(req, "X-Betta-Request", marker, sizeof(marker)) == ESP_OK && !strcmp(marker, "1"));
}

static const char *method_name(httpd_req_t *req)
{
    switch (req->method)
    {
        case HTTP_GET:
            return "GET";
        case HTTP_POST:
            return "POST";
        case HTTP_PUT:
            return "PUT";
        case HTTP_DELETE:
            return "DELETE";
        default:
            return "OTHER";
    }
}

esp_err_t http_user_auth_handle(httpd_req_t *req, http_guard_handler_t next)
{
    memset(&s_current, 0, sizeof(s_current));
    s_current_token[0] = 0;
    user_role_t required = user_auth_required_role(req->uri, method_name(req));
    const user_auth_state_t state = user_auth_state();
    bool cookie = read_cookie(req, s_current_token);
    if (req->method != HTTP_GET && !same_origin_or_tool(req, cookie))
        return reply(req, "403 Forbidden", "{\"error\":\"Same-origin request required\"}");
    if (state == USER_AUTH_UNAVAILABLE &&
        (required != USER_ROLE_NONE || path_is(req, "/api/auth/session") || path_is(req, "/api/auth/login")))
        return reply(req, "503 Service Unavailable", "{\"error\":\"Account storage unavailable; access is closed\"}");
    if (state == USER_AUTH_OPEN)
    {
        s_current.role = USER_ROLE_ADMIN;
        s_current.enabled = true;
    }
    else if (state == USER_AUTH_READY && !(path_is(req, "/api/auth/login") || path_is(req, "/api/auth/logout")))
    {
        char basic[160];
        bool supplied = httpd_req_get_hdr_value_len(req, "Authorization") > 0;
        if (supplied)
        {
            if (!attempt_allowed(req))
                return throttled(req);
            bool ok =
                httpd_req_get_hdr_value_str(req, "Authorization", basic, sizeof(basic)) == ESP_OK && user_auth_basic(basic, &s_current);
            user_password_wipe(basic, sizeof(basic));
            attempt_result(req, ok);
            s_current_token[0] = 0;
        }
        else if (cookie)
        {
            (void) user_auth_session_check(s_current_token, &s_current);
        }
    }
    if (required != USER_ROLE_NONE && s_current.role == USER_ROLE_NONE)
        return reply(req, "401 Unauthorized", "{\"error\":\"Sign in required\"}");
    if (s_current.role < required)
        return reply(req, "403 Forbidden", "{\"error\":\"Insufficient permission\"}");
    esp_err_t result = next(req);
    memset(&s_current, 0, sizeof(s_current));
    user_password_wipe(s_current_token, sizeof(s_current_token));
    return result;
}

static cJSON *read_json(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 768)
        return NULL;
    char body[769];
    int used = 0;
    while (used < req->content_len)
    {
        int n = httpd_req_recv(req, body + used, req->content_len - used);
        if (n <= 0)
        {
            user_password_wipe(body, sizeof(body));
            return NULL;
        }
        used += n;
    }
    body[used] = 0;
    /* cJSON strings cannot represent embedded NUL. Reject rather than truncate. */
    cJSON *root = memchr(body, 0, used) || strstr(body, "\\u0000") ? NULL : cJSON_ParseWithLengthOpts(body, used + 1, NULL, true);
    user_password_wipe(body, sizeof(body));
    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return NULL;
    }
    for (cJSON *a = root->child; a; a = a->next)
    {
        for (cJSON *b = a->next; b; b = b->next)
        {
            if (!strcmp(a->string, b->string))
            {
                cJSON_Delete(root);
                return NULL;
            }
        }
    }
    return root;
}

static const char *string_field(cJSON *root, const char *name)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsString(value) ? value->valuestring : NULL;
}

static void erase_json(cJSON *root)
{
    for (cJSON *v = root ? root->child : NULL; v; v = v->next)
        if (v->valuestring)
            user_password_wipe(v->valuestring, strlen(v->valuestring));
    cJSON_Delete(root);
}

static esp_err_t session_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "enabled", user_auth_state() != USER_AUTH_OPEN);
    cJSON_AddBoolToObject(root, "authenticated", s_current.role != USER_ROLE_NONE);
    cJSON_AddStringToObject(root, "username", s_current.username);
    cJSON_AddStringToObject(root, "role", user_auth_role_name(s_current.role));
    cJSON_AddNumberToObject(root, "session_seconds", USER_AUTH_SESSION_SECONDS);
    return send_object(req, root);
}

static esp_err_t login_post(httpd_req_t *req)
{
    if (user_auth_state() != USER_AUTH_READY)
        return reply(req, "409 Conflict", "{\"error\":\"Create the first administrator in Users\"}");
    if (!attempt_allowed(req))
        return throttled(req);
    cJSON *root = read_json(req);
    user_auth_info_t info = {0};
    bool ok = root && user_auth_verify(string_field(root, "username"), string_field(root, "password"), &info);
    erase_json(root);
    attempt_result(req, ok);
    if (!ok)
        return reply(req, "401 Unauthorized", "{\"error\":\"Invalid username or password\"}");
    /* Replacing this browser's session does not consume a new slot on each login. */
    user_auth_logout(s_current_token);
    char token[USER_AUTH_TOKEN_LEN + 1], cookie[192];
    if (!user_auth_session_create(&info, token))
        return reply(req, "503 Service Unavailable", "{\"error\":\"Session limit reached. Sign out another browser or wait.\"}");
    snprintf(cookie, sizeof(cookie), "betta_session=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=%d", token, USER_AUTH_SESSION_SECONDS);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    s_current = info;
    esp_err_t result = session_get(req);
    user_password_wipe(token, sizeof(token));
    user_password_wipe(cookie, sizeof(cookie));
    return result;
}

static esp_err_t logout_post(httpd_req_t *req)
{
    user_auth_logout(s_current_token);
    httpd_resp_set_hdr(req, "Set-Cookie", "betta_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
    return reply(req, "200 OK", "{\"ok\":true}");
}

static esp_err_t users_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject(), *users = cJSON_AddArrayToObject(root, "users");
    cJSON_AddNumberToObject(root, "max_users", USER_AUTH_MAX_USERS);
    cJSON_AddBoolToObject(root, "enabled", user_auth_state() == USER_AUTH_READY);
    user_auth_info_t info;
    for (int i = 0; user_auth_info(i, &info); ++i)
    {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddStringToObject(row, "username", info.username);
        cJSON_AddStringToObject(row, "role", user_auth_role_name(info.role));
        cJSON_AddBoolToObject(row, "enabled", info.enabled);
        cJSON_AddItemToArray(users, row);
    }
    return send_object(req, root);
}

static esp_err_t edit_result(httpd_req_t *req, esp_err_t err)
{
    if (err == ESP_OK)
        return reply(req, "200 OK", "{\"ok\":true,\"reauthenticate\":true}");
    if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_SIZE || err == ESP_ERR_NOT_FOUND)
        return reply(
            req, "400 Bad Request",
            "{\"error\":\"Invalid account change. Keep one enabled administrator; use a unique 3-32 character name and an 8-64 byte password.\"}");
    return reply(req, "503 Service Unavailable", "{\"error\":\"Account storage or password service unavailable\"}");
}

static esp_err_t users_post(httpd_req_t *req)
{
    cJSON *root = read_json(req);
    const char *action = string_field(root, "action"), *name = string_field(root, "username");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (action && name)
    {
        if (!strcmp(action, "delete"))
            err = user_auth_delete(name);
        else if (!strcmp(action, "create") || !strcmp(action, "update"))
        {
            cJSON *enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
            const char *password = string_field(root, "password");
            if (cJSON_IsBool(enabled) && password)
                err = user_auth_update(name, password, user_auth_role_parse(string_field(root, "role")), cJSON_IsTrue(enabled),
                                       !strcmp(action, "create"));
        }
    }
    erase_json(root);
    return edit_result(req, err);
}

static esp_err_t password_post(httpd_req_t *req)
{
    if (!attempt_allowed(req))
        return throttled(req);
    cJSON *root = read_json(req);
    user_auth_info_t info;
    bool ok = root && user_auth_verify(s_current.username, string_field(root, "current_password"), &info);
    attempt_result(req, ok);
    if (!ok)
    {
        erase_json(root);
        return reply(req, "401 Unauthorized", "{\"error\":\"Current password is incorrect\"}");
    }
    const char *password = string_field(root, "new_password");
    esp_err_t err = !password || !*password ? ESP_ERR_INVALID_SIZE : user_auth_update(info.username, password, info.role, true, false);
    erase_json(root);
    return edit_result(req, err);
}

static esp_err_t auth_handler(httpd_req_t *req)
{
    http_guard_handler_t handler = NULL;
    if (path_is(req, "/api/auth/session"))
        handler = session_get;
    else if (path_is(req, "/api/auth/login"))
        handler = login_post;
    else if (path_is(req, "/api/auth/logout"))
        handler = logout_post;
    else if (path_is(req, "/api/auth/password"))
        handler = password_post;
    else if (path_is(req, "/api/users"))
        handler = req->method == HTTP_GET ? users_get : users_post;
    return handler ? http_guard_handle(req, handler) : httpd_resp_send_404(req);
}

esp_err_t http_user_auth_register(httpd_handle_t server)
{
    const httpd_uri_t routes[] = {
        {.uri = "/api/auth/session", .method = HTTP_GET, .handler = auth_handler},
        {.uri = "/api/auth/login", .method = HTTP_POST, .handler = auth_handler},
        {.uri = "/api/auth/logout", .method = HTTP_POST, .handler = auth_handler},
        {.uri = "/api/auth/password", .method = HTTP_POST, .handler = auth_handler},
        {.uri = "/api/users", .method = HTTP_GET, .handler = auth_handler},
        {.uri = "/api/users", .method = HTTP_POST, .handler = auth_handler},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); ++i)
    {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK)
            return err;
    }
    return ESP_OK;
}
