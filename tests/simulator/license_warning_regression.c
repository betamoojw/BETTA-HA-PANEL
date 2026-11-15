/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "sim_clock.h"
#include "ui/ui_license_warning.h"
#include "ui/ui_pages.h"
#include "ui/ui_i18n.h"
#include "ui/ui_screen_settings.h"
#include "ui/text_font.h"
#include "ui/fonts/app_symbols.h"

static ui_license_warning_status_t sample;
static ui_license_warning_status_t read_status(void) { return sample; }
/* Synthetic CLI fixture, never read by firmware or sent to a device. */
bool sim_license_warning_fixture(const char *state)
{
    static const char *names[] = {"missing", "valid", "invalid", "error", "unconfigured"};
    for (unsigned i = 0; i < 5; ++i) if (!strcmp(state, names[i])) {
        sample = (ui_license_warning_status_t){.state = i, .uptime_us = LICENSE_LCD_DELAY_US};
        ui_license_warning_set_source(read_status);
        return true;
    }
    return false;
}
static void tick(void) { sim_virtual_delay(1001); lv_timer_handler(); }
static unsigned timers(void)
{
    unsigned count = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) count++;
    return count;
}
static lv_draw_buf_t *shot(lv_obj_t *obj)
{
    lv_obj_update_layout(obj);
    lv_draw_buf_t *b = lv_snapshot_take(obj, LV_COLOR_FORMAT_RGB888);
    assert(b);
    return b;
}
static bool same(lv_draw_buf_t *a, lv_draw_buf_t *b, lv_area_t area)
{
    assert(a->header.w == b->header.w && a->header.h == b->header.h);
    for (int y = area.y1; y <= area.y2; ++y)
        if (memcmp(a->data + y * a->header.stride + area.x1 * 3,
                   b->data + y * b->header.stride + area.x1 * 3,
                   (area.x2 - area.x1 + 1) * 3)) return false;
    return true;
}
static lv_obj_t *find(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find(lv_obj_get_child(root, i), text);
        if (found) return found;
    }
    return NULL;
}
static void save(lv_obj_t *obj, const char *name)
{
    const char *directory = getenv("BETTA_LICENSE_CAPTURES");
    if (!directory) return;
    lv_draw_buf_t *b = shot(obj);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(b->data, b->header.w,
        b->header.h, 24, b->header.stride, SDL_PIXELFORMAT_BGR24);
    assert(s);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.bmp", directory, name);
    assert(SDL_SaveBMP(s, path) == 0);
    SDL_FreeSurface(s); lv_draw_buf_destroy(b);
}

