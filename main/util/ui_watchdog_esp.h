/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Device-side LVGL and runtime heartbeat monitoring.
 */
#pragma once

#include "cJSON.h"

/** @brief Start the watchdog task after the UI runtime has started. */
void ui_watchdog_start(void);
/** @brief Record a UI runtime heartbeat by storing a timestamp; callable from any task. */
void ui_watchdog_beat_runtime(void);
/** @brief Start the heartbeat timer in LVGL.
 * @pre The caller holds the display lock.
 */
void ui_watchdog_attach_lvgl(void);
/** @brief Add the `ui_watchdog` diagnostics object to the version response. */
void ui_watchdog_add_json(cJSON *root);
