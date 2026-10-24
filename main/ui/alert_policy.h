/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Unknown/missing samples retain the last usable state; boot never fires an event. */
bool alert_policy_sample(bool event, bool *primed, bool *active, char *last,
    size_t capacity, const char *state);
/* Optional integrations stay silent when disabled; Wi-Fi failure suppresses duplicates. */
unsigned alert_policy_connections(uint64_t uptime_ms, bool wifi_expected, bool wifi,
    bool ha_expected, bool ha, bool mqtt_expected, bool mqtt);
