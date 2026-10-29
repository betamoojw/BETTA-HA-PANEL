/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Real LVGL events, persistence round trips and narrow/wide view lifetimes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "SDL.h"
#include "settings/screen_settings_service.h"
#include "ui/ui_screen_settings.h"
#include "ui/ui_display_protection.h"
#include "ui/ui_pages.h"
#include "ui/ui_transition.h"
#include "ui/fonts/app_symbols.h"
#include "ui/ui_i18n.h"
#include "ui/theme/theme_palette.h"

static runtime_settings_t settings;
static screen_settings_t pending;
static int status;
static bool fail_save;
static int saved_emoji;
static char saved_theme[32] = "dark_v2";
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "Screen settings line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

esp_err_t screen_settings_load(screen_settings_t *out)
{
    if (!settings.hardware_active_brightness_percent) {
        settings.hardware_active_brightness_percent = 60;
        settings.hardware_dim_brightness_percent = 30;
        settings.hardware_dim_timeout_seconds = 300;
        settings.hardware_saver_timeout_minutes = 10;
        strcpy(settings.hardware_saver_mode, "disabled");
        display_clock_defaults(&settings.display_clock);
        strcpy(settings.ui_language, "en");
        strcpy(settings.ui_style, "glow");
        strcpy(settings.time_tz, "Asia/Shanghai");
    }
    screen_settings_read(out, &settings);
    out->xiaozhi_supported = true; out->value[SCREEN_XIAOZHI_EMOJI] = saved_emoji;
    snprintf(out->theme, sizeof(out->theme), "%s", saved_theme);
    out->custom_theme_count = 1;
    strcpy(out->custom_theme_ids[0], "imported_test");
    strcpy(out->custom_theme_names[0], "Imported\npalette");
    return ESP_OK;
}
bool screen_settings_submit(const screen_settings_t *patch) { pending = *patch; status = 1; return true; }
int screen_settings_status(bool ack) { int out = status; if (ack && status != 1) status = 0; return out; }
void screen_settings_process(void)
{
    if (status != 1) return;
    if (fail_save) { status = -1; return; }
    screen_settings_merge(&settings, &pending);
    if (pending.dirty & (1U << SCREEN_XIAOZHI_EMOJI)) saved_emoji = pending.value[SCREEN_XIAOZHI_EMOJI];
    if (pending.dirty & SYSTEM_DIRTY(SYSTEM_THEME)) snprintf(saved_theme, sizeof(saved_theme), "%s", pending.theme);
    cJSON *json = cJSON_CreateObject();
    CHECK(display_clock_write(json, &settings.display_clock));
    char *wire = cJSON_PrintUnformatted(json);
    cJSON *reloaded = cJSON_Parse(wire);
    display_clock_t clock;
    display_clock_defaults(&clock);
    CHECK(display_clock_parse(cJSON_GetObjectItemCaseSensitive(reloaded, "clock"), &clock));
    CHECK(display_clock_equal(&clock, &settings.display_clock));
    settings.display_clock = clock;
    cJSON_Delete(json); cJSON_Delete(reloaded); free(wire);
    status = 2;
}
bool ui_display_brightness_test_start(int percent, int kind) { (void)kind; return percent >= 1 && percent <= 100; }
void ui_display_brightness_test_stop(void) {}

static lv_obj_t *find_text(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_text(lv_obj_get_child(root, i), text);
        if (found != NULL) return found;
    }
    return NULL;
}

static lv_obj_t *control(int field)
{
    lv_obj_t *body = lv_obj_get_child(ui_screen_settings_root(), 1);
    lv_obj_t *row = lv_obj_get_child(body, field + SYSTEM_FIELD_COUNT);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
        lv_obj_t *child = lv_obj_get_child(row, i);
        if (lv_obj_check_type(child, &lv_slider_class) || lv_obj_check_type(child, &lv_dropdown_class) ||
            lv_obj_check_type(child, &lv_switch_class)) return child;
    }
    CHECK(false); return NULL;
}

