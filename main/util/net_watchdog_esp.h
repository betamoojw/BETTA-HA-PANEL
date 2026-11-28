/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Device task and status interface for link and local HTTP monitoring.
 */
#pragma once

#include <stdbool.h>

#include "cJSON.h"

/** @brief Start the watchdog at the end of app_main.
 * @param wifi_configured Whether a network connection is expected.
 * @param ha_expected Whether a Home Assistant connection is configured.
 */
void net_watchdog_start(bool wifi_configured, bool ha_expected);
/** @brief Add the `net_watchdog` diagnostics object to the version response. */
void net_watchdog_add_json(cJSON *root);
