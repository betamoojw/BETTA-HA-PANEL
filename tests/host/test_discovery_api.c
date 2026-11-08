/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "api/api_routes.h"
#include "ha/ha_client.h"
#include "ha/ha_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t observed_generation;
static int get_calls, deletes, areas;
static char observed_search[64], observed_domain[48];
static bool observed_refresh;
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
esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *s) { (void)r; assert(s); return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *s) { (void)r; assert(!strcmp(s,"application/json")); return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *k, const char *v) { (void)r; (void)k; (void)v; return ESP_OK; }
bool ha_client_is_connected(void) { return true; }
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
int main(void)
{
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
    puts("discovery API: poll/cancel generations, invalid query rejection, legacy requests and area dispatch passed");
    return 0;
}