int sim_license_warning_regression(void)
{
    ui_license_warning_set_source(read_status);
    sample = (ui_license_warning_status_t){.state = LICENSE_MISSING};
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    const unsigned initial_timers = timers(), top_children = lv_obj_get_child_count(lv_layer_top());
    const int w = lv_display_get_horizontal_resolution(lv_display_get_default());
    const int h = lv_display_get_vertical_resolution(lv_display_get_default());
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_remove_style_all(bar); lv_obj_set_size(bar, w, 60);
    lv_obj_t *button = lv_button_create(bar);
    lv_obj_set_pos(button, w - 200, 8); lv_obj_set_size(button, 180, 44);
    lv_obj_update_layout(screen);
    lv_point_t point = {w - 100, 25};
    lv_obj_t *hit = lv_indev_search_obj(screen, &point);
    assert(hit == button);
    lv_draw_buf_t *before = shot(screen);
    assert(ui_license_warning_attach(bar, button, 60));
    assert(ui_license_warning_attach(bar, button, 60));
    assert(timers() == initial_timers + 1 && lv_obj_get_child_count(bar) == 1);
    sample.uptime_us = LICENSE_LCD_DELAY_US - 1; tick();
    lv_draw_buf_t *after = shot(screen);
    assert(same(before, after, (lv_area_t){0, 0, w - 1, h - 1}));
    lv_draw_buf_destroy(after);
    sample.uptime_us++; tick(); after = shot(screen);
    assert(!same(before, after, (lv_area_t){0, 0, w - 201, 59}));
    assert(same(before, after, (lv_area_t){w - 200, 6, w - 1, 53}));
    assert(same(before, after, (lv_area_t){0, 60, w - 1, h - 1}));
    assert(lv_indev_search_obj(screen, &point) == hit);
    assert(lv_obj_get_child_count(lv_layer_top()) == top_children);
    lv_draw_buf_destroy(after);
    for (int state = LICENSE_MISSING; state <= LICENSE_UNCONFIGURED; ++state) {
        sample.state = state; tick(); after = shot(screen);
        assert(same(before, after, (lv_area_t){0, 0, w - 1, 59}) == (state == LICENSE_VALID));
        lv_draw_buf_destroy(after);
    }
    sample.suppressed = true; tick(); after = shot(screen);
    assert(same(before, after, (lv_area_t){0, 0, w - 1, h - 1}));
    lv_draw_buf_destroy(after); sample.suppressed = false;
    /* A fullscreen top-layer view keeps priority without registering the warning there. */
    lv_obj_t *modal = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(modal); lv_obj_set_size(modal, w, h);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x172839), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
    lv_draw_buf_t *modal_before = shot(lv_layer_top());
    sample.state = LICENSE_VALID; tick();
    lv_draw_buf_t *modal_after = shot(lv_layer_top());
    assert(same(modal_before, modal_after, (lv_area_t){0, 0, w - 1, h - 1}));
    lv_draw_buf_destroy(modal_before); lv_draw_buf_destroy(modal_after);
    lv_obj_delete(modal); sample.state = LICENSE_MISSING;
    /* Simulate reboot and a page rebuild: time, not object age, owns the delay. */
    sample.uptime_us = 0; tick(); after = shot(screen);
    assert(same(before, after, (lv_area_t){0, 0, w - 1, h - 1}));
    lv_draw_buf_destroy(after); lv_draw_buf_destroy(before);
    lv_obj_delete(bar); assert(timers() == initial_timers);
    for (int i = 0; i < 40; ++i) {
        bar = lv_obj_create(screen); lv_obj_set_size(bar, w, 60);
        assert(ui_license_warning_attach(bar, NULL, 60));
        lv_obj_t *other = lv_obj_create(screen); lv_obj_set_size(other, w, 60);
        assert(ui_license_warning_attach(other, NULL, 60));
        assert(timers() == initial_timers + 1);
        lv_obj_delete(bar); assert(timers() == initial_timers + 1);
        lv_obj_delete(other); assert(timers() == initial_timers);
    }
    extern bool g_sim_mqtt;
    const char *languages[] = {"en", "de", "es", "fr", "pl", "zh-cn", "zh-tw"};
    const char *keys[] = {"missing", "invalid", "unconfigured", "error", "compact"};
    sample.state = LICENSE_UNCONFIGURED; sample.uptime_us = LICENSE_LCD_DELAY_US;
    for (unsigned lang = 0; lang < 7; ++lang) for (int step = 0; step < 3; ++step) {
        assert(ui_i18n_init(languages[lang]) == ESP_OK);
        for (unsigned key = 0; key < 5; ++key) {
            char path[64]; snprintf(path, sizeof(path), "license_lcd.%s", keys[key]);
            assert(strcmp(ui_i18n_get(path, "MISSING_TRANSLATION"), "MISSING_TRANSLATION"));
        }
        ui_fonts_set_user_step(step);
        g_sim_mqtt = true; ui_pages_init();
        ui_pages_set_alert_warning(true);
        ui_pages_set_topbar_status(true, true, true, true);
        lv_obj_t *gear = find(screen, APP_SYMBOL_SETTINGS); assert(gear);
        lv_obj_t *control = lv_obj_get_parent(gear);
        bar = lv_obj_get_parent(control);
        if (w >= 800) {
            lv_obj_t *voice = ui_pages_create_topbar_action(bar, APP_SYMBOL_AI_AGENT, NULL, NULL);
            ui_pages_set_topbar_auxiliary(voice);
        }
        lv_obj_update_layout(screen); tick();
        lv_area_t bounds; lv_obj_get_coords(control, &bounds);
        point = (lv_point_t){(bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2};
        assert(lv_indev_search_obj(screen, &point) == control);
        assert(lv_obj_get_child_count(lv_layer_top()) == top_children);
        char name[64]; snprintf(name, sizeof(name), "%s-step%d-topbar", languages[lang], step);
        save(bar, name);
        ui_screen_settings_open(); tick();
        lv_obj_t *settings = ui_screen_settings_root(); assert(settings);
        lv_obj_t *cancel_text = find(settings, ui_i18n_get("screen.cancel", "Cancel")); assert(cancel_text);
        lv_obj_t *cancel = lv_obj_get_parent(cancel_text);
        lv_obj_get_coords(cancel, &bounds);
        point = (lv_point_t){(bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2};
        assert(lv_indev_search_obj(screen, &point) == cancel);
        snprintf(name, sizeof(name), "%s-step%d-settings", languages[lang], step);
        save(settings, name);
        lv_obj_send_event(cancel, LV_EVENT_CLICKED, NULL); tick();
        assert(!ui_screen_settings_root());
    }
    g_sim_mqtt = false;
    ui_license_warning_set_source(NULL);
    lv_obj_clean(screen);
    puts("LCD license warning: boundary/state pixels, touch, layers, timer lifecycle and 7 languages x 3 text sizes passed");
    return 0;
}
