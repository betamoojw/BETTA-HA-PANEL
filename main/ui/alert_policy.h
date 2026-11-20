/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Unknown/missing samples retain the last usable state; boot never fires an event. */
bool alert_policy_sample(bool event, bool *primed, bool *active, char *last,
    size_t capacity, const char *state);
enum {
    ALERT_CONNECTION_WIFI_DOWN = 1U,
    ALERT_CONNECTION_HA_DOWN = 2U,
    ALERT_CONNECTION_MQTT_DOWN = 4U,
    ALERT_CONNECTION_HA_KEEP = 8U,
};
/* Disabled integrations and Wi-Fi failure clear their alerts. A busy HA snapshot
 * keeps the HA alert as it was while the integration is configured and Wi-Fi is up. */
unsigned alert_policy_connections(uint64_t uptime_ms, bool wifi_expected, bool wifi,
    bool ha_expected, bool ha_status_available, bool ha, bool mqtt_expected, bool mqtt);
