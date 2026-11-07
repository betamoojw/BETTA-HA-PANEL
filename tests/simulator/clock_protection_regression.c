/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
 * Production protection state machine with a controllable clock and real LVGL.
 */
#include <assert.h>
#include "esp_timer.h"
static uint32_t test_ms = 1000;
static int64_t test_time(void) { return (int64_t)test_ms * 1000; }
#define esp_timer_get_time test_time
#include "../../main/ui/ui_display_protection.c"
#include "ui/theme/ui_style_registry.h"
void sim_screenshot_regression(lv_obj_t *overlay, const char *capture_path);

static runtime_settings_t saved;
static int brightness = 60, wakes;
static bool night;
static bool storage_busy;
bool ui_reply_font_available(void) { return false; }
lv_font_t *ui_reply_font_create(int px) { (void)px; return NULL; }
const lv_font_t *mdi_font_weather(void) { return LV_FONT_DEFAULT; }
const char *ui_i18n_get(const char *key, const char *fallback) { (void)key; return fallback; }
esp_err_t theme_store_load_custom(const char *id, theme_entry_t *out) { (void)id; (void)out; return ESP_ERR_NOT_FOUND; }
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

static void check_digital_layout(int width, int height)
{
    saved.display_clock.style = DISPLAY_CLOCK_DIGITAL;
    saved.display_clock.off_enabled = false;
    int instances = 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int step = 0; step <= 2; step += 2) {
            ui_fonts_set_user_step(step);
            for (int options = 0; options < 8; ++options) {
                saved.display_clock.format_24h = options & 1;
                saved.display_clock.show_seconds = options & 2;
                saved.display_clock.show_date = options & 4;
                assert(ui_display_protection_reload()); advance(15000);
                const lv_font_t *font = lv_obj_get_style_text_font(s_clock_time, 0);
                assert(font->line_height > ui_font_role(UI_TEXT_HERO)->line_height);
                assert(lv_obj_get_height(s_clock_time) == font->line_height);
                /* All hour/minute combinations fit without changing font as time advances. */
                for (int hour = 0; hour < 24; ++hour) for (int minute = 0; minute < 60; ++minute) {
                    struct tm sample = {.tm_year=126, .tm_mon=9, .tm_mday=26,
                                        .tm_hour=hour, .tm_min=minute, .tm_sec=minute};
                    char text[32], date[32]; lv_point_t size;
                    display_clock_format(&saved.display_clock, &sample, text, sizeof(text), date, sizeof(date));
                    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
                    assert(size.x <= lv_obj_get_width(s_clock_group));
                }
                for (int pos = 0; pos < 8; ++pos) {
                    advance(60000);
                    lv_area_t group, time, date;
                    lv_obj_get_coords(s_clock_group, &group); lv_obj_get_coords(s_clock_time, &time);
                    assert(group.x1 >= 20 && group.y1 >= 20 && group.x2 < width-20 && group.y2 < height-20);
                    assert(time.x1 >= group.x1 && time.x2 <= group.x2 && time.y2 <= group.y2);
                    if (saved.display_clock.show_date) {
                        lv_obj_get_coords(s_clock_date, &date);
                        assert(date.y1 > time.y2 && date.x1 >= group.x1 && date.x2 <= group.x2);
                    }
                }
            }
        }
        if (pass) assert(ui_fonts_stats().instances == instances);
        instances = ui_fonts_stats().instances;
    }
    ui_fonts_set_user_step(0);
}

static unsigned timer_count(void)
{
    unsigned count = 0;
    for (lv_timer_t *timer = lv_timer_get_next(NULL); timer; timer = lv_timer_get_next(timer)) count++;
    return count;
}

