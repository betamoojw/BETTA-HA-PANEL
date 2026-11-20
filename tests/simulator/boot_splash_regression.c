/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "esp_app_desc.h"
#include "drivers/display_init.h"
#include "sim_clock.h"
#include "ui/periodic_refresh.h"
#include "ui/text_font.h"
#include "ui/ui_boot_splash.h"
#include "ui/ui_license_warning.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static bool fail_lock;
static unsigned lock_attempts;
static unsigned activity_events;
static void (*before_display_lock)(void);
static ui_license_warning_status_t license_status(void)
{ return (ui_license_warning_status_t){.state = LICENSE_MISSING, .uptime_us = LICENSE_LCD_DELAY_US}; }
bool display_is_ready(void) { return true; }
bool display_lock(uint32_t ms)
{
    (void)ms;
    ++lock_attempts;
    if (before_display_lock != NULL) {
        void (*interleave)(void) = before_display_lock;
        before_display_lock = NULL;
        interleave();
    }
    return !fail_lock;
}
void display_unlock(void) {}
void display_note_activity(void) { ++activity_events; }
int display_geometry_width(void) { return 480; }
int display_geometry_height(void) { return 480; }
const lv_font_t *ui_font_role(ui_text_role_t role) { (void)role; return LV_FONT_DEFAULT; }
const char *ui_i18n_get(const char *key, const char *fallback) { (void)key; return fallback; }
const char *ui_i18n_get_language(void) { return "en"; }
const esp_app_desc_t *esp_app_get_description(void)
{ static const esp_app_desc_t desc = {.version = "test"}; return &desc; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }

static void advance_time(uint32_t ms)
{
    sim_virtual_delay(ms);
    lv_tick_inc(ms);
}

static void reshow_splash(void) { CHECK(ui_boot_splash_show() == ESP_OK); }
static void run_due_timers(void) { (void)lv_timer_handler(); }

static void check_dismissal_interleavings(uint32_t children)
{
    CHECK(ui_boot_splash_show() == ESP_OK);
    advance_time(1500);
    const unsigned activity_before_hide = activity_events;
    before_display_lock = reshow_splash;
    ui_boot_splash_hide();
    lv_timer_handler();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
    CHECK(activity_events == activity_before_hide);

    /* LVGL may delete the labels while a caller is waiting for the display lock. */
    ui_boot_splash_request_hide();
    advance_time(280);
    before_display_lock = run_due_timers;
    ui_boot_splash_set_title("Late title update");
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children);
    CHECK(activity_events == activity_before_hide + 1);

    CHECK(ui_boot_splash_show() == ESP_OK);
    advance_time(1500);
    fail_lock = true;
    ui_boot_splash_hide();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
    fail_lock = false;
    lv_timer_handler();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children);
    CHECK(activity_events == activity_before_hide + 3);

    CHECK(ui_boot_splash_show() == ESP_OK);
    advance_time(1199);
    /* The scheduler delay stub leaves the virtual clock just before the deadline. */
    ui_boot_splash_hide();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
    advance_time(280);
    lv_timer_handler();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children);
}

static void check_periodic_refresh(void)
{
    int64_t next_attempt_ms = UI_PERIODIC_REFRESH_MS;
    const unsigned locks_before = lock_attempts;
    fail_lock = true;
    /* A busy first render spans many UI task turns. Only the periodic deadlines may try. */
    for (int64_t now_ms = 0; now_ms < 3000; now_ms += 20) {
        if (ui_periodic_refresh_begin(now_ms, &next_attempt_ms)) CHECK(!display_lock(20));
    }
    CHECK(lock_attempts == locks_before + 2);
    fail_lock = false;
    CHECK(!ui_periodic_refresh_begin(2999, &next_attempt_ms));
    CHECK(ui_periodic_refresh_begin(3000, &next_attempt_ms) && display_lock(20));
    display_unlock();
    CHECK(lock_attempts == locks_before + 3);
    CHECK(ui_periodic_refresh_begin(6000, &next_attempt_ms));
    CHECK(!ui_periodic_refresh_begin(6000, &next_attempt_ms));
}

int main(void)
{
    sim_clock_set(1);
    lv_init();
    lv_display_t *d = lv_display_create(480, 480);
    static uint16_t buffer[480 * 2];
    lv_display_set_buffers(d, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    lv_obj_t *dashboard = lv_label_create(lv_screen_active());
    lv_label_set_text(dashboard, "Dashboard");
    const uint32_t children = lv_obj_get_child_count(lv_layer_top());
    ui_license_warning_set_source(license_status);
    for (int manual = 0; manual < 2; ++manual) {
        CHECK(ui_boot_splash_show() == ESP_OK);
        if (manual) {
            ui_boot_splash_set_progress(100);
            ui_boot_splash_set_setup();
            advance_time(1500);
            lv_timer_handler();
            /* Provisioning remains visible past the minimum without a hide request. */
            CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
            lv_draw_buf_t *image = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
            CHECK(image != NULL); lv_draw_buf_destroy(image);
        }
        CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
        const unsigned locks_before_hide = lock_attempts;
        const unsigned activity_before_hide = activity_events;
        fail_lock = true;
        ui_boot_splash_request_hide();
        ui_boot_splash_request_hide();
        CHECK(lock_attempts == locks_before_hide);
        /* A busy render keeps the LVGL timer from running until it finishes. */
        advance_time(manual ? 2040 : 1199);
        CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
        CHECK(activity_events == activity_before_hide);
        lv_timer_handler();
        if (!manual) {
            CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
            CHECK(activity_events == activity_before_hide);
            advance_time(280);
            lv_timer_handler();
        }
        CHECK(lv_obj_get_child_count(lv_layer_top()) == children);
        CHECK(lv_obj_is_valid(dashboard));
        CHECK(lock_attempts == locks_before_hide);
        CHECK(activity_events == activity_before_hide + 1);
        ui_boot_splash_request_hide(); /* A stale request must not dismiss the next splash. */
        fail_lock = false;
    }

    CHECK(ui_boot_splash_show() == ESP_OK);
    advance_time(1500);
    ui_boot_splash_request_hide();
    CHECK(ui_boot_splash_show() == ESP_OK);
    lv_timer_handler();
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children + 1);
    ui_boot_splash_hide(); /* Coprocessor update error paths retain synchronous dismissal. */
    CHECK(lv_obj_get_child_count(lv_layer_top()) == children);

    check_dismissal_interleavings(children);
    check_periodic_refresh();
    puts("boot splash: minimum time, provisioning, and lock-free dismiss passed");
    puts("periodic refresh: bounded attempts, recovery, and no catch-up retries passed");
    return 0;
}
