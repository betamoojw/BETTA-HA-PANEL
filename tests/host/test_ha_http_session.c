/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_http_session.h"
#include "ha/ha_discovery_policy.h"
#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_crt_bundle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, created, cleaned;
static bool init_failure, request_active;
static void *live;
static size_t free_internal = 100000, largest = 16384;
static const size_t retained_bytes = 1252; /* Measured initialization delta, not an SDK allocator model. */
static char captured_url[256], captured_cn[128];
static bool bundle;
static const char *borrowed_cn;
#define CHECK(expr) do { ++checks; assert(expr); } while (0)

size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return free_internal; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return largest; }
void discovery_test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
esp_err_t esp_crt_bundle_attach(void *conf) { (void)conf; return ESP_OK; }

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    CHECK(live == NULL);
    CHECK(config->timeout_ms == 2500 && config->keep_alive_enable);
    CHECK(config->buffer_size == 2048 && config->buffer_size_tx == APP_HA_HTTP_TX_BUFFER_SIZE);
    snprintf(captured_url, sizeof(captured_url), "%s", config->url);
    snprintf(captured_cn, sizeof(captured_cn), "%s", config->common_name ? config->common_name : "");
    bundle = config->crt_bundle_attach == esp_crt_bundle_attach;
    borrowed_cn = config->common_name;
    if (init_failure) return NULL; /* SDK init itself unwinds its partial allocation. */
    live = malloc(1);
    assert(live && free_internal > retained_bytes);
    free_internal -= retained_bytes;
    ++created;
    return live;
}

esp_err_t esp_http_client_cleanup(void *client)
{
    /* A release in a simulated synchronous request would fail this test. */
    CHECK(!request_active && client == live && live != NULL);
    free(live); live = NULL;
    free_internal += retained_bytes;
    ++cleaned;
    return ESP_OK;
}

static void ensure(ha_http_session_t *s)
{
    CHECK(ha_http_session_ensure(s, "http://ha.invalid", NULL) == ESP_OK);
    CHECK(s->handle == live && !s->idle && !bundle);
}

