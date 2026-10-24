/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Inactivity protection uses one LVGL overlay. The weather view moves while
 * visible; once the backlight is off, small random patches exercise LCD
 * pixels without allocating another frame buffer or keeping a static page.
 */
#include "ui/ui_display_protection.h"
#include "util/user_activity.h"
#include "ui/ui_analog_clock.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "drivers/display_init.h"
#include "drivers/display_policy.h"
#include "ha/ha_model.h"
#include "settings/night_mode.h"
#include "settings/runtime_settings.h"
#include "ui/theme/ui_typography.h"
#include "ui/fonts/mdi_font_registry.h"
#include "ui/ui_i18n.h"
#include "ui/ui_pages.h"
#include "ui/ui_runtime.h"
#include "ui/ui_memory.h"

typedef enum { SAVER_ACTIVE, SAVER_WEATHER, SAVER_PAGES, SAVER_OFF, SAVER_TEST, SAVER_CLOCK } saver_state_t;

typedef struct {
    char hardware_saver_mode[12];
    display_clock_t clock;
    char hardware_saver_weather_entity_id[APP_MAX_ENTITY_ID_LEN];
    int hardware_saver_timeout_minutes;
    int hardware_saver_weather_minutes;
    int hardware_saver_pages_minutes;
    int hardware_saver_page_interval_minutes;
    int hardware_saver_weather_brightness_percent;
    int hardware_dim_brightness_percent;
    bool hardware_saver_page_wake_home;
    bool hardware_saver_pixel_refresh;
} saver_settings_t;

static saver_settings_t s_settings;
static saver_state_t s_state;
static lv_obj_t *s_overlay;
static lv_obj_t *s_weather_group;
static lv_obj_t *s_icon;
static lv_obj_t *s_temperature;
static lv_obj_t *s_condition;
static lv_obj_t *s_clock_group, *s_clock_time, *s_clock_date, *s_clock_dial;
static uint32_t s_last_clock_ms;
static lv_obj_t *s_snow[4];
static ha_state_t *s_weather_state; /* large HA attributes prefer PSRAM */
static uint32_t s_last_activity_ms;
static uint32_t s_handled_activity_ms;
static uint32_t s_last_weather_ms;
static uint32_t s_last_motion_ms;
static uint32_t s_last_snow_ms;
static uint32_t s_last_page_ms;
static uint32_t s_motion_step;
static bool s_ready;
static bool s_overlay_active;
static bool s_touch_pending;
static bool s_pages_cycled;
static display_test_t s_brightness_test;
static int s_test_kind;
static int s_test_percent = -1; /* atomically published fixed backlight, including 0 = off */
static lv_timer_t *s_test_timer;

static void create_overlay(void);

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

bool ui_display_protection_note_activity(void)
{
    __atomic_store_n(&s_last_activity_ms, now_ms(), __ATOMIC_RELAXED);
    user_activity_note((uint32_t)(esp_timer_get_time() / 1000000)); /* the automatic update waits for a quiet panel */
    return __atomic_load_n(&s_overlay_active, __ATOMIC_RELAXED);
}

bool ui_display_protection_is_active(void)
{
    return __atomic_load_n(&s_overlay_active, __ATOMIC_RELAXED);
}

int ui_display_brightness_test_percent(void)
{
    return __atomic_load_n(&s_test_percent, __ATOMIC_RELAXED);
}

int ui_display_brightness_test_remaining_seconds(void)
{
    return display_test_remaining_seconds(&s_brightness_test, now_ms());
}

const char *ui_display_brightness_test_kind(void)
{
    return s_test_kind == 1 ? "dim" : (s_test_kind == 2 ? "night" : "active");
}

bool ui_display_protection_is_off(void)
{
    return __atomic_load_n(&s_overlay_active, __ATOMIC_RELAXED) &&
        __atomic_load_n(&s_state, __ATOMIC_RELAXED) == SAVER_OFF;
}

bool ui_display_protection_is_pages(void)
{
    return __atomic_load_n(&s_overlay_active, __ATOMIC_RELAXED) &&
        __atomic_load_n(&s_state, __ATOMIC_RELAXED) == SAVER_PAGES;
}

