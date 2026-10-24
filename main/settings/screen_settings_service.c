/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "settings/screen_settings_service.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include "ui/theme/theme_store.h"
#include "net/time_sync.h"
#include "app_events.h"
#include "drivers/display_init.h"
#include "settings/night_mode.h"
#include "ha/ha_client.h"
#include "ui/ui_memory.h"

static atomic_int s_status;
static screen_settings_t s_patch;
static bool s_reload;
static bool s_rebuild;

esp_err_t screen_settings_load(screen_settings_t *out)
{
    runtime_settings_t *settings = ui_calloc_prefer_psram(1, sizeof(*settings));
    if (settings == NULL) return ESP_ERR_NO_MEM;
    esp_err_t err = runtime_settings_load(settings);
    if (err == ESP_OK) {
        screen_settings_read(out, settings);
        theme_entry_t *theme = ui_calloc_prefer_psram(1, sizeof(*theme));
        err = theme != NULL ? theme_store_load_active(theme) : ESP_ERR_NO_MEM;
        if (err == ESP_OK) snprintf(out->theme, sizeof(out->theme), "%s", theme->id);
        if (err == ESP_OK) {
            size_t count = 0;
            err = theme_store_list_custom(out->custom_theme_ids, 16, &count);
            for (size_t i = 0; err == ESP_OK && i < count; i++) {
                err = theme_store_load_custom(out->custom_theme_ids[i], theme);
                if (err == ESP_OK) snprintf(out->custom_theme_names[i], 48, "%s", theme->name);
            }
            out->custom_theme_count = count;
        }
        free(theme);
    }
    free(settings);
    return err;
}

bool screen_settings_submit(const screen_settings_t *patch)
{
    int expected = 0;
    if (!atomic_compare_exchange_strong(&s_status, &expected, 3)) return false;
    s_patch = *patch;
    atomic_store(&s_status, 1);
    return true;
}

int screen_settings_status(bool acknowledge)
{
    int result = atomic_load(&s_status);
    if (acknowledge && (result == 2 || result == -1)) atomic_compare_exchange_strong(&s_status, &result, 0);
    return result == 3 ? 1 : result;
}

static void merge(runtime_settings_t *settings, void *context)
{
    screen_settings_merge(settings, context);
}

void screen_settings_process(void)
{
    if (s_rebuild) {
        app_event_t event = {.type = EV_LAYOUT_UPDATED};
        if (app_events_publish(&event, 0)) s_rebuild = false;
    }
    if (s_reload) {
        app_event_t event = {.type = EV_DISPLAY_PROTECTION_UPDATED};
        if (app_events_publish(&event, 0)) s_reload = false;
    }
    if (atomic_load(&s_status) != 1) return;
    runtime_settings_t *settings = ui_calloc_prefer_psram(1, sizeof(*settings));
    esp_err_t err = settings != NULL ? ESP_OK : ESP_ERR_NO_MEM;
    if (err == ESP_OK && (s_patch.dirty & SYSTEM_DIRTY(SYSTEM_THEME))) {
        theme_entry_t *entry = ui_calloc_prefer_psram(1, sizeof(*entry));
        err = entry != NULL ? theme_palette_resolve_by_id(s_patch.theme, entry) : ESP_ERR_NO_MEM;
        free(entry);
    }
    if (err == ESP_OK) err = runtime_settings_update(merge, &s_patch, settings);
    if (err == ESP_OK && (s_patch.dirty & SYSTEM_DIRTY(SYSTEM_THEME)))
        err = theme_store_set_active_id(s_patch.theme);
    if (err == ESP_OK) {
        if (s_patch.dirty & SYSTEM_DIRTY(SYSTEM_TIMEZONE))
            (void)time_sync_set_timezone(settings->time_tz);
        display_configure_brightness(settings->hardware_active_brightness_percent,
            settings->hardware_dim_brightness_percent, settings->hardware_dim_timeout_seconds);
        night_mode_configure(settings->hardware_night_enabled, settings->hardware_night_start_min,
            settings->hardware_night_end_min, settings->hardware_night_brightness_percent);
        s_reload = true;
        s_rebuild = (s_patch.dirty & (SYSTEM_DIRTY(SYSTEM_LANGUAGE) | SYSTEM_DIRTY(SYSTEM_STYLE) |
            SYSTEM_DIRTY(SYSTEM_THEME))) != 0;
        (void)ha_client_notify_layout_updated();
    }
    free(settings);
    atomic_store(&s_status, err == ESP_OK ? 2 : -1);
}
