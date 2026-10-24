/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "settings/screen_settings_model.h"

esp_err_t screen_settings_load(screen_settings_t *out);
bool screen_settings_submit(const screen_settings_t *patch);
/* 0 idle, 1 pending, 2 saved, -1 failed. Acknowledge consumes a completed result. */
int screen_settings_status(bool acknowledge);
/* UI task only, outside the display lock; storage and hardware configuration live here. */
void screen_settings_process(void);
