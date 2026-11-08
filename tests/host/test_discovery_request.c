/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_discovery_request.h"
#include "app_config.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *live[2048];
static bool parse_live[2048];
static int parse_live_count;
static int allocations, fail_at, live_count, closes, reads, writes, auth_status;
static size_t offset, fragment, max_payload, max_body;
static int64_t length_override, clock_us, read_step_us;
static int64_t open_step_us, write_step_us, headers_step_us;
static int headers;
static char last_log[512];
static int open_error, status_code;
static bool read_error, incomplete, short_write, header_error, read_memory_drop;
static const char *response;
static size_t internal_free, internal_block, connected_free, connected_block;
static int opens;
static bool parse_memory_drop;

static void *tracked_malloc(size_t size)
{
    if (parse_memory_drop && reads > 0) internal_free = 24575;
    if (++allocations == fail_at) return NULL;
    void *p = malloc(size);
    assert(p != NULL);
    for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); ++i) {
        if (!live[i]) {
            live[i] = p; live_count++;
            parse_live[i] = parse_memory_drop && reads > 0;
            if (parse_live[i]) parse_live_count++;
            return p;
        }
    }
    abort();
}
void heap_caps_free(void *p)
{
    if (!p) return;
    for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); ++i) {
        if (live[i] == p) {
            live[i] = NULL; live_count--;
            if (parse_live[i] && --parse_live_count == 0) internal_free = connected_free;
            parse_live[i] = false;
            break;
        }
    }
    free(p);
}
void *heap_caps_calloc(size_t n, size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    void *p = tracked_malloc(n * size);
    if (p) memset(p, 0, n * size);
    if (n * size > max_payload) max_payload = n * size;
    return p;
}
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return internal_free; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return internal_block; }
int64_t esp_timer_get_time(void) { return clock_us; }
void discovery_test_log(const char *tag, const char *format, ...)
{
    (void)tag;
    va_list args;
    va_start(args, format);
    vsnprintf(last_log, sizeof(last_log), format, args);
    va_end(args);
}
esp_err_t esp_http_client_set_url(void *c, const char *s) { (void)c; assert(strstr(s, "/api/template")); return 0; }
esp_err_t esp_http_client_set_method(void *c, int method) { (void)c; assert(method == HTTP_METHOD_POST); return 0; }
esp_err_t esp_http_client_set_header(void *c, const char *key, const char *value) { (void)c; (void)key; (void)value; return header_error ? ESP_ERR_NO_MEM : 0; }
static esp_err_t open_http(void *client, int len, const char *reason)
{
    assert(client && len > 0 && strcmp(reason, "light-discovery") == 0);
    if ((size_t)len > max_body) max_body = (size_t)len;
    opens++;
    clock_us += open_step_us;
    internal_free = connected_free;
    internal_block = connected_block;
    return open_error;
}
int esp_http_client_write(void *c, const char *body, int len)
{
    (void)c;
    assert(strstr(body, "template") && (int)strlen(body) == len);
    writes++;
    clock_us += write_step_us;
    return short_write ? len - 1 : len;
}
int64_t esp_http_client_fetch_headers(void *c)
{
    (void)c; headers++; clock_us += headers_step_us;
    return length_override == -2 ? (int64_t)strlen(response) : length_override;
}
int esp_http_client_get_status_code(void *c) { (void)c; return status_code; }
static void report_status(int status, bool is_template) { assert(is_template); auth_status = status; }
int esp_http_client_read(void *c, char *out, int len)
{
    (void)c;
    reads++;
    if (read_memory_drop) internal_free = 24575;
    clock_us += read_step_us;
    if (read_error) return -1;
    size_t n = strlen(response) - offset;
    if (n > (size_t)len) n = (size_t)len;
    if (n > fragment) n = fragment;
    memcpy(out, response + offset, n);
    offset += n;
    return (int)n;
}
bool esp_http_client_is_complete_data_received(void *c) { (void)c; return !incomplete && offset == strlen(response); }
esp_err_t esp_http_client_close(void *c) { (void)c; closes++; return 0; }

static void reset(void)
{
    assert(live_count == 0);
    assert(parse_live_count == 0);
    allocations = fail_at = closes = reads = writes = auth_status = 0;
    opens = 0;
    internal_free = connected_free = 40047;
    internal_block = connected_block = 8704;
    offset = 0; fragment = 7; clock_us = read_step_us = 0;
    open_step_us = write_step_us = headers_step_us = 0;
    headers = 0; last_log[0] = '\0';
    open_error = 0; status_code = 200; length_override = -2;
    read_error = incomplete = short_write = header_error = read_memory_drop = false;
    parse_memory_drop = false;
    response = "{\"total\":1,\"items\":[{\"id\":\"light.test\",\"name\":\"Test\"}]}";
}