static void set_mode(int mode)
{
    lv_obj_t *object = control(SCREEN_MODE);
    lv_dropdown_set_selected(object, mode);
    lv_obj_send_event(object, LV_EVENT_VALUE_CHANGED, NULL);
}

static void click_save(void)
{
    lv_obj_t *caption = find_text(ui_screen_settings_root(), ui_i18n_get("screen.save", "Save"));
    CHECK(caption != NULL);
    lv_obj_send_event(lv_obj_get_parent(caption), LV_EVENT_CLICKED, NULL);
    screen_settings_process();
    SDL_Delay(110); lv_timer_handler();
}

static void select_system(int field, unsigned index)
{
    lv_obj_t *body = lv_obj_get_child(ui_screen_settings_root(), 1);
    lv_obj_t *row = lv_obj_get_child(body, field);
    lv_obj_t *object = lv_obj_get_child(row, 1);
    CHECK(lv_obj_check_type(object, &lv_dropdown_class));
    lv_dropdown_set_selected(object, index);
    lv_obj_send_event(object, LV_EVENT_VALUE_CHANGED, NULL);
}

static void check_system_settings(void)
{
    ui_screen_settings_open();
    lv_obj_t *cancel_emoji = control(SCREEN_XIAOZHI_EMOJI);
    lv_dropdown_set_selected(cancel_emoji, 1);
    lv_obj_send_event(cancel_emoji, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *cancel = find_text(ui_screen_settings_root(), ui_i18n_get("screen.cancel", "Cancel"));
    lv_obj_send_event(lv_obj_get_parent(cancel), LV_EVENT_CLICKED, NULL);
    CHECK(saved_emoji == 0 && ui_screen_settings_root() == NULL);
    for (int style = 1; style >= 0; style--) {
        ui_screen_settings_open();
        lv_obj_t *emoji = control(SCREEN_XIAOZHI_EMOJI);
        CHECK(lv_obj_check_type(emoji, &lv_dropdown_class));
        lv_dropdown_set_selected(emoji, style); lv_obj_send_event(emoji, LV_EVENT_VALUE_CHANGED, NULL);
        fail_save = true; click_save(); CHECK(ui_screen_settings_root() != NULL && saved_emoji != style);
        fail_save = false; click_save(); CHECK(saved_emoji == style);
        ui_screen_settings_open(); CHECK(lv_dropdown_get_selected(control(SCREEN_XIAOZHI_EMOJI)) == (unsigned)style);
        click_save();
    }
    ui_screen_settings_open();
    for (int field = 0; field < SYSTEM_FIELD_COUNT; field++) {
        lv_obj_t *row = lv_obj_get_child(lv_obj_get_child(ui_screen_settings_root(), 1), field);
        lv_obj_t *object = lv_obj_get_child(row, 1);
        lv_obj_scroll_to_view(row, LV_ANIM_OFF);
        lv_dropdown_open(object); lv_obj_update_layout(ui_screen_settings_root());
        lv_area_t bounds; lv_obj_get_coords(lv_dropdown_get_list(object), &bounds);
        CHECK(bounds.x1 >= 0 && bounds.y1 >= 0);
        CHECK(bounds.x2 < lv_display_get_horizontal_resolution(lv_display_get_default()));
        CHECK(bounds.y2 < lv_display_get_vertical_resolution(lv_display_get_default()));
        lv_dropdown_close(object);
    }
    select_system(SYSTEM_ANIMATIONS, 1); /* enabled */
    select_system(SYSTEM_LANGUAGE, 6); /* Simplified Chinese */
    select_system(SYSTEM_TIMEZONE, 1); /* First shared catalog entry */
    select_system(SYSTEM_STYLE, 4); /* Matte */
    select_system(SYSTEM_THEME, 3); /* Light */
    strcpy(settings.ha_ws_url, "ws://example.invalid/concurrent");
    fail_save = true; click_save();
    CHECK(ui_screen_settings_root() != NULL && !strcmp(settings.ui_language, "en"));
    fail_save = false; click_save();
    CHECK(ui_screen_settings_root() == NULL);
    CHECK(settings.ui_page_animations);
    CHECK(!strcmp(settings.ui_language, "zh-cn") && !strcmp(settings.ui_style, "matte"));
    CHECK(strcmp(settings.time_tz, "Asia/Shanghai") != 0);
    CHECK(!strcmp(settings.ha_ws_url, "ws://example.invalid/concurrent"));
    CHECK(strcmp(saved_theme, "dark_v2") != 0);
    ui_screen_settings_open();
    select_system(SYSTEM_ANIMATIONS, 2); /* disabled */
    select_system(SYSTEM_THEME, 7); /* Imported palette after six built-ins */
    select_system(SYSTEM_LANGUAGE, 1);
    select_system(SYSTEM_LANGUAGE, 0); /* Return to preserved saved value */
    click_save(); CHECK(!strcmp(settings.ui_language, "zh-cn"));
    CHECK(!strcmp(saved_theme, "imported_test") && !settings.ui_page_animations);
    ui_screen_settings_open(); select_system(SYSTEM_LANGUAGE, 1);
    lv_obj_t *caption = find_text(ui_screen_settings_root(), ui_i18n_get("screen.cancel", "Cancel"));
    lv_obj_send_event(lv_obj_get_parent(caption), LV_EVENT_CLICKED, NULL);
    CHECK(!strcmp(settings.ui_language, "zh-cn"));
    strcpy(settings.ui_language, "en");
}

static void check_bounds(lv_obj_t *object, lv_obj_t *viewport)
{
    if (lv_obj_is_hidden(object)) return;
    lv_area_t a, b; lv_obj_get_coords(object, &a); lv_obj_get_coords(viewport, &b);
    CHECK(a.x1 >= b.x1 && a.x2 <= b.x2);
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); i++) check_bounds(lv_obj_get_child(object, i), viewport);
}