static bool utf8_from_codepoint(uint32_t cp, char out[5])
{
    if (cp < 0x10000U || cp > 0x10FFFFU) return false;
    out[0] = (char)(0xF0U | (cp >> 18));
    out[1] = (char)(0x80U | ((cp >> 12) & 0x3FU));
    out[2] = (char)(0x80U | ((cp >> 6) & 0x3FU));
    out[3] = (char)(0x80U | (cp & 0x3FU));
    out[4] = '\0';
    return true;
}

static void saver_position(lv_obj_t *group)
{
    if (group == NULL) return;
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) return;
    int32_t width = lv_display_get_horizontal_resolution(display);
    int32_t height = lv_display_get_vertical_resolution(display);
    int32_t group_w = lv_obj_get_width(group);
    int32_t group_h = lv_obj_get_height(group);
    int32_t margin = 20;
    int32_t max_x = width - group_w - margin;
    int32_t max_y = height - group_h - margin;
    if (max_x < margin) max_x = margin;
    if (max_y < margin) max_y = margin;
    static const uint8_t positions[8][2] = {
        {0, 0}, {100, 100}, {0, 100}, {100, 0},
        {50, 50}, {25, 75}, {75, 25}, {50, 0},
    };
    size_t index = s_motion_step++ % 8U;
    lv_obj_set_pos(group,
        margin + (max_x - margin) * positions[index][0] / 100,
        margin + (max_y - margin) * positions[index][1] / 100);
}

static bool weather_update(void)
{
    if (s_settings.hardware_saver_weather_entity_id[0] == '\0') return false;
    if (s_weather_state == NULL) s_weather_state = ui_calloc_prefer_psram(1, sizeof(*s_weather_state));
    if (s_weather_state == NULL ||
        !ha_model_get_state(s_settings.hardware_saver_weather_entity_id, s_weather_state) ||
        strcmp(s_weather_state->state, "unavailable") == 0 ||
        strcmp(s_weather_state->state, "unknown") == 0) return false;

    cJSON *attrs = cJSON_Parse(s_weather_state->attributes_json);
    cJSON *item = attrs != NULL ? cJSON_GetObjectItemCaseSensitive(attrs, "temperature") : NULL;
    if (!cJSON_IsNumber(item)) {
        if (attrs != NULL) cJSON_Delete(attrs);
        return false;
    }
    if (!isfinite(item->valuedouble)) {
        cJSON_Delete(attrs);
        return false;
    }
    cJSON *unit_item = cJSON_GetObjectItemCaseSensitive(attrs, "temperature_unit");
    const char *unit = cJSON_IsString(unit_item) && unit_item->valuestring != NULL &&
        strlen(unit_item->valuestring) <= 4 ? unit_item->valuestring : "°C";
    char text[32];
    snprintf(text, sizeof(text), "%.0f%s", item->valuedouble, unit);
    lv_label_set_text(s_temperature, text);
    char key[APP_MAX_STATE_LEN];
    size_t i = 0;
    for (; s_weather_state->state[i] != '\0' && i + 1 < sizeof(key); i++) {
        char c = s_weather_state->state[i];
        if (c == '_' || c == ' ') c = '-';
        if (c >= 'A' && c <= 'Z') c = (char)(c + 'a' - 'A');
        key[i] = c;
    }
    key[i] = '\0';
    if (strcmp(key, "partly-cloudy") == 0) strcpy(key, "partlycloudy");
    if (strcmp(key, "clear-day") == 0) strcpy(key, "sunny");
    char condition_key[64] = "weather.condition.";
    size_t prefix_len = strlen(condition_key);
    for (i = 0; key[i] != '\0' && prefix_len + i + 1 < sizeof(condition_key); i++)
        condition_key[prefix_len + i] = key[i] == '-' ? '_' : key[i];
    condition_key[prefix_len + i] = '\0';
    lv_label_set_text(s_condition, ui_i18n_get(condition_key, s_weather_state->state));
    char icon[5] = {0};
    uint32_t cp = ui_runtime_weather_icon_codepoint_for_key(key);
    lv_label_set_text(s_icon, utf8_from_codepoint(cp, icon) ? icon : "");
    cJSON_Delete(attrs);
    return true;
}