int main(void)
{
    cJSON_Hooks hooks = {.malloc_fn = tracked_malloc, .free_fn = heap_caps_free};
    cJSON_InitHooks(&hooks);
    ha_discovery_request_t r = {.client = (void *)1, .url = "http://example.invalid/api/template",
        .token = "synthetic", .host = "", .domain = "any", .search = "test",
        .page_size = APP_HA_LIGHT_DISCOVERY_PAGE_SIZE, .open = open_http, .auth_status = report_status};
    cJSON *out;
    reset();
    assert(ha_discovery_request_page(&r, &out) == ESP_OK);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(out, "items")) == 1);
    assert(auth_status == 200 && closes == 1 && reads > 1);
    int successful_allocations = allocations;
    cJSON_Delete(out);
    /* Exercise reserve boundaries with mocked SDK calls, not a real allocation peak. */
    reset(); internal_free = 32768; internal_block = 4096;
    connected_free = 24576; connected_block = 4096;
    assert(ha_discovery_request_page(&r, &out) == ESP_OK); cJSON_Delete(out);
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    assert(APP_HA_HTTP_TX_BUFFER_SIZE > 1024);
    reset(); internal_free = 32767;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && opens == 0 && !out);
    reset(); internal_block = 4095;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && opens == 0 && !out);
    reset(); connected_free = 24575;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && opens == 1 && !writes && !out && closes == 1);
    reset(); connected_block = 4095;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && !writes && !out);
    reset(); read_memory_drop = true;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && reads == 1 && !out && closes == 1);
    reset(); parse_memory_drop = true;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && reads > 1 && !out && closes == 1);
    assert(internal_free == connected_free);
    assert(strstr(last_log, "sampled_free_min=24575 ") != NULL);
    reset(); /* Recovery releases the temporary buffers and permits the next attempt. */
    assert(ha_discovery_request_page(&r, &out) == ESP_OK); cJSON_Delete(out);
#else
    assert(APP_HA_HTTP_TX_BUFFER_SIZE == 1024);
    reset(); internal_free = connected_free = 1; internal_block = connected_block = 1;
    assert(ha_discovery_request_page(&r, &out) == ESP_OK); cJSON_Delete(out);
#endif
    reset(); length_override = 0; /* IDF uses zero for unknown/chunked length. */
    assert(ha_discovery_request_page(&r, &out) == ESP_OK); cJSON_Delete(out);
    reset(); length_override = -1; /* A status line can precede a header read failure. */
    assert(ha_discovery_request_page(&r, &out) == ESP_FAIL && !out && !reads && !auth_status && closes == 1);
    reset(); length_override = -ESP_ERR_HTTP_EAGAIN;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && !reads && !auth_status && closes == 1);
    reset(); open_error = ESP_ERR_TIMEOUT;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && !writes && !reads);
    reset(); short_write = true;
    assert(ha_discovery_request_page(&r, &out) == ESP_FAIL && !out && closes == 1);
    reset(); header_error = true;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_NO_MEM && !out && !writes && !reads);
    reset(); status_code = 403;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_INVALID_RESPONSE && !out && auth_status == 403);
    reset(); length_override = APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_INVALID_SIZE && !reads && !out);
    reset(); read_error = true;
    assert(ha_discovery_request_page(&r, &out) == ESP_FAIL && !out && closes == 1);
    reset(); incomplete = true;
    assert(ha_discovery_request_page(&r, &out) != ESP_OK && !out);
    reset(); response = "{\"total\":20}";
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_INVALID_RESPONSE && !out);
    reset(); response = "not json";
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_INVALID_RESPONSE && !out);
    reset(); fragment = 1; read_step_us = 1000000;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && reads == 5);
    reset(); open_step_us = 5000000;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && !writes && !headers && closes == 1);
    reset(); write_step_us = 5000000;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && !headers && closes == 1);
    reset(); headers_step_us = 5000000;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && !reads && closes == 1);
    reset(); fragment = 4096; read_step_us = 5000000;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_TIMEOUT && !out && reads == 1 && closes == 1);
    reset();
    char *huge = malloc(APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES + 8);
    memset(huge, ' ', APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES + 7);
    huge[APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES + 7] = 0;
    response = huge; length_override = 0; fragment = 2000;
    assert(ha_discovery_request_page(&r, &out) == ESP_ERR_INVALID_SIZE && !out);
    free(huge);
    for (int i = 1; i <= successful_allocations; ++i) {
        reset(); fail_at = i;
        esp_err_t result = ha_discovery_request_page(&r, &out);
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        assert(result != ESP_OK && out == NULL); /* PSRAM failure never consumes internal RAM. */
#else
        (void)result; cJSON_Delete(out); /* Preserve the existing P4 payload fallback. */
#endif
    }
    reset(); r.domain = "room";
    assert(ha_discovery_request_page(&r, &out) == ESP_OK); cJSON_Delete(out);
    reset(); fail_at = 1;
    void *cache = ha_discovery_calloc(640, 528);
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    assert(cache == NULL);
#else
    assert(cache != NULL);
#endif
    heap_caps_free(cache);
    reset();
    printf("discovery HTTP: bounded pages, budget/error cleanup, auth, truncation, timeout and %d allocation failures passed; max_body=%u max_buffer=%u\n",
        successful_allocations, (unsigned)max_body, (unsigned)max_payload);
    return 0;
}
