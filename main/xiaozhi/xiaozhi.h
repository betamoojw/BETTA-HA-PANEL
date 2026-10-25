/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "lvgl.h"

typedef struct {
    bool supported, enabled, paired, listening, wake_enabled, wake_ready, wake_running, conversation;
    int volume, dialog_timeout, emoji_style; /* 0 Otto (default), 1 classic */
    uint32_t dialog_seq, activity_seq;
    uint32_t wake_frames, wake_hits;
    int wake_mic_peak, wake_ref_peak, wake_detect_us; /* Diagnostics, no audio retained. */
    char state[32], code[24], text[512], recognized[512], reply[512], emotion[24];
} xiaozhi_status_t;

void xiaozhi_init(void);
void xiaozhi_status(xiaozhi_status_t *out);
/* Commands are serialized by a worker; these never call LVGL or wait on audio. */
esp_err_t xiaozhi_command(const char *action, int value);
/* Storage operation: only HTTP/settings workers, never under the display lock. */
esp_err_t xiaozhi_set_emoji_style(int value);
esp_err_t xiaozhi_api_register(httpd_handle_t server);
void xiaozhi_ui_create(lv_obj_t *screen);
