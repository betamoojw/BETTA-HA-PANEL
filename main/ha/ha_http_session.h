/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "esp_http_client.h"
#include <stdbool.h>
#include <stdint.h>

/* Owned by the HA task. No HTTP/API callback may release this shared handle. */
typedef struct {
    esp_http_client_handle_t handle;
    char base_url[256];
    char cert_common_name[128];
    int64_t idle_since_ms;
    bool idle;
} ha_http_session_t;

/* Input strings are copied; they must not alias the session's storage. */
esp_err_t ha_http_session_ensure(ha_http_session_t *session, const char *base_url, const char *cert_common_name);
void ha_http_session_reset(ha_http_session_t *session, const char *reason);
/* HA-task loop boundary only, after all synchronous HTTP work has returned.
 * close() keeps the SDK buffers/headers; this reclaims idle S3 handles with cleanup().
 * P4 retains its existing reuse policy. Stop/reset calls require the owner stopped. */
void ha_http_session_idle(ha_http_session_t *session, int64_t now_ms);
