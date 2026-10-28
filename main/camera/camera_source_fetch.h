/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>
/* Worker-only; id is "local" or a saved source id. No URLs/credentials in
 * layout JSON, no HTTP from the LVGL task, no arbitrary request target. */
esp_err_t camera_source_fetch(const char *id, uint8_t **jpeg, size_t *length);
int camera_source_refresh_ms(const char *id);