static void hide_overlay(void)
{
    if (!s_ready || s_state == SAVER_ACTIVE) return;
    display_test_stop(&s_brightness_test);
    if (s_test_timer != NULL) {
        lv_timer_delete(s_test_timer);
        s_test_timer = NULL;
    }
    if (s_pages_cycled && s_settings.hardware_saver_page_wake_home && ui_pages_count() > 0)
        (void)ui_pages_show_index(0);
    s_pages_cycled = false;
    for (size_t i = 0; i < 4; i++) lv_obj_set_hidden(s_snow[i], true);
    lv_obj_set_hidden(s_overlay, true);
    /* Present the dashboard while the backlight is still low, so waking
     * cannot briefly expose the previous black or weather frame at full brightness. */
    lv_display_t *display = lv_display_get_default();
    if (display != NULL) lv_refr_now(display);
    __atomic_store_n(&s_test_percent, -1, __ATOMIC_RELAXED);
    __atomic_store_n(&s_state, SAVER_ACTIVE, __ATOMIC_RELAXED);
    __atomic_store_n(&s_overlay_active, false, __ATOMIC_RELAXED);
    s_handled_activity_ms = __atomic_load_n(&s_last_activity_ms, __ATOMIC_RELAXED);
    display_note_activity();
}

static void overlay_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        s_touch_pending = true;
        (void)ui_display_protection_note_activity();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_touch_pending = false;
        hide_overlay();
    }
}

static void clock_layout(void)
{
    lv_display_t *display = lv_display_get_default();
    int width = lv_display_get_horizontal_resolution(display);
    int height = lv_display_get_vertical_resolution(display);
    bool analog = s_settings.clock.analog && s_clock_dial;
    int diameter = LV_MIN(400, LV_MIN(width, height) * 2 / 3);
    int date_height = s_settings.clock.show_date ?
        lv_font_get_line_height(lv_obj_get_style_text_font(s_clock_date, 0)) + 16 : 0;
    lv_obj_set_size(s_clock_group, analog ? diameter : width - 60,
                    analog ? diameter + date_height : 170);
    lv_obj_set_hidden(s_clock_time, analog);
    if (s_clock_dial) {
        lv_obj_set_hidden(s_clock_dial, !analog);
        lv_obj_set_size(s_clock_dial, diameter, diameter);
        lv_obj_align(s_clock_dial, LV_ALIGN_TOP_MID, 0, 0);
    }
    lv_obj_set_hidden(s_clock_date, !s_settings.clock.show_date);
    lv_obj_align(s_clock_date, LV_ALIGN_BOTTOM_MID, 0, 0);
    s_last_clock_ms = now_ms() - 1000U;
    lv_obj_update_layout(s_clock_group);
}

