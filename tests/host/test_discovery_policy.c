/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_discovery_policy.h"
#include "app_config.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    assert(APP_HA_LIGHT_DISCOVERY_PAGE_SIZE == 12);
    assert(APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES == 8192);
    /* Real post-request and idle-editor waits: ordinary HTTP can now fit. */
    assert(APP_HA_DISCOVERY_HTTP_MIN_INTERNAL_FREE_BYTES == 34816);
    const size_t observed[] = {36255, 35319, 35003};
    for (size_t i = 0; i < sizeof(observed) / sizeof(observed[0]); ++i) {
        assert(ha_discovery_memory_wait(7999, observed[i], 8704, 98304, 16384, "ws://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
        assert(ha_discovery_memory_wait(8000, observed[i], 8704, 98304, 16384, "ws://test/api/websocket") == HA_DISCOVERY_WAIT_NONE);
        assert(ha_discovery_memory_wait(8000, observed[i], 8704, 98304, 16384, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    }
    assert(ha_discovery_memory_wait(8000, 34815, 8192, 98304, 16384, "ws://test") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 34816, 8192, 98304, 16384, "ws://test") == HA_DISCOVERY_WAIT_NONE);
    assert(ha_discovery_memory_wait(8000, 34816, 8191, 98304, 16384, "ws://test") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 34816, 8192, 98303, 16384, "ws://test") == HA_DISCOVERY_WAIT_PSRAM);
    assert(ha_discovery_memory_wait(8000, 34816, 8192, 98304, 16383, "ws://test") == HA_DISCOVERY_WAIT_PSRAM);
    assert(ha_discovery_memory_wait(8000, 36255, 8192, 98304, 16384, NULL) == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 36255, 8192, 98304, 16384, "") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 36255, 8192, 98304, 16384, "https://test") == HA_DISCOVERY_WAIT_INTERNAL);
    /* Captured COM18 idle: it used to wait forever even after escalation. */
    assert(ha_discovery_memory_wait(0, 40047, 16384, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 40047, 16384, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_NONE);
    assert(ha_discovery_memory_wait(8000, 36863, 16384, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 40047, 8191, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 36864, 8192, 98304, 16384, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_NONE);
    /* Post-flash COM18: 8.5 KiB cannot recover to the former 12 KiB floor. */
    assert(ha_discovery_memory_wait(7999, 39891, 8704, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 39891, 8704, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_NONE);
    assert(ha_discovery_memory_wait(8000, 35003, 8704, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 40047, 16384, 98303, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_PSRAM);
    assert(ha_discovery_memory_wait(8000, 40047, 16384, 743324, 16383, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_PSRAM);
#else
    assert(APP_HA_LIGHT_DISCOVERY_PAGE_SIZE == 40);
    assert(APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES == 16384);
    /* Transport selection never relaxes P4 admission. */
    assert(ha_discovery_memory_wait(8000, 36255, 16384, 98304, 16384, "ws://test") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 65535, 24576, 0, 0, "ws://test") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 65536, 24576, 0, 0, "ws://test") == HA_DISCOVERY_WAIT_NONE);
    assert(ha_discovery_memory_wait(8000, 40047, 16384, 743324, 131072, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(7999, 65536, 24576, 0, 0, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 65536, 24576, 0, 0, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_NONE);
    assert(ha_discovery_memory_wait(0, 122880, 49152, 0, 0, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_NONE);
    assert(ha_discovery_memory_wait(0, 122879, 49152, 0, 0, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
    assert(ha_discovery_memory_wait(8000, 65536, 24575, 0, 0, "wss://test/api/websocket") == HA_DISCOVERY_WAIT_INTERNAL);
#endif
    ha_discovery_guard_t g = {0};
    uint32_t parsed;
    assert(ha_discovery_parse_generation(NULL, &parsed) && parsed == 0);
    assert(ha_discovery_parse_generation("4294967295", &parsed) && parsed == UINT32_MAX);
    const char *invalid[] = {"0", "-1", "1x", "1.5", "4294967296", "10000000000", "+1"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        assert(!ha_discovery_parse_generation(invalid[i], &parsed));
    uint16_t page = 12;
    const uint16_t smaller[] = {6, 3, 2, 1, 0};
    for (size_t i = 0; i < sizeof(smaller) / sizeof(smaller[0]); ++i) {
        page = ha_discovery_smaller_page(page);
        assert(page == smaller[i]);
    }
    assert(ha_discovery_smaller_page(40) == 20);
    ha_discovery_begin(&g, 100);
    uint32_t first = g.generation;
    assert(ha_discovery_accept_poll(&g, 0) && ha_discovery_accept_poll(&g, first));
    g.reason = HA_DISCOVERY_WAIT_INTERNAL;
    for (int t = 100; t < 45100; t += 700) assert(!ha_discovery_expire(&g, t));
    assert(ha_discovery_expire(&g, 45100));
    assert(g.failed && g.timed_out && !g.active && g.reason == HA_DISCOVERY_WAIT_INTERNAL);
    assert(!ha_discovery_matches(&g, first));
    assert(ha_discovery_accept_poll(&g, first)); /* Polls may read terminal state, never restart it. */
    assert(!ha_discovery_expire(&g, 90000)); /* Polling never reactivates timed-out work. */
    ha_discovery_begin(&g, 90000); /* Explicit retry clears the previous error. */
    assert(g.active && !g.failed && !g.timed_out && g.generation != first);
    assert(!ha_discovery_matches(&g, first));
    assert(!ha_discovery_accept_poll(&g, first)); /* Old GET/DELETE cannot steal or cancel the retry. */
    g.progress_ms = 130000;
    assert(!ha_discovery_expire(&g, 150000));
    uint32_t retry = g.generation;
    ha_discovery_cancel(&g);
    assert(!g.active && !g.failed && !ha_discovery_matches(&g, retry));
    ha_discovery_begin(&g, 150001);
    assert(!ha_discovery_matches(&g, retry)); /* Delayed result from same-query cancellation. */
    ha_discovery_fail(&g, HA_DISCOVERY_WAIT_PERMISSION);
    assert(g.failed && !g.active && !g.timed_out);
    ha_discovery_begin(&g, 200000);
    ha_discovery_finish(&g);
    assert(!g.active && !g.failed && g.reason == HA_DISCOVERY_WAIT_NONE);
    ha_discovery_begin(&g, 300000);
    for (int64_t now = 340000; now < 480000; now += 40000) {
        g.progress_ms = now;
        assert(!ha_discovery_expire(&g, now));
    }
    g.progress_ms = 479999;
    assert(ha_discovery_expire(&g, 480000)); /* Progress cannot extend the total deadline. */
    g.generation = UINT32_MAX;
    ha_discovery_begin(&g, 1);
    assert(g.generation == 1);
    puts("discovery policy: board budgets, recovery, bounded waits, cancel, stale results and retry passed");
    return 0;
}
