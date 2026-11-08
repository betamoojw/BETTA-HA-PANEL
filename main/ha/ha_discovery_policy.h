/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HA_DISCOVERY_PRIORITY_MS 8000
#define HA_DISCOVERY_STALL_MS 45000
#define HA_DISCOVERY_TOTAL_MS 180000

typedef enum {
    HA_DISCOVERY_WAIT_NONE,
    HA_DISCOVERY_WAIT_INTERNAL,
    HA_DISCOVERY_WAIT_PSRAM,
    HA_DISCOVERY_WAIT_ALLOCATION,
    HA_DISCOVERY_WAIT_CONNECTION,
    HA_DISCOVERY_WAIT_BUSY,
    HA_DISCOVERY_WAIT_HTTP,
    HA_DISCOVERY_WAIT_PERMISSION,
    HA_DISCOVERY_WAIT_RESPONSE,
} ha_discovery_wait_t;

/* Protected by the HA client mutex. Deadlines use monotonic time, never wall time. */
typedef struct {
    uint32_t generation;
    int64_t started_ms;
    int64_t progress_ms;
    ha_discovery_wait_t reason;
    bool active;
    bool failed;
    bool timed_out;
} ha_discovery_guard_t;

void ha_discovery_begin(ha_discovery_guard_t *guard, int64_t now_ms);
void ha_discovery_cancel(ha_discovery_guard_t *guard);
void ha_discovery_finish(ha_discovery_guard_t *guard);
void ha_discovery_fail(ha_discovery_guard_t *guard, ha_discovery_wait_t reason);
bool ha_discovery_expire(ha_discovery_guard_t *guard, int64_t now_ms);
bool ha_discovery_matches(const ha_discovery_guard_t *guard, uint32_t generation);
/* Zero is the legacy/unconditional request. A poll never owns a newer query. */
bool ha_discovery_accept_poll(const ha_discovery_guard_t *guard, uint32_t generation);
bool ha_discovery_parse_generation(const char *value, uint32_t *generation);
ha_discovery_wait_t ha_discovery_memory_wait(int64_t waited_ms, size_t free_internal,
    size_t largest_internal, size_t free_psram, size_t largest_psram, const char *ws_url);
const char *ha_discovery_wait_name(ha_discovery_wait_t reason);
uint16_t ha_discovery_smaller_page(uint16_t page_size);