static void enter_state(saver_state_t next)
{
    if (next == s_state) return;
    __atomic_store_n(&s_state, next, __ATOMIC_RELAXED);
    __atomic_store_n(&s_overlay_active, true, __ATOMIC_RELAXED);
    lv_obj_set_hidden(s_overlay, false);
    lv_obj_move_foreground(s_overlay);
    lv_obj_set_style_bg_opa(s_overlay, next == SAVER_PAGES || next == SAVER_TEST ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_hidden(s_clock_group, next != SAVER_CLOCK);
    if (next == SAVER_WEATHER) {
        for (size_t i = 0; i < 4; i++) lv_obj_set_hidden(s_snow[i], true);
        lv_obj_set_hidden(s_weather_group, false);
        saver_position(s_weather_group);
        display_set_brightness_percent(s_settings.hardware_saver_weather_brightness_percent);
    } else if (next == SAVER_CLOCK) {
        clock_layout();
        lv_obj_set_hidden(s_weather_group, true);
        for (size_t i = 0; i < 4; i++) lv_obj_set_hidden(s_snow[i], true);
        saver_position(s_clock_group);
        display_set_brightness_percent(s_settings.clock.brightness);
    } else if (next == SAVER_TEST) {
        lv_obj_set_hidden(s_weather_group, true);
        for (size_t i = 0; i < 4; i++) lv_obj_set_hidden(s_snow[i], true);
    } else if (next == SAVER_PAGES) {
        s_pages_cycled = true;
        lv_obj_set_hidden(s_weather_group, true);
        for (size_t i = 0; i < 4; i++) lv_obj_set_hidden(s_snow[i], true);
        s_last_page_ms = now_ms();
        display_set_brightness_percent(s_settings.hardware_dim_brightness_percent);
    } else {
        lv_obj_set_hidden(s_weather_group, true);
        display_set_brightness_percent(0);
        s_last_snow_ms = 0;
    }
}

static void brightness_test_tick(lv_timer_t *timer)
{
    (void)timer;
    if (!ui_display_brightness_test_remaining_seconds() && !s_touch_pending) hide_overlay();
}

bool ui_display_brightness_test_start(int percent, int kind)
{
    if (!s_ready || !display_is_ready() || s_touch_pending) return false;
    if (s_state != SAVER_ACTIVE) hide_overlay();
    create_overlay();
    if (s_overlay == NULL) return false;
    s_test_timer = lv_timer_create(brightness_test_tick, 100, NULL);
    if (s_test_timer == NULL) return false;
    display_test_start(&s_brightness_test, now_ms(), percent);
    s_test_kind = kind;
    __atomic_store_n(&s_test_percent, s_brightness_test.percent, __ATOMIC_RELAXED);
    enter_state(SAVER_TEST);
    s_handled_activity_ms = __atomic_load_n(&s_last_activity_ms, __ATOMIC_RELAXED);
    (void)display_set_brightness_percent(s_brightness_test.percent);
    return true;
}

bool ui_display_brightness_test_update(int percent)
{
    if (!display_test_update(&s_brightness_test, now_ms(), percent)) return false;
    __atomic_store_n(&s_test_percent, s_brightness_test.percent, __ATOMIC_RELAXED);
    (void)display_set_brightness_percent(s_brightness_test.percent);
    return true;
}

void ui_display_brightness_test_stop(void)
{
    if (s_state == SAVER_TEST) {
        display_test_stop(&s_brightness_test);
        if (!s_touch_pending) hide_overlay();
    }
}

static void snow_update(uint32_t now)
{
    if (!s_settings.hardware_saver_pixel_refresh || now - s_last_snow_ms < 5000U) return;
    s_last_snow_ms = now;
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) return;
    uint32_t width = (uint32_t)lv_display_get_horizontal_resolution(display);
    uint32_t height = (uint32_t)lv_display_get_vertical_resolution(display);
    if (width < 32U || height < 32U) return;
    for (size_t i = 0; i < 4; i++) {
        lv_obj_set_pos(s_snow[i], (int32_t)(esp_random() % (width - 24U)),
            (int32_t)(esp_random() % (height - 24U)));
        lv_obj_set_style_bg_color(s_snow[i], lv_color_hex(esp_random() & 0xFFFFFFU), 0);
        lv_obj_set_hidden(s_snow[i], false);
    }
}

bool ui_display_protection_reload(void)
{
    runtime_settings_t *settings = ui_calloc_prefer_psram(1, sizeof(*settings));
    if (settings == NULL) {
        return false;
    }
    if (runtime_settings_load(settings) != ESP_OK) { free(settings); return false; }
    strlcpy(s_settings.hardware_saver_mode, settings->hardware_saver_mode, sizeof(s_settings.hardware_saver_mode));
    strlcpy(s_settings.hardware_saver_weather_entity_id, settings->hardware_saver_weather_entity_id,
        sizeof(s_settings.hardware_saver_weather_entity_id));
    s_settings.hardware_saver_timeout_minutes = settings->hardware_saver_timeout_minutes;
    s_settings.clock = settings->display_clock;
    ui_pages_set_clock_24h(settings->display_clock.format_24h);
    s_settings.hardware_saver_weather_minutes = settings->hardware_saver_weather_minutes;
    s_settings.hardware_saver_pages_minutes = settings->hardware_saver_pages_minutes;
    s_settings.hardware_saver_page_interval_minutes = settings->hardware_saver_page_interval_minutes;
    s_settings.hardware_saver_weather_brightness_percent = settings->hardware_saver_weather_brightness_percent;
    s_settings.hardware_dim_brightness_percent = settings->hardware_dim_brightness_percent;
    s_settings.hardware_saver_page_wake_home = settings->hardware_saver_page_wake_home;
    s_settings.hardware_saver_pixel_refresh = settings->hardware_saver_pixel_refresh;
    free(settings);
    if (strcmp(s_settings.hardware_saver_mode, "disabled") != 0 && s_overlay == NULL) create_overlay();
    if (s_state != SAVER_ACTIVE && s_state != SAVER_TEST) hide_overlay();
    __atomic_store_n(&s_last_activity_ms, now_ms(), __ATOMIC_RELAXED);
    s_handled_activity_ms = __atomic_load_n(&s_last_activity_ms, __ATOMIC_RELAXED);
    return true;
}