int main(void)
{
    ha_http_session_t s = {0};
    ha_http_session_idle(&s, 0); /* No client: no allocation or cleanup. */
    CHECK(created == 0 && cleaned == 0);
    ensure(&s);
    void *first = s.handle;
    CHECK(ha_http_session_ensure(&s, "http://ha.invalid", "") == ESP_OK);
    CHECK(s.handle == first && created == 1);
    /* All HTTP users of the session can reuse the same buffers in a burst. */
    request_active = true;
    CHECK(s.handle == first);
    request_active = false; /* The HA loop is not entered until the request returns. */
    ha_http_session_idle(&s, 20000);
    ha_http_session_idle(&s, 21999);
    CHECK(s.handle == first);
    ensure(&s); /* New request resets idle age, including forecast after state sync. */
    ha_http_session_idle(&s, 22000);
    ha_http_session_idle(&s, 23999);
    CHECK(s.handle == first);
    ha_http_session_idle(&s, 24000);
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    CHECK(s.handle == NULL && live == NULL && cleaned == 1 && free_internal == 100000);
#else
    CHECK(s.handle == first && cleaned == 0); /* P4 never adopts the idle timeout. */
#endif
    ha_http_session_reset(&s, "test-stop");
    int cleanup_count = cleaned;
    ha_http_session_reset(&s, "test-stop-again");
    CHECK(cleaned == cleanup_count && !s.handle && !s.base_url[0] && !s.cert_common_name[0]);

    ensure(&s);
    char caller_cn[] = "ha.invalid";
    CHECK(ha_http_session_ensure(&s, "https://ha.invalid", caller_cn) == ESP_OK);
    CHECK(bundle && strcmp(captured_url, "https://ha.invalid") == 0 && strcmp(captured_cn, "ha.invalid") == 0);
    memset(caller_cn, 'x', sizeof(caller_cn) - 1);
    CHECK(borrowed_cn == s.cert_common_name && strcmp(borrowed_cn, "ha.invalid") == 0);
    int init_count = created;
    CHECK(ha_http_session_ensure(&s, "https://ha.invalid", "ha.invalid") == ESP_OK);
    CHECK(created == init_count);
    CHECK(ha_http_session_ensure(&s, "https://ha.invalid", "other.invalid") == ESP_OK);
    CHECK(created == init_count + 1 && strcmp(captured_cn, "other.invalid") == 0);
    CHECK(ha_http_session_ensure(&s, "https://ha.invalid", NULL) == ESP_OK);
    CHECK(!captured_cn[0] && bundle); /* No stale certificate-name override. */
    CHECK(ha_http_session_ensure(&s, NULL, NULL) == ESP_ERR_INVALID_ARG);
    CHECK(ha_http_session_ensure(&s, "", NULL) == ESP_ERR_INVALID_ARG);
    char too_long[257]; memset(too_long, 'x', sizeof(too_long)); too_long[256] = '\0';
    CHECK(ha_http_session_ensure(&s, too_long, NULL) == ESP_ERR_INVALID_SIZE);
    CHECK(ha_http_session_ensure(&s, "http://ha.invalid", too_long) == ESP_ERR_INVALID_SIZE);
    CHECK(s.handle != NULL); /* Rejected context never damages the existing client. */

    init_failure = true;
    CHECK(ha_http_session_ensure(&s, "http://changed.invalid", NULL) == ESP_ERR_NO_MEM);
    CHECK(!s.handle && !live && !s.base_url[0] && !s.cert_common_name[0]);
    init_failure = false;
    ensure(&s); /* Allocation failure is recoverable on the next request. */

    free_internal = 36255; /* The real follow-up repeatedly missed 36 KiB by 609 bytes. */
    ha_http_session_idle(&s, 25000);
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    CHECK(!s.handle && free_internal == 36255 + retained_bytes);
    CHECK(ha_discovery_memory_wait(8000, 36255, 8704, 98304, 16384, "ws://ha.invalid") == HA_DISCOVERY_WAIT_NONE);
    CHECK(ha_discovery_memory_wait(8000, 36255, 8704, 98304, 16384, "wss://ha.invalid") == HA_DISCOVERY_WAIT_INTERNAL);
    free_internal = 100000;
    ensure(&s); free_internal = 36864; largest = 8192;
    ha_http_session_idle(&s, 26000);
    CHECK(s.handle != NULL); /* Exact idle-pressure boundaries retain the client. */
    largest = 8191;
    ha_http_session_idle(&s, 26001);
    CHECK(!s.handle); /* Contiguous-memory pressure also reclaims idle buffers. */

    free_internal = 33000 + retained_bytes; largest = 8704;
    ensure(&s);
    ha_discovery_guard_t guard = {0}; ha_discovery_begin(&guard, 100);
    uint32_t generation = guard.generation;
    ha_http_session_idle(&s, 27000); /* Cleanup alone cannot make an unsafe heap admissible. */
    CHECK(!s.handle && free_internal == 33000 + retained_bytes);
    guard.reason = ha_discovery_memory_wait(8000, free_internal, largest, 98304, 16384, "ws://ha.invalid");
    CHECK(guard.reason == HA_DISCOVERY_WAIT_INTERNAL);
    CHECK(!ha_discovery_expire(&guard, 45099));
    CHECK(ha_discovery_expire(&guard, 45100) && guard.timed_out);
    free_internal = 35319; /* Later recovery followed by explicit retry. */
    ha_discovery_begin(&guard, 50000);
    CHECK(guard.generation != generation && !guard.failed);
    CHECK(ha_discovery_memory_wait(8000, free_internal, largest, 98304, 16384, "ws://ha.invalid") == HA_DISCOVERY_WAIT_NONE);
    ensure(&s);
    CHECK(free_internal >= APP_HA_DISCOVERY_PREPARED_INTERNAL_FREE_BYTES);
    generation = guard.generation;
    ha_discovery_cancel(&guard); /* API cancellation only invalidates the generation. */
    CHECK(!ha_discovery_matches(&guard, generation) && s.handle != NULL);
    ha_http_session_idle(&s, 28000); /* Cleanup stays with the HA task, after I/O returns. */
    CHECK(s.handle == NULL);
#else
    CHECK(s.handle != NULL && free_internal == 36255);
    largest = 1; free_internal = 1;
    ha_http_session_idle(&s, 1000000);
    CHECK(s.handle != NULL); /* Neither S3 pressure rule changes P4 retention. */
#endif
    ha_http_session_reset(&s, "shutdown");
    CHECK(live == NULL && created == cleaned);
    printf("HA HTTP session: %d checks passed; reuse, pressure/idle release, TLS context, allocation failure, recovery and shutdown\n", checks);
    return 0;
}
