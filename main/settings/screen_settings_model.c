/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "settings/screen_settings_model.h"
#include "settings/brightness_model.h"
#include <string.h>
#include <stdio.h>

static const char *const modes[] = {"disabled", "clock", "off", "weather", "pages"};
const char *screen_settings_mode(int index) { return modes[index >= 0 && index < 5 ? index : 0]; }

void screen_settings_read(screen_settings_t *out, const runtime_settings_t *settings)
{
    const display_clock_t *clock = &settings->display_clock;
    *out = (screen_settings_t){.value = {settings->hardware_active_brightness_percent,
        settings->hardware_dim_brightness_percent, settings->hardware_dim_timeout_seconds, 0,
        settings->hardware_saver_timeout_minutes, clock->start_seconds, clock->brightness,
        clock->off_enabled, clock->off_seconds, clock->format_24h, clock->show_seconds, clock->show_date, clock->analog}};
    for (int i = 0; i < 5; i++) if (strcmp(settings->hardware_saver_mode, modes[i]) == 0) out->value[SCREEN_MODE] = i;
    snprintf(out->language, sizeof(out->language), "%s", settings->ui_language);
    snprintf(out->timezone, sizeof(out->timezone), "%s", settings->time_tz);
    snprintf(out->style, sizeof(out->style), "%s", settings->ui_style);
}

void screen_settings_merge(runtime_settings_t *settings, const screen_settings_t *patch)
{
    if (patch->dirty & SYSTEM_DIRTY(SYSTEM_LANGUAGE))
        snprintf(settings->ui_language, sizeof(settings->ui_language), "%s", patch->language);
    if (patch->dirty & SYSTEM_DIRTY(SYSTEM_TIMEZONE))
        snprintf(settings->time_tz, sizeof(settings->time_tz), "%s", patch->timezone);
    if (patch->dirty & SYSTEM_DIRTY(SYSTEM_STYLE))
        snprintf(settings->ui_style, sizeof(settings->ui_style), "%s", patch->style);
    screen_settings_t merged;
    screen_settings_read(&merged, settings);
    static const int low[] = {1, 0, 5, 0, 1, 15, 1, 0, 30, 0, 0, 0, 0};
    static const int high[] = {100, 100, 3600, 4, 120, 7200, 100, 1, 7200, 1, 1, 1, 1};
    for (int i = 0; i < SCREEN_FIELD_COUNT; i++) if (patch->dirty & (1U << i)) {
        int v = patch->value[i];
        merged.value[i] = v < low[i] ? low[i] : (v > high[i] ? high[i] : v);
    }
    const int *v = merged.value;
    settings->hardware_active_brightness_percent = v[SCREEN_ACTIVE];
    settings->hardware_dim_brightness_percent = v[SCREEN_DIM];
    settings->hardware_dim_timeout_seconds = v[SCREEN_DIM_SECONDS];
    snprintf(settings->hardware_saver_mode, sizeof(settings->hardware_saver_mode), "%s", screen_settings_mode(v[SCREEN_MODE]));
    settings->hardware_saver_timeout_minutes = v[SCREEN_START_MINUTES];
    settings->display_clock = (display_clock_t){v[SCREEN_CLOCK_START], v[SCREEN_CLOCK_BRIGHTNESS],
        v[SCREEN_CLOCK_OFF], v[SCREEN_CLOCK_OFF_SECONDS], v[SCREEN_CLOCK_24H], v[SCREEN_CLOCK_SECONDS], v[SCREEN_CLOCK_DATE], v[SCREEN_CLOCK_ANALOG]};
    brightness_levels_t levels = {settings->hardware_active_brightness_percent,
        settings->hardware_dim_brightness_percent, settings->hardware_night_brightness_percent};
    brightness_levels_normalize(&levels);
    settings->hardware_active_brightness_percent = levels.active;
    settings->hardware_dim_brightness_percent = levels.dimmed;
    settings->hardware_night_brightness_percent = levels.night;
}
