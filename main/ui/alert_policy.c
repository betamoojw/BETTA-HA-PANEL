/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/alert_policy.h"
#include <stdio.h>
#include <string.h>
bool alert_policy_sample(bool event, bool *primed, bool *active, char *last,
    size_t capacity, const char *state)
{
    if (!state || !state[0] || !strcmp(state, "unknown") || !strcmp(state, "unavailable")) return false;
    bool fire = *primed && (event ? strcmp(state, last) != 0 : !strcmp(state, "on") && strcmp(last, "on"));
    if (!event) *active = !strcmp(state, "on");
    snprintf(last, capacity, "%s", state);
    *primed = true;
    return fire;
}
unsigned alert_policy_connections(uint64_t uptime_ms, bool wifi_expected, bool wifi,
    bool ha_expected, bool ha, bool mqtt_expected, bool mqtt)
{
    if (uptime_ms < 30000) return 0;
    return (wifi_expected && !wifi ? 1U : 0U) |
        (wifi && ha_expected && !ha ? 2U : 0U) |
        (wifi && mqtt_expected && !mqtt ? 4U : 0U);
}
