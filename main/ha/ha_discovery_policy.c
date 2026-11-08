/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_discovery_policy.h"
#include "app_config.h"
#include <string.h>

void ha_discovery_begin(ha_discovery_guard_t *guard, int64_t now_ms)
{
    ha_discovery_cancel(guard);
    guard->active = true;
    guard->started_ms = guard->progress_ms = now_ms;
}

void ha_discovery_cancel(ha_discovery_guard_t *guard)
{
    uint32_t generation = guard->generation + 1U;
    *guard = (ha_discovery_guard_t){.generation = generation ? generation : 1U};
}

void ha_discovery_finish(ha_discovery_guard_t *guard)
{
    guard->active = false;
    guard->reason = HA_DISCOVERY_WAIT_NONE;
    guard->failed = guard->timed_out = false;
}

void ha_discovery_fail(ha_discovery_guard_t *guard, ha_discovery_wait_t reason)
{
    guard->active = false;
    guard->failed = true;
    guard->reason = reason;
}

bool ha_discovery_expire(ha_discovery_guard_t *guard, int64_t now_ms)
{
    if (!guard->active || (now_ms - guard->progress_ms < HA_DISCOVERY_STALL_MS &&
                          now_ms - guard->started_ms < HA_DISCOVERY_TOTAL_MS)) return false;
    ha_discovery_fail(guard, guard->reason);
    guard->timed_out = true;
    return true;
}

bool ha_discovery_matches(const ha_discovery_guard_t *guard, uint32_t generation)
{
    return guard->active && guard->generation == generation;
}

bool ha_discovery_accept_poll(const ha_discovery_guard_t *guard, uint32_t generation)
{
    return generation == 0 || guard->generation == generation;
}

bool ha_discovery_parse_generation(const char *value, uint32_t *generation)
{
    *generation = 0;
    if (value == NULL || value[0] == '\0') return true;
    uint32_t n = 0;
    for (const char *p = value; *p; ++p) {
        if (*p < '0' || *p > '9' || n > (UINT32_MAX - (uint32_t)(*p - '0')) / 10U) return false;
        n = n * 10U + (uint32_t)(*p - '0');
    }
    if (n == 0) return false;
    *generation = n;
    return true;
}

ha_discovery_wait_t ha_discovery_memory_wait(int64_t waited_ms, size_t free_internal,
    size_t largest_internal, size_t free_psram, size_t largest_psram, const char *ws_url)
{
    bool priority = waited_ms >= HA_DISCOVERY_PRIORITY_MS;
    size_t free_min = priority ? APP_HA_DISCOVERY_USER_MIN_INTERNAL_FREE_BYTES
                              : APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_FREE_BYTES;
    size_t block_min = priority ? APP_HA_DISCOVERY_USER_MIN_INTERNAL_LARGEST_BYTES
                               : APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_LARGEST_BYTES;
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    /* Only the explicit plain-WS configuration maps to qualified plain HTTP.
     * Unknown/secure schemes retain the conservative TLS reserve. */
    if (priority && ws_url != NULL && strncmp(ws_url, "ws://", 5) == 0)
        free_min = APP_HA_DISCOVERY_HTTP_MIN_INTERNAL_FREE_BYTES;
#else
    (void)ws_url;
#endif
    if (free_internal < free_min || largest_internal < block_min) return HA_DISCOVERY_WAIT_INTERNAL;
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    /* Query/body, bounded response and cJSON nodes must fit outside internal RAM. */
    if (free_psram < 96U * 1024U || largest_psram < 16U * 1024U) return HA_DISCOVERY_WAIT_PSRAM;
#else
    (void)free_psram;
    (void)largest_psram;
#endif
    return HA_DISCOVERY_WAIT_NONE;
}

const char *ha_discovery_wait_name(ha_discovery_wait_t reason)
{
    switch (reason) {
    case HA_DISCOVERY_WAIT_INTERNAL: return "internal_memory";
    case HA_DISCOVERY_WAIT_PSRAM: return "external_memory";
    case HA_DISCOVERY_WAIT_ALLOCATION: return "memory";
    case HA_DISCOVERY_WAIT_CONNECTION: return "connection";
    case HA_DISCOVERY_WAIT_BUSY: return "busy";
    case HA_DISCOVERY_WAIT_HTTP: return "http_retry";
    case HA_DISCOVERY_WAIT_PERMISSION: return "permission";
    case HA_DISCOVERY_WAIT_RESPONSE: return "response";
    default: return "request";
    }
}

uint16_t ha_discovery_smaller_page(uint16_t page_size)
{
    return page_size > 1 ? (page_size + 1U) / 2U : 0;
}