static void check_flip_layout(int width, int height)
{
    saved.display_clock.style = DISPLAY_CLOCK_FLIP;
    saved.display_clock.off_enabled = false;
    saved.display_clock.show_date = false; saved.display_clock.show_seconds = false;
    assert(ui_display_protection_reload());
    unsigned timers = timer_count();
    const char *themes[] = {"dark_v2", "light", "black", "contrast"};
    for (int format = 0; format < 2; format++) {
        saved.display_clock.format_24h = format != 0;
        assert(ui_display_protection_reload()); advance(15000);
        for (unsigned theme = 0; theme < sizeof(themes) / sizeof(themes[0]); theme++) {
            assert(theme_palette_activate_by_id(themes[theme]) == ESP_OK);
            theme_default_rebuild_styles();
            for (int style = UI_STYLE_FLAT; style <= UI_STYLE_MATTE; style++) {
                ui_style_set((ui_style_kind_t)style);
                uint32_t activity = s_last_activity_ms;
                assert(ui_display_protection_refresh_appearance());
                assert(s_last_activity_ms == activity && s_state == SAVER_CLOCK && brightness == 17);
                assert(s_flip_clock && timer_count() == timers + 1 && lv_obj_is_hidden(s_clock_group));
                assert(lv_color_eq(lv_obj_get_style_bg_color(s_overlay, 0), ui_style_page_bg(lv_color_hex(APP_UI_COLOR_CONTENT_BG))));
                lv_obj_t *group = lv_obj_get_child(s_flip_clock, 0);
                struct tm sample = {.tm_year=126,.tm_mon=10,.tm_mday=6,.tm_wday=5,.tm_hour=20,.tm_min=58,.tm_sec=36};
                ui_flip_clock_update(s_flip_clock, &sample, false);
                assert(!strcmp(lv_label_get_text(lv_obj_get_child(group, 0)), "Friday, 2026-11-06"));
                assert(lv_obj_is_hidden(lv_obj_get_child(group, 1)) == (format != 0));
                assert(!strcmp(lv_label_get_text(lv_obj_get_child(group, 1)), "PM"));
                const char *digits = format ? "205836" : "085836";
                for (unsigned i = 0; i < 6; i++) {
                    lv_obj_t *label = lv_obj_get_child(lv_obj_get_child(lv_obj_get_child(group, 4+i), 0), 0);
                    assert(lv_label_get_text(label)[0] == digits[i]);
                    assert(lv_color_eq(lv_obj_get_style_text_color(label, 0), theme_default_color_text_primary()));
                }
                lv_obj_update_layout(s_overlay);
                lv_area_t before; lv_obj_get_coords(group, &before);
                assert(abs(before.x1 + before.x2 - (width-1)) <= 2);
                assert(abs(before.y1 + before.y2 - (height-1)) <= 2);
                assert(before.x1 >= 0 && before.y1 >= 0 && before.x2 < width && before.y2 < height);
                for (int minute = 0; minute < 3; minute++) {
                    advance(60000);
                    lv_area_t after; lv_obj_get_coords(group, &after);
                    assert(before.x1 == after.x1 && before.y1 == after.y1 && before.x2 == after.x2 && before.y2 == after.y2);
                }
                sample.tm_sec++; ui_flip_clock_update(s_flip_clock, &sample, true);
                assert(lv_anim_count_running() > 0);
            }
        }
        /* A complete touch dismisses only the overlay and releases its timers/animations. */
        lv_obj_send_event(s_overlay, LV_EVENT_PRESSED, NULL); advance(1);
        assert(ui_display_protection_is_active());
        lv_obj_send_event(s_overlay, LV_EVENT_RELEASED, NULL);
        assert(!ui_display_protection_is_active() && !s_flip_clock && timer_count() == timers && lv_anim_count_running() == 0);
        assert(ui_display_protection_refresh_appearance() && !s_flip_clock);
    }
    saved.display_clock.off_enabled = true;
    assert(ui_display_protection_reload()); advance(15000);
    assert(s_flip_clock); advance(29999);
    assert(ui_display_protection_refresh_appearance()); /* Theme rebuild must not extend the deadline. */
    advance(1);
    assert(ui_display_protection_is_off() && !s_flip_clock && brightness == 0 && timer_count() == timers);
    assert(lv_color_eq(lv_obj_get_style_bg_color(s_overlay, 0), lv_color_black()));
    saved.display_clock.off_enabled = false;
    assert(ui_display_protection_reload()); advance(15000);
    night = true; advance(1);
    assert(ui_display_protection_is_off() && !s_flip_clock && brightness == 0);
    night = false;
    assert(!saved.display_clock.show_date && !saved.display_clock.show_seconds);
    assert(ui_display_protection_reload());
}

