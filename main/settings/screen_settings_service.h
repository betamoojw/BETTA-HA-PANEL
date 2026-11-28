/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Native settings snapshot and single-slot asynchronous save service.
 */
#pragma once
#include "settings/screen_settings_model.h"

/** @brief Read the current settings and theme options for the screen settings view. */
esp_err_t screen_settings_load(screen_settings_t *out);
/** @brief Copy a patch into the free mailbox slot without saving it in the caller.
 * @return False while a save is pending or a previous result is unacknowledged.
 */
bool screen_settings_submit(const screen_settings_t *patch);
/** @return Zero for idle, one for pending, two for saved, or -1 for failed.
 * Acknowledging a completed result frees the slot for the next submission.
 */
int screen_settings_status(bool acknowledge);
/** @brief Process a pending save or retry its deferred reload notifications.
 * @pre Call from the UI task without holding the display lock.
 */
void screen_settings_process(void);
