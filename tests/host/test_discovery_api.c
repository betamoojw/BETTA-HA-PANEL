/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "api/api_routes.h"
#include "ha/ha_auth_state.h"
#include "ha/ha_client.h"
#include "ha/ha_model.h"
#include "settings/runtime_settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t observed_generation;
static int get_calls, deletes, areas;
static char observed_search[64], observed_domain[48];
static bool observed_refresh;
static bool connected = true, url_set, token_set, settings_unavailable;
static unsigned settings_reads;
static ha_auth_block_t auth;
static char last_payload[64];
size_t httpd_req_get_url_query_len(httpd_req_t *r) { return strlen(r->query); }
esp_err_t httpd_req_get_url_query_str(httpd_req_t *r, char *out, size_t cap)
{
    if (r->query_error) return ESP_FAIL;
    assert(strlen(r->query) < cap); strcpy(out, r->query); return ESP_OK;
}
esp_err_t httpd_query_key_value(const char *q, const char *key, char *out, size_t cap)
{
    for (const char *p = q; p && *p; p = strchr(p, '&'), p = p ? p + 1 : NULL) {
        if (strncmp(p, key, strlen(key)) || p[strlen(key)] != '=') continue;
        p += strlen(key) + 1;
        size_t len = strcspn(p, "&");
        if (len >= cap) return ESP_ERR_INVALID_SIZE;
        memcpy(out, p, len); out[len] = 0; return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}
esp_err_t httpd_resp_send_500(httpd_req_t *r) { (void)r; return 500; }
esp_err_t httpd_resp_send_err(httpd_req_t *r, int code, const char *s) { (void)r; (void)s; return code; }
esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *s)
{
    (void)r;
    assert(s != NULL && strlen(s) < sizeof(last_payload));
    strcpy(last_payload, s);
    return ESP_OK;
}
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *s) { (void)r; assert(!strcmp(s,"application/json")); return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *k, const char *v) { (void)r; (void)k; (void)v; return ESP_OK; }
bool ha_client_is_connected(void) { return connected; }
ha_auth_block_t ha_auth_state_reason(void) { return auth; }
esp_err_t runtime_settings_load(runtime_settings_t *out)
{
    ++settings_reads;
    if (settings_unavailable) return ESP_FAIL;
    if (url_set) strcpy(out->ha_ws_url, "ws://ha.test/api/websocket");
    if (token_set) strcpy(out->ha_access_token, "test-token");
    return ESP_OK;
}
bool runtime_settings_has_ha(const runtime_settings_t *settings)
{
    return settings->ha_ws_url[0] != '\0' && settings->ha_access_token[0] != '\0';
}
size_t ha_model_list_entities_page(const char *domain, const char *search, size_t offset,
    ha_entity_info_t *out, size_t cap, size_t *total)
{
    (void)domain; (void)search; (void)offset; (void)out; (void)cap; *total = 0; return 0;
}
esp_err_t ha_client_get_domain_entities_json(const char *domain, const char *search, bool refresh,
    uint32_t generation, char **out)
{
    get_calls++; observed_generation = generation; observed_refresh = refresh;
    strcpy(observed_domain, domain); strcpy(observed_search, search);
    *out = malloc(3); assert(*out); strcpy(*out, "{}");
    return ESP_OK;
}
esp_err_t ha_client_get_room_areas_json(bool refresh, char **out)
{
    (void)refresh; areas++; *out = malloc(3); strcpy(*out, "{}"); return ESP_OK;
}
esp_err_t ha_client_cancel_domain_entities(const char *domain, const char *search, uint32_t generation)
{
    deletes++; observed_generation = generation; strcpy(observed_domain, domain); strcpy(observed_search, search);
    return ESP_OK;
}
static void check_availability(void)
{
    const char *bad_status[] = {"status_only=", "status_only=0", "status_only=10", "status_only=invalid"};
    for (size_t i = 0; i < sizeof(bad_status) / sizeof(*bad_status); ++i) {
        httpd_req_t bad = {.query = bad_status[i]};
        assert(api_light_entities_get_handler(&bad) == 400);
        assert(get_calls == 0 && areas == 0 && deletes == 0);
    }
    httpd_req_t r = {.query = "status_only=1&domain=any&refresh=1"};
    assert(api_light_entities_get_handler(&r) == ESP_OK);
    assert(strcmp(last_payload, "{\"status\":\"ready\"}") == 0 && settings_reads == 0);
    connected = false;
    assert(api_light_entities_get_handler(&r) == ESP_OK);
    assert(strcmp(last_payload, "{\"status\":\"not_configured\"}") == 0);
    url_set = true;
    assert(api_light_entities_get_handler(&r) == ESP_OK);
    assert(strcmp(last_payload, "{\"status\":\"not_configured\"}") == 0);
    token_set = true;
    assert(api_light_entities_get_handler(&r) == ESP_OK);
    assert(strcmp(last_payload, "{\"status\":\"disconnected\"}") == 0 && settings_reads == 3);
    auth = HA_AUTH_TOKEN_REJECTED;
    assert(api_light_entities_get_handler(&r) == ESP_OK);
    assert(strcmp(last_payload, "{\"status\":\"token_rejected\"}") == 0);
    auth = HA_AUTH_FORBIDDEN;
    assert(api_light_entities_get_handler(&r) == ESP_OK);
    assert(strcmp(last_payload, "{\"status\":\"forbidden\"}") == 0 && settings_reads == 3);
    auth = HA_AUTH_OK;
    settings_unavailable = true;
    assert(api_light_entities_get_handler(&r) == 500);
    assert(get_calls == 0 && areas == 0 && deletes == 0); /* No discovery side effects. */
    settings_unavailable = false;
    connected = true;
}

int main(void)
{
    check_availability();
    httpd_req_t r = {.query="domain=any&search=button&generation=41"};
    assert(api_light_entities_get_handler(&r) == ESP_OK && get_calls == 1 && observed_generation == 41);
    assert(!strcmp(observed_domain, "any") && !strcmp(observed_search,"button") && !observed_refresh);
    assert(api_light_entities_delete_handler(&r) == ESP_OK && deletes == 1 && observed_generation == 41);
    r.query="domain=any&search=light%20test&refresh=1";
    assert(api_light_entities_get_handler(&r) == ESP_OK && get_calls == 2 && observed_generation == 0 && observed_refresh);
    assert(!strcmp(observed_search, "light test"));
    const char *bad[] = {"generation=", "generation=0", "generation=-1", "generation=4294967296",
        "generation=42949672951", "generation=2x"};
    for (size_t i=0; i<sizeof(bad)/sizeof(bad[0]); ++i) {
        r.query=bad[i];
        assert(api_light_entities_get_handler(&r) == 400 && get_calls == 2);
        assert(api_light_entities_delete_handler(&r) == 400 && deletes == 1);
    }
    r.query="domain=any&search=light&generation=4294967295";
    assert(api_light_entities_get_handler(&r) == ESP_OK && observed_generation == UINT32_MAX);
    r.query_error=true;
    assert(api_light_entities_get_handler(&r) == 400 && get_calls == 3);
    assert(api_light_entities_delete_handler(&r) == 400 && deletes == 1);
    r.query_error=false; r.query="domain=room&areas_only=1";
    assert(api_light_entities_get_handler(&r) == ESP_OK && areas == 1 && get_calls == 3);
    puts("discovery API: HA availability, query generations, legacy requests and area dispatch passed");
    return 0;
}
