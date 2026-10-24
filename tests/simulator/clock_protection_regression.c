/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
 * Production protection state machine with a controllable clock and real LVGL.
 */
#include <assert.h>
#include "esp_timer.h"
static uint32_t test_ms = 1000;
static int64_t test_time(void) { return (int64_t)test_ms * 1000; }
#define esp_timer_get_time test_time
#include "../../main/ui/ui_display_protection.c"

static runtime_settings_t saved;
static int brightness = 60, wakes;
static bool night;
static bool storage_busy;
const lv_font_t *ui_font_role(ui_text_role_t role) { (void)role; return LV_FONT_DEFAULT; }
const lv_font_t *ui_font_text_px(int px) { (void)px; return LV_FONT_DEFAULT; }
const lv_font_t *ui_font_probe(int px) { (void)px; return LV_FONT_DEFAULT; }
int ui_font_text_px_of(const lv_font_t *font) { (void)font; return 1; }
text_board_t ui_fonts_board(void) { return TEXT_BOARD_S3; }
int text_scale_floor_px(text_board_t board) { (void)board; return 1; }
const lv_font_t *mdi_font_weather(void) { return LV_FONT_DEFAULT; }
const char *ui_i18n_get(const char *key, const char *fallback) { (void)key; return fallback; }
esp_err_t runtime_settings_load(runtime_settings_t *out) { *out = saved; return storage_busy ? ESP_ERR_TIMEOUT : ESP_OK; }
void runtime_settings_set_defaults(runtime_settings_t *out) { *out = saved; }
bool night_mode_active(void) { return night; }
int night_mode_brightness_percent(void) { return 0; }
bool display_is_ready(void) { return true; }
int display_get_brightness_percent(void) { return brightness; }
esp_err_t display_set_brightness_percent(int percent) { brightness = percent; return ESP_OK; }
void display_note_activity(void) { wakes++; brightness = 60; ui_display_protection_note_activity(); }
void user_activity_note(uint32_t now) { (void)now; }
bool ha_model_get_state(const char *id, ha_state_t *out) { (void)id; (void)out; return false; }
uint32_t ui_runtime_weather_icon_codepoint_for_key(const char *key) { (void)key; return 0; }
uint16_t ui_pages_count(void) { return 1; }
bool ui_pages_show_index(uint16_t index) { (void)index; return true; }
bool ui_pages_next(void) { return true; }
void ui_pages_set_clock_24h(bool format) { (void)format; }

static void advance(uint32_t ms)
{
    test_ms += ms; lv_tick_inc(ms); ui_display_protection_poll();
    lv_obj_update_layout(lv_layer_top());
}
int main(int argc, char **argv)
{
    int width = argc > 1 ? atoi(argv[1]) : 480;
    int height = argc > 2 ? atoi(argv[2]) : 480;
    lv_init(); lv_display_t *display = lv_display_create(width, height);
    lv_timer_pause(lv_display_get_refr_timer(display));
    strcpy(saved.hardware_saver_mode, "clock"); display_clock_defaults(&saved.display_clock);
    saved.display_clock.start_seconds = 15; saved.display_clock.off_seconds = 30;
    saved.display_clock.brightness = 17;
    ui_display_protection_init();
    advance(14999); assert(!ui_display_protection_is_active());
    advance(1); assert(ui_display_protection_is_active() && !ui_display_protection_is_off());
    assert(brightness == 17 && !lv_obj_is_hidden(s_clock_group));
    storage_busy = true; assert(!ui_display_protection_reload()); advance(1);
    assert(ui_display_protection_is_active() && brightness == 17); storage_busy = false;
    assert(strlen(lv_label_get_text(s_clock_time)) == 8 && strlen(lv_label_get_text(s_clock_date)) == 10);
    advance(29998); assert(!ui_display_protection_is_off());
    advance(1); assert(ui_display_protection_is_off() && brightness == 0);
    /* The complete wake gesture is consumed before the overlay disappears. */
    lv_obj_send_event(s_overlay, LV_EVENT_PRESSED, NULL); advance(1);
    assert(ui_display_protection_is_active());
    lv_obj_send_event(s_overlay, LV_EVENT_RELEASED, NULL);
    assert(!ui_display_protection_is_active() && wakes == 1 && brightness == 60);
    saved.display_clock.off_enabled = false; saved.display_clock.show_date = false;
    saved.display_clock.show_seconds = false; saved.display_clock.format_24h = false;
    ui_display_protection_reload(); advance(15000);
    assert(strlen(lv_label_get_text(s_clock_time)) == 8 && strlen(lv_label_get_text(s_clock_date)) == 0);
    for (int i = 0; i < 10; i++) {
        advance(60000); assert(!ui_display_protection_is_off() && brightness == 17);
        lv_area_t rect; lv_obj_get_coords(s_clock_group, &rect);
        assert(rect.x1 >= 0 && rect.y1 >= 0 && rect.x2 < width && rect.y2 < height);
    }
    night = true; advance(1); assert(ui_display_protection_is_off() && brightness == 0);
    night = false; ui_display_protection_reload();
    assert(ui_display_brightness_test_start(23, 0)); advance(15000);
    assert(ui_display_brightness_test_update(29)); advance(15000);
    assert(!ui_display_protection_is_active() && ui_display_brightness_test_percent() == -1);
    assert(!ui_display_brightness_test_update(31));
    strcpy(saved.hardware_saver_mode, "disabled"); ui_display_protection_reload();
    advance(300000); assert(!ui_display_protection_is_active());
    printf("Clock protection %dx%d: timing, movement, night priority, wake gesture and bounded preview passed\n", width, height);
    lv_deinit(); return 0;
}
