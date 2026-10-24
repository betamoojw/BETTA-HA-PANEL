/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "settings/runtime_settings.h"

/* Only fields exposed on the panel, never network credentials or other settings. */
typedef enum {
    SCREEN_ACTIVE, SCREEN_DIM, SCREEN_DIM_SECONDS, SCREEN_MODE, SCREEN_START_MINUTES,
    SCREEN_CLOCK_START, SCREEN_CLOCK_BRIGHTNESS, SCREEN_CLOCK_OFF, SCREEN_CLOCK_OFF_SECONDS,
    SCREEN_CLOCK_24H, SCREEN_CLOCK_SECONDS, SCREEN_CLOCK_DATE, SCREEN_CLOCK_ANALOG, SCREEN_ROTATION, SCREEN_AUTO_ROTATION, SCREEN_FIELD_COUNT
} screen_field_t;
enum { SYSTEM_LANGUAGE, SYSTEM_TIMEZONE, SYSTEM_STYLE, SYSTEM_THEME, SYSTEM_FIELD_COUNT };
#define SYSTEM_DIRTY(field) (1U << (16 + (field)))
typedef struct {
    int value[SCREEN_FIELD_COUNT]; uint32_t dirty;
    char language[APP_UI_LANGUAGE_MAX_LEN], timezone[APP_TIME_TZ_MAX_LEN], style[12];
    char theme[32];
    unsigned custom_theme_count;
    char custom_theme_ids[16][32], custom_theme_names[16][48];
} screen_settings_t;
const char *screen_settings_mode(int index);
void screen_settings_read(screen_settings_t *out, const runtime_settings_t *settings);
void screen_settings_merge(runtime_settings_t *settings, const screen_settings_t *patch);