static void snapshot(const char *path, int width, int height)
{
    if (!s_flip_clock) lv_obj_center(s_clock_group);
    lv_obj_update_layout(s_overlay);
    lv_draw_buf_t *shot = lv_snapshot_take(s_overlay, LV_COLOR_FORMAT_RGB888);
    assert(shot);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = lv_draw_buf_goto_xy(shot, 0, y);
        for (int x = 0; x < width; ++x) {
            uint8_t rgb[] = {row[x*3+2], row[x*3+1], row[x*3]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    fclose(file); lv_draw_buf_destroy(shot);
}
int main(int argc, char **argv)
{
    int width = argc > 1 ? atoi(argv[1]) : 480;
    int height = argc > 2 ? atoi(argv[2]) : 480;
    lv_init(); lv_display_t *display = lv_display_create(width, height);
    lv_timer_pause(lv_display_get_refr_timer(display));
    ui_fonts_set_board(width == 480 ? TEXT_BOARD_S3 : TEXT_BOARD_WIDE10);
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
    saved.display_clock.style = DISPLAY_CLOCK_ANALOG; saved.display_clock.show_seconds = true;
    saved.display_clock.show_date = true; saved.display_clock.off_enabled = true;
    ui_display_protection_reload(); advance(15000);
    assert(!lv_obj_is_hidden(s_clock_dial) && lv_obj_is_hidden(s_clock_time));
    assert(!lv_obj_is_hidden(s_clock_date));
    advance(30000); assert(ui_display_protection_is_off() && brightness == 0);
    lv_obj_send_event(s_overlay, LV_EVENT_PRESSED, NULL);
    lv_obj_send_event(s_overlay, LV_EVENT_RELEASED, NULL);
    assert(!ui_display_protection_is_active());
    saved.display_clock.off_enabled = false;
    saved.display_clock.show_date = false; saved.display_clock.show_seconds = false;
    ui_display_protection_reload(); advance(15000);
    assert(lv_obj_is_hidden(s_clock_date));
    for (int i = 0; i < 10; ++i) {
        advance(60000);
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
    strcpy(saved.hardware_saver_mode, "clock"); check_digital_layout(width, height);
    check_flip_layout(width, height);
    if (argc > 3) {
        saved.display_clock.style = argc < 5 || !strcmp(argv[4], "analog") ? DISPLAY_CLOCK_ANALOG :
            (!strcmp(argv[4], "flip") ? DISPLAY_CLOCK_FLIP : DISPLAY_CLOCK_DIGITAL);
        assert(theme_palette_activate_by_id(argc > 5 ? argv[5] : "dark_v2") == ESP_OK);
        theme_default_rebuild_styles(); ui_style_set(ui_style_from_name(argc > 6 ? argv[6] : "flat"));
        saved.display_clock.show_date = true;
        saved.display_clock.show_seconds = argc < 5 || strcmp(argv[4], "short");
        saved.display_clock.format_24h = saved.display_clock.style != DISPLAY_CLOCK_FLIP && (argc < 5 || strcmp(argv[4], "12"));
        assert(ui_display_protection_reload()); advance(15000);
        struct tm sample = {.tm_year=126, .tm_mon=9, .tm_mday=26, .tm_wday=1, .tm_hour=20, .tm_min=58, .tm_sec=36};
        char text[32], date[32];
        display_clock_format(&saved.display_clock, &sample, text, sizeof(text), date, sizeof(date));
        lv_label_set_text(s_clock_time, text); lv_label_set_text(s_clock_date, date);
        ui_analog_clock_update(s_clock_dial, &sample, saved.display_clock.show_seconds);
        if (s_flip_clock) ui_flip_clock_update(s_flip_clock, &sample, false);
        if (!strcmp(argv[3], "--bmp-regression")) sim_screenshot_regression(s_overlay, argc > 7 ? argv[7] : NULL);
        else snapshot(argv[3], width, height);
    }
    printf("Clock protection %dx%d: digital/analog/flip sizing, formats, fixed centre, themes, movement, timing, wake and cleanup passed\n", width, height);
    lv_deinit(); return 0;
}
