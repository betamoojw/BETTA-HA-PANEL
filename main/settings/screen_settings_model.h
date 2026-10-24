/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "settings/runtime_settings.h"

/* Only fields exposed on the panel, never network credentials or other settings. */
typedef enum {
    SCREEN_ACTIVE, SCREEN_DIM, SCREEN_DIM_SECONDS, SCREEN_MODE, SCREEN_START_MINUTES,
    SCREEN_CLOCK_START, SCREEN_CLOCK_BRIGHTNESS, SCREEN_CLOCK_OFF, SCREEN_CLOCK_OFF_SECONDS,
    SCREEN_CLOCK_24H, SCREEN_CLOCK_SECONDS, SCREEN_CLOCK_DATE, SCREEN_FIELD_COUNT
} screen_field_t;
typedef struct { int value[SCREEN_FIELD_COUNT]; uint32_t dirty; } screen_settings_t;
const char *screen_settings_mode(int index);
void screen_settings_read(screen_settings_t *out, const runtime_settings_t *settings);
void screen_settings_merge(runtime_settings_t *settings, const screen_settings_t *patch);