int sim_screen_settings_regression(void)
{
    extern bool g_sim_mqtt;
    for (int state = 0; state < 8; state++) {
        g_sim_mqtt = (state & 1) != 0;
        ui_pages_init();
        ui_pages_set_topbar_status(true, (state & 2) != 0, true, true);
        ui_pages_set_alert_warning((state & 4) != 0);
        lv_obj_update_layout(lv_screen_active());
        lv_obj_t *gear = find_text(lv_screen_active(), APP_SYMBOL_SETTINGS);
        lv_obj_t *bar = lv_obj_get_parent(lv_obj_get_parent(gear));
        if (lv_display_get_horizontal_resolution(lv_display_get_default()) >= 800) {
            lv_obj_t *assistant = ui_pages_create_topbar_action(bar, APP_SYMBOL_AI_AGENT, NULL, NULL);
            ui_pages_set_topbar_auxiliary(assistant);
            lv_obj_update_layout(assistant);
            CHECK(lv_obj_get_width(assistant) == lv_obj_get_width(lv_obj_get_parent(gear)));
            CHECK(lv_obj_get_height(assistant) == lv_obj_get_height(lv_obj_get_parent(gear)));
            if (state & 4) {
                lv_obj_t *warning = find_text(bar, APP_SYMBOL_ALERT);
                CHECK(lv_obj_get_x(lv_obj_get_parent(warning)) < lv_obj_get_x(assistant));
            }
            lv_obj_update_layout(lv_screen_active());
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(bar); i++) {
            lv_obj_t *a = lv_obj_get_child(bar, i);
            if (lv_obj_is_hidden(a)) continue;
            lv_area_t r; lv_obj_get_coords(a, &r);
            CHECK(r.x1 >= 0 && r.x2 < lv_display_get_horizontal_resolution(lv_display_get_default()));
            for (uint32_t j = i + 1; j < lv_obj_get_child_count(bar); j++) {
                lv_obj_t *b = lv_obj_get_child(bar, j);
                if (lv_obj_is_hidden(b)) continue;
                lv_area_t q; lv_obj_get_coords(b, &q);
                if (!(r.x2 < q.x1 || q.x2 < r.x1 || r.y2 < q.y1 || q.y2 < r.y1))
                    fprintf(stderr, "header state=%d objects=%u,%u x=%d..%d,%d..%d\n", state, i, j, r.x1, r.x2, q.x1, q.x2);
                CHECK(r.x2 < q.x1 || q.x2 < r.x1 || r.y2 < q.y1 || q.y2 < r.y1);
            }
        }
    }
    g_sim_mqtt = false;
    check_system_settings();
    const char *languages[] = {"en", "de", "es", "fr", "pl", "zh-cn", "zh-tw"};
    for (unsigned lang = 0; lang < sizeof(languages) / sizeof(languages[0]); lang++) {
        CHECK(ui_i18n_init(languages[lang]) == ESP_OK);
        ui_pages_init(); lv_obj_update_layout(lv_screen_active());
        lv_obj_t *gear = find_text(lv_screen_active(), APP_SYMBOL_SETTINGS);
        lv_obj_t *wifi = find_text(lv_screen_active(), APP_SYMBOL_WIFI_OFF);
        CHECK(gear && wifi);
        lv_obj_t *gear_chip = lv_obj_get_parent(gear), *wifi_chip = lv_obj_get_parent(wifi);
        CHECK(lv_obj_get_height(gear_chip) == 40 && lv_obj_get_height(wifi_chip) == 40);
        CHECK(lv_obj_get_width(gear_chip) == lv_obj_get_width(wifi_chip));
        lv_obj_send_event(gear_chip, LV_EVENT_CLICKED, NULL);
        CHECK(ui_screen_settings_root() != NULL);
        set_mode(1);
        lv_obj_update_layout(ui_screen_settings_root());
        check_bounds(ui_screen_settings_root(), ui_screen_settings_root());
        lv_obj_add_state(control(SCREEN_CLOCK_ANALOG), LV_STATE_CHECKED);
        lv_obj_send_event(control(SCREEN_CLOCK_ANALOG), LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_t *clock_off = control(SCREEN_CLOCK_OFF);
        lv_obj_remove_state(clock_off, LV_STATE_CHECKED);
        lv_obj_send_event(clock_off, LV_EVENT_VALUE_CHANGED, NULL);
        CHECK(lv_obj_is_hidden(lv_obj_get_parent(control(SCREEN_CLOCK_OFF_SECONDS))));
        lv_obj_t *slider = control(SCREEN_CLOCK_BRIGHTNESS);
        lv_slider_set_value(slider, 17, LV_ANIM_OFF);
        lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, NULL);
        /* A concurrent editor change to unrelated fields must survive the local patch. */
        strcpy(settings.ui_language, "en"); strcpy(settings.ha_ws_url, "ws://example.invalid/api/websocket");
        settings.hardware_active_brightness_percent = 76;
        click_save(); CHECK(ui_screen_settings_root() == NULL);
        CHECK(settings.display_clock.brightness == 17 && !settings.display_clock.off_enabled && settings.display_clock.analog);
        CHECK(settings.hardware_active_brightness_percent == 76 && strcmp(settings.ui_language, "en") == 0);
        CHECK(strcmp(settings.ha_ws_url, "ws://example.invalid/api/websocket") == 0);
        ui_screen_settings_open(); CHECK(lv_slider_get_value(control(SCREEN_CLOCK_BRIGHTNESS)) == 17);
        set_mode(0); click_save(); CHECK(strcmp(settings.hardware_saver_mode, "disabled") == 0);
        ui_screen_settings_open(); set_mode(1); click_save();
        CHECK(settings.display_clock.brightness == 17 && !settings.display_clock.off_enabled && settings.display_clock.analog);
        ui_screen_settings_open(); set_mode(2); fail_save = true; click_save();
        CHECK(ui_screen_settings_root() != NULL && strcmp(settings.hardware_saver_mode, "clock") == 0);
        fail_save = false; click_save(); CHECK(ui_screen_settings_root() == NULL);
    }
    for (int angle = 0; angle < 4; angle++) {
        ui_screen_settings_open();
        lv_obj_t *rotation = control(SCREEN_ROTATION);
        CHECK(lv_obj_check_type(rotation, &lv_dropdown_class));
        lv_dropdown_set_selected(rotation, angle);
        lv_obj_send_event(rotation, LV_EVENT_VALUE_CHANGED, NULL);
        click_save();
        CHECK(settings.hardware_rotation == angle * 90);
        ui_screen_settings_open();
        CHECK(lv_dropdown_get_selected(control(SCREEN_ROTATION)) == (unsigned)angle);
        click_save();
    }
    for (int i = 0; i < 100; i++) {
        ui_screen_settings_open(); CHECK(ui_screen_settings_root() != NULL);
        ui_pages_init(); CHECK(ui_screen_settings_root() == NULL);
        lv_tick_inc(110); lv_timer_handler();
    }
    display_clock_t clock; display_clock_defaults(&clock);
    CHECK(!display_clock_is_off(&clock, 359999) && display_clock_is_off(&clock, 360000));
    clock.off_enabled = false; CHECK(!display_clock_is_off(&clock, 4000000));
    const char *bad[] = {"[]", "{\"brightness\":0}", "{\"show_date\":1}", "{\"start_seconds\":15.5}"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        cJSON *json = cJSON_Parse(bad[i]); display_clock_t before = clock;
        CHECK(!display_clock_parse(json, &clock) && display_clock_equal(&clock, &before)); cJSON_Delete(json);
    }
    CHECK(!clock.analog);
    cJSON *patch = cJSON_Parse("{\"analog\":true}");
    CHECK(display_clock_parse(patch, &clock) && clock.analog); cJSON_Delete(patch);
    patch = cJSON_Parse("{\"analog\":1}");
    CHECK(!display_clock_parse(patch, &clock) && clock.analog); cJSON_Delete(patch);
    patch = cJSON_CreateObject(); CHECK(display_clock_write(patch, &clock));
    display_clock_t roundtrip; display_clock_defaults(&roundtrip);
    CHECK(display_clock_parse(cJSON_GetObjectItemCaseSensitive(patch, "clock"), &roundtrip));
    CHECK(display_clock_equal(&clock, &roundtrip)); cJSON_Delete(patch);
    patch = cJSON_Parse("{}"); CHECK(display_clock_parse(patch, &clock) && clock.analog); cJSON_Delete(patch);
    struct tm time = {.tm_year=126, .tm_mon=9, .tm_mday=24, .tm_hour=0, .tm_min=5, .tm_sec=7};
    char text[32], date[32]; display_clock_format(&clock, &time, text, sizeof(text), date, sizeof(date));
    CHECK(strcmp(text, "00:05:07") == 0 && strcmp(date, "2026-10-24") == 0);
    clock.format_24h = false; clock.show_date = false;
    display_clock_format(&clock, &time, text, sizeof(text), date, sizeof(date));
    CHECK(strcmp(text, "12:05:07 AM") == 0 && date[0] == '\0');
    ui_transition_configure(true);
    ui_screen_settings_open();
    lv_obj_t *animated = ui_screen_settings_root();
    CHECK(lv_obj_get_style_opa(animated, 0) == 0);
    SDL_Delay(900); lv_timer_handler();
    CHECK(lv_obj_get_style_opa(animated, 0) == 255);
    lv_obj_t *cancel = lv_obj_get_parent(find_text(animated, ui_i18n_get("screen.cancel", "Cancel")));
    lv_obj_send_event(cancel, LV_EVENT_CLICKED, NULL);
    CHECK(ui_screen_settings_root() == animated && lv_obj_has_state(cancel, LV_STATE_DISABLED));
    lv_obj_send_event(cancel, LV_EVENT_CLICKED, NULL); /* duplicate close is harmless */
    SDL_Delay(900); lv_timer_handler();
    CHECK(ui_screen_settings_root() == NULL);
    ui_screen_settings_open();
    ui_pages_init(); /* rebuild while settings fade in */
    SDL_Delay(900); lv_timer_handler();
    CHECK(ui_screen_settings_root() == NULL);
    ui_transition_configure(false);
    printf("Screen settings: %u checks passed\n", checks); return 0;
}