void ui_display_protection_poll(void)
{
    if (!s_ready || s_overlay == NULL) return;
    uint32_t now = now_ms();
    if (s_state == SAVER_TEST) {
        /* Touch release consumes the whole gesture before the dashboard becomes clickable. */
        if (!ui_display_brightness_test_remaining_seconds() && !s_touch_pending) hide_overlay();
        return;
    }
    uint32_t activity = __atomic_load_n(&s_last_activity_ms, __ATOMIC_RELAXED);
    if (activity != s_handled_activity_ms) {
        s_handled_activity_ms = activity;
        if (s_state != SAVER_ACTIVE && !s_touch_pending) hide_overlay();
    }
    if (strcmp(s_settings.hardware_saver_mode, "disabled") == 0) return;

    uint32_t idle_ms = now - activity;
    uint32_t start_ms = (uint32_t)s_settings.hardware_saver_timeout_minutes * 60000U;
    if (strcmp(s_settings.hardware_saver_mode, "clock") == 0) start_ms = (uint32_t)s_settings.clock.start_seconds * 1000U;
    bool night_off = night_mode_active() && night_mode_brightness_percent() == 0 &&
        display_get_brightness_percent() == 0;
    if (idle_ms < start_ms && !night_off) return;

    saver_state_t target = SAVER_OFF;
    if (!night_mode_active() && strcmp(s_settings.hardware_saver_mode, "clock") == 0 &&
        !display_clock_is_off(&s_settings.clock, idle_ms)) {
        target = SAVER_CLOCK;
    } else if (!night_mode_active() && strcmp(s_settings.hardware_saver_mode, "weather") == 0 &&
        idle_ms < start_ms + (uint32_t)s_settings.hardware_saver_weather_minutes * 60000U) {
        if (now - s_last_weather_ms >= 30000U || s_state != SAVER_WEATHER) {
            s_last_weather_ms = now;
            if (weather_update()) target = SAVER_WEATHER;
        } else {
            target = SAVER_WEATHER;
        }
    } else if (!night_mode_active() && strcmp(s_settings.hardware_saver_mode, "pages") == 0 &&
        idle_ms < start_ms + (uint32_t)s_settings.hardware_saver_pages_minutes * 60000U) {
        target = SAVER_PAGES;
    }
    enter_state(target);
    if ((s_state == SAVER_WEATHER || s_state == SAVER_CLOCK) && now - s_last_motion_ms >= 60000U) {
        saver_position(s_state == SAVER_CLOCK ? s_clock_group : s_weather_group);
        s_last_motion_ms = now;
    } else if (s_state == SAVER_OFF) {
        snow_update(now);
    } else if (s_state == SAVER_PAGES &&
        now - s_last_page_ms >= (uint32_t)s_settings.hardware_saver_page_interval_minutes * 60000U) {
        s_last_page_ms = now;
        if (ui_pages_count() > 1) (void)ui_pages_next();
    }
    if (s_state == SAVER_CLOCK && now - s_last_clock_ms >= 1000U) {
        s_last_clock_ms = now;
        time_t timestamp = time(NULL);
        struct tm local;
        char text[32], date[32];
        const struct tm *local_time = localtime_r(&timestamp, &local);
        display_clock_format(&s_settings.clock, local_time, text, sizeof(text), date, sizeof(date));
        lv_label_set_text(s_clock_time, text);
        lv_label_set_text(s_clock_date, date);
        if (s_settings.clock.analog) ui_analog_clock_update(s_clock_dial, local_time, s_settings.clock.show_seconds);
    }
}

