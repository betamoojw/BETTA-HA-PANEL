/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "esp_app_desc.h"
#include "drivers/display_init.h"
#include "ui/text_font.h"
#include "ui/ui_boot_splash.h"
#include "ui/ui_license_warning.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static bool fail_lock;
static ui_license_warning_status_t license_status(void)
{ return (ui_license_warning_status_t){.state = LICENSE_MISSING, .uptime_us = LICENSE_LCD_DELAY_US}; }
bool display_is_ready(void) { return true; }
bool display_lock(uint32_t ms) { (void)ms; return !fail_lock; }
void display_unlock(void) {}
void display_note_activity(void) {}
int display_geometry_width(void) { return 480; }
int display_geometry_height(void) { return 480; }
const lv_font_t *ui_font_role(ui_text_role_t role) { (void)role; return LV_FONT_DEFAULT; }
const char *ui_i18n_get(const char *key, const char *fallback) { (void)key; return fallback; }
const char *ui_i18n_get_language(void) { return "en"; }
const esp_app_desc_t *esp_app_get_description(void)
{ static const esp_app_desc_t desc = {.version = "test"}; return &desc; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }

int main(void)
{
    lv_init();
    lv_display_t *d = lv_display_create(480, 480);
    static uint16_t buffer[480 * 2];
    lv_display_set_buffers(d, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    const uint32_t children = lv_obj_get_child_count(lv_layer_top());
    ui_license_warning_set_source(license_status);
    for (int manual = 0; manual < 2; ++manual) {
        CHECK(ui_boot_splash_show() == ESP_OK);
        if (manual) {
            ui_boot_splash_set_progress(100);
            ui_boot_splash_set_setup();
            lv_tick_inc(300); lv_timer_handler();
            /* Setup reuses the splash owner instead of adding a permanent top layer. */
            CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
            lv_draw_buf_t *image = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
            CHECK(image != NULL); lv_draw_buf_destroy(image);
        }
        CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
        fail_lock = true;
        ui_boot_splash_hide();
        CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
        fail_lock = false;
        /* The production LVGL callback must retire the overlay without a second hide call. */
        lv_tick_inc(300);
        lv_timer_handler();
        CHECK(lv_obj_get_child_count(lv_layer_top()) == children);
        ui_boot_splash_hide(); /* Idempotent after deferred deletion. */
    }
    CHECK(ui_boot_splash_show() == ESP_OK);
    ui_boot_splash_hide();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children);
    puts("boot splash: automatic/manual deferred hide and immediate hide passed");
    return 0;
}
