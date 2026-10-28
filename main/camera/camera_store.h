/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "camera/camera_config.h"
#include "esp_err.h"
esp_err_t camera_store_init(void);
bool camera_store_get(camera_settings_t *out);
bool camera_store_local(camera_config_t *out);
bool camera_store_source(const char *id, camera_source_t *out);
esp_err_t camera_store_update(const cJSON *root);
uint32_t camera_store_generation(void);
/* Non-blocking display-side privacy gate; a busy store is treated as disabled. */
bool camera_store_output_allowed(const char *id);