static void create_overlay(void)
{
    if (s_overlay != NULL) return;
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) return;
    int32_t width = lv_display_get_horizontal_resolution(display);
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_set_scrollable(s_overlay, false);
    lv_obj_set_clickable(s_overlay, true);
    lv_obj_add_event_cb(s_overlay, overlay_event, LV_EVENT_ALL, NULL);

    s_weather_group = lv_obj_create(s_overlay);
    int32_t group_width = width < 320 ? width - 40 : 280;
    lv_obj_set_size(s_weather_group, group_width, 170);
    lv_obj_set_style_bg_opa(s_weather_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_weather_group, 0, 0);
    lv_obj_set_style_pad_all(s_weather_group, 0, 0);
    lv_obj_set_clickable(s_weather_group, false);
    lv_obj_set_scrollable(s_weather_group, false);
    s_icon = lv_label_create(s_weather_group);
    const lv_font_t *font = mdi_font_weather();
    if (font != NULL) lv_obj_set_style_text_font(s_icon, font, 0);
    lv_obj_set_style_text_color(s_icon, lv_color_hex(0xC6DFEA), 0);
    lv_obj_set_pos(s_icon, 0, 0);
    s_temperature = lv_label_create(s_weather_group);
    lv_obj_set_style_text_font(s_temperature, ui_font_role(UI_TEXT_KEY), 0);
    lv_obj_set_style_text_color(s_temperature, lv_color_hex(0xF2F7F9), 0);
    lv_obj_set_pos(s_temperature, 100, 12);
    s_condition = lv_label_create(s_weather_group);
    lv_obj_set_style_text_font(s_condition, ui_font_role(UI_TEXT_META), 0);
    lv_obj_set_style_text_color(s_condition, lv_color_hex(0xA6BBC7), 0);
    lv_obj_set_width(s_condition, group_width);
    lv_label_set_long_mode(s_condition, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_condition, 0, 125);
    s_clock_group = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_clock_group);
    lv_obj_set_size(s_clock_group, width - 60, 170);
    lv_obj_set_clickable(s_clock_group, false);
    lv_obj_set_scrollable(s_clock_group, false);
    s_clock_dial = ui_analog_clock_create(s_clock_group);
    if (s_clock_dial) lv_obj_set_hidden(s_clock_dial, true);
    s_clock_time = lv_label_create(s_clock_group);
    lv_label_set_text(s_clock_time, "--:--");
    /* The longest 12-hour form fits compact panels even at the largest text step. */
    lv_obj_set_style_text_font(s_clock_time, ui_text_fit("12:59:59 PM", ui_font_role(UI_TEXT_HERO), width - 60, 90), 0);
    lv_obj_set_style_text_color(s_clock_time, lv_color_white(), 0);
    lv_obj_align(s_clock_time, LV_ALIGN_TOP_MID, 0, 0);
    s_clock_date = lv_label_create(s_clock_group);
    ui_text_set(s_clock_date, UI_TEXT_LABEL, false);
    lv_obj_set_style_text_color(s_clock_date, lv_color_white(), 0);
    lv_obj_align(s_clock_date, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_hidden(s_clock_group, true);
    for (size_t i = 0; i < 4; i++) {
        s_snow[i] = lv_obj_create(s_overlay);
        lv_obj_set_size(s_snow[i], 24, 24);
        lv_obj_set_style_border_width(s_snow[i], 0, 0);
        lv_obj_set_style_radius(s_snow[i], 0, 0);
        lv_obj_set_clickable(s_snow[i], false);
        lv_obj_set_hidden(s_snow[i], true);
    }
    lv_obj_set_hidden(s_overlay, true);
}

bool ui_display_protection_init(void)
{
    if (s_ready) return true;
    s_ready = true;
    __atomic_store_n(&s_state, SAVER_ACTIVE, __ATOMIC_RELAXED);
    return ui_display_protection_reload();
}
