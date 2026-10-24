/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "lvgl.h"

typedef struct {
    bool supported, enabled, paired, listening;
    int volume;
    char state[32], code[24], text[512];
} xiaozhi_status_t;

void xiaozhi_init(void);
void xiaozhi_status(xiaozhi_status_t *out);
/* Commands are serialized by a worker; these never call LVGL or wait on audio. */
esp_err_t xiaozhi_command(const char *action, int value);
esp_err_t xiaozhi_api_register(httpd_handle_t server);
void xiaozhi_ui_create(lv_obj_t *screen);
