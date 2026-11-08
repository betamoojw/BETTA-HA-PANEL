/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_http_session.h"
#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include <string.h>

void ha_http_session_reset(ha_http_session_t *s, const char *reason)
{
    if (s->handle != NULL) {
        size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        esp_http_client_cleanup(s->handle);
        ESP_LOGI("ha_client", "HTTP client released: reason=%s internal_before=%u internal_after=%u block_after=%u",
            reason, (unsigned)before,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    *s = (ha_http_session_t){0};
}

esp_err_t ha_http_session_ensure(ha_http_session_t *s, const char *base_url, const char *cert_common_name)
{
    if (base_url == NULL || base_url[0] == '\0') return ESP_ERR_INVALID_ARG;
    const char *cn = cert_common_name ? cert_common_name : "";
    if (strlen(base_url) >= sizeof(s->base_url) || strlen(cn) >= sizeof(s->cert_common_name))
        return ESP_ERR_INVALID_SIZE;
    if (s->handle != NULL && strcmp(s->base_url, base_url) == 0 && strcmp(s->cert_common_name, cn) == 0) {
        s->idle = false;
        return ESP_OK;
    }
    ha_http_session_reset(s, "endpoint");
    memcpy(s->base_url, base_url, strlen(base_url) + 1U);
    memcpy(s->cert_common_name, cn, strlen(cn) + 1U);
    esp_http_client_config_t config = {
        .url = s->base_url, .timeout_ms = 2500, .keep_alive_enable = true,
        .buffer_size = 2048, .buffer_size_tx = APP_HA_HTTP_TX_BUFFER_SIZE,
    };
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    if (strncmp(base_url, "https://", 8) == 0) config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    /* IDF's SSL transport borrows this pointer, including after init returns. */
    if (s->cert_common_name[0]) config.common_name = s->cert_common_name;
    size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s->handle = esp_http_client_init(&config);
    ESP_LOGI("ha_client", "HTTP client memory: internal_before=%u internal_after=%u block_before=%u block_after=%u ok=%d",
        (unsigned)before, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)block, (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        s->handle != NULL);
    if (s->handle == NULL) {
        ha_http_session_reset(s, "init_failure");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void ha_http_session_idle(ha_http_session_t *s, int64_t now_ms)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    if (s->handle == NULL) return;
    if (!s->idle) {
        s->idle_since_ms = now_ms;
        s->idle = true;
    }
    /* Check pressure even during a pending discovery wait. This never runs in an
     * HTTP call: the HA task reaches here only after returning to its loop. */
    bool pressure = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) <
            APP_HA_DISCOVERY_USER_MIN_INTERNAL_FREE_BYTES ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) <
            APP_HA_DISCOVERY_USER_MIN_INTERNAL_LARGEST_BYTES;
    if (pressure || now_ms - s->idle_since_ms >= APP_HA_HTTP_IDLE_RELEASE_MS)
        ha_http_session_reset(s, pressure ? "internal_memory" : "idle");
#else
    (void)s;
    (void)now_ms;
#endif
}
