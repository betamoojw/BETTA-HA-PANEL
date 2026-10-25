/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Exercise real LVGL cleanup, timers and the production assistant overlay.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xiaozhi/xiaozhi.h"
#include "ui/text_font.h"
#include "ui/ui_popup.h"
#include "ui/theme/ui_style.h"
#include "xiaozhi/xiaozhi_dialog.h"
#include "xiaozhi/xiaozhi_emoji.h"

/* Fail in the test runner instead of opening the Windows CRT assertion dialog. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "Xiaozhi check %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static bool enabled;
static xiaozhi_status_t mock;
static unsigned wakeups;
static const theme_palette_t palette = {.content_bg=0x101820,.text_primary=0xeeeeee};
const theme_palette_t *theme_palette_active(void) { return &palette; }
ui_style_kind_t ui_style_get(void) { return UI_STYLE_MATTE; }
void display_note_activity(void) { ++wakeups; }
int ui_fonts_role_px(ui_text_role_t role) { (void)role; return 48; }
const lv_font_t *ui_font_emoji_for_text(int px) { (void)px; return LV_FONT_DEFAULT; }
const lv_font_t *ui_font_emotion(int height) { (void)height; return LV_FONT_DEFAULT; }
void ui_popup_style_frame(lv_obj_t *card) { (void)card; }
lv_obj_t *ui_popup_close_button(lv_obj_t *parent, lv_color_t color, lv_event_cb_t cb, void *data)
{
    (void)color; lv_obj_t *b=lv_button_create(parent);lv_obj_set_size(b,48,48);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,data);return b;
}
static unsigned disconnects;
lv_obj_t *ui_pages_create_topbar_action(lv_obj_t *parent, const char *symbol,
    lv_event_cb_t clicked, void *user_data)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_t *label = lv_label_create(button); lv_label_set_text(label, symbol);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, user_data);
    return button;
}
void ui_pages_set_topbar_auxiliary(lv_obj_t *button) { assert(button != NULL); }
const lv_font_t *ui_font_role(ui_text_role_t role) { (void)role; return LV_FONT_DEFAULT; }
const char *ui_i18n_get(const char *key, const char *fallback) { (void)key; return fallback; }
void xiaozhi_status(xiaozhi_status_t *s)
{
    *s=mock; s->supported = true; s->enabled = enabled;
    strcpy(s->state, enabled ? "ready" : "disabled");
}
esp_err_t xiaozhi_command(const char *action, int value)
{
    (void)value; assert(!strcmp(action,"open") || !strcmp(action,"disconnect")); if (!strcmp(action,"disconnect")) ++disconnects; return ESP_OK;
}
static unsigned timer_count(void)
{
    unsigned n = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++n;
    return n;
}
int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(1280, 800);
    /* No pixels are needed: only real object lifetime and timer dispatch. */
    lv_timer_pause(lv_display_get_refr_timer(display));
    unsigned baseline = timer_count();
    lv_obj_t *screen = lv_screen_active();
    const char *emotions[] = {"neutral", "happy", "laughing", "funny", "sad", "angry", "crying",
        "loving", "embarrassed", "surprised", "shocked", "thinking", "winking", "cool",
        "relaxed", "delicious", "kissy", "confident", "sleepy", "silly", "confused"};
    lv_obj_t *asset_test = lv_gif_create(screen);
    for (unsigned i = 0; i < sizeof(emotions) / sizeof(emotions[0]); ++i) {
        lv_gif_set_src(asset_test, xiaozhi_otto_asset(emotions[i]));
        assert(lv_gif_is_loaded(asset_test) && lv_gif_get_frame_count(asset_test) >= 30);
        lv_tick_inc(250); lv_timer_handler();
        assert(lv_gif_get_current_frame_index(asset_test) > 0);
    }
    assert(xiaozhi_otto_asset("unknown") == xiaozhi_otto_asset("neutral"));
    lv_obj_delete(asset_test); assert(timer_count() == baseline);
    for (unsigned i = 0; i < 100; ++i) {
        enabled = (i % 2) != 0;
        xiaozhi_ui_create(screen);
        assert(timer_count() == baseline + 1);
        lv_tick_inc(300); lv_timer_handler();
        lv_obj_clean(screen);
        assert(disconnects == i + 1);
        assert(timer_count() == baseline);
        lv_tick_inc(300); lv_timer_handler();
    }
    lv_obj_t *other = lv_obj_create(NULL);
    xiaozhi_ui_create(other);
    lv_obj_t *root = lv_obj_get_child(other, 0);
    lv_obj_t *button = lv_obj_get_child(root, 0), *panel = lv_obj_get_child(root, 1);
    assert(lv_obj_is_hidden(panel));
    lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
    assert(!lv_obj_is_hidden(panel));
    lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
    assert(lv_obj_is_hidden(panel) && disconnects == 101);
    lv_obj_delete(other);
    assert(disconnects == 102 && timer_count() == baseline);
    const int sizes[][2]={{480,480},{720,720},{1280,800},{800,1280},{720,1280}};
    for (unsigned i=0;i<5;++i) {
        lv_display_set_resolution(display,sizes[i][0],sizes[i][1]);
        enabled=true;memset(&mock,0,sizeof(mock));mock.dialog_timeout=10;
        xiaozhi_ui_create(screen);
        root=lv_obj_get_child(screen,0);panel=lv_obj_get_child(root,1);
        ++mock.dialog_seq;mock.wake_enabled=mock.wake_ready=true;
        strcpy(mock.emotion,"happy");strcpy(mock.recognized,"hello");strcpy(mock.reply,"welcome");
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(!lv_obj_is_hidden(panel));assert(wakeups>0);
        lv_obj_t *emoji=lv_obj_get_child(panel,2), *chat=lv_obj_get_child(panel,3), *talk=lv_obj_get_child(panel,4);
        assert(!strcmp(lv_label_get_text(emoji),xiaozhi_emotion_glyph("happy")));
        assert(lv_obj_is_hidden(talk));
        assert(lv_obj_get_y(chat)>lv_obj_get_y(emoji)+lv_obj_get_height(emoji));
        assert(lv_obj_get_y(chat)+lv_obj_get_height(chat)<=lv_obj_get_content_height(panel));
        assert(lv_color_eq(lv_obj_get_style_bg_color(panel,0),lv_color_hex(palette.content_bg)));
        lv_obj_t *otto=lv_obj_get_child(panel,5);
        assert(lv_obj_check_type(otto,&lv_gif_class)&&lv_gif_is_loaded(otto));
        assert(lv_obj_is_hidden(emoji)&&lv_gif_get_frame_count(otto)>1);
        lv_tick_inc(100);lv_timer_handler();assert(lv_gif_get_current_frame_index(otto)>0);
        mock.emoji_style=1;lv_tick_inc(300);lv_timer_handler();
        assert(lv_obj_get_child_count(panel)==5&&!lv_obj_is_hidden(emoji));
        assert(timer_count()==baseline+1);
        mock.emoji_style=0;lv_tick_inc(300);lv_timer_handler();
        assert(lv_obj_get_child_count(panel)==6&&lv_obj_is_hidden(emoji));
        lv_tick_inc(9000);++mock.activity_seq;lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        lv_tick_inc(9000);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        lv_tick_inc(1100);lv_timer_handler();assert(lv_obj_is_hidden(panel));
        mock.wake_ready=false;++mock.dialog_seq;lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(panel)&&!lv_obj_is_hidden(talk));
        lv_obj_send_event(lv_obj_get_child(panel,1),LV_EVENT_CLICKED,NULL);assert(lv_obj_is_hidden(panel));
        lv_obj_clean(screen);assert(timer_count()==baseline);
    }
    assert(!strcmp(xiaozhi_emotion_glyph("unexpected"),xiaozhi_emotion_glyph("neutral")));
    assert(!xiaozhi_dialog_expired(9999,0,10));assert(xiaozhi_dialog_expired(10000,0,10));
    lv_display_delete(display);
    lv_deinit();
    puts("PASS: 100 rebuilds, Otto animation/classic switching, timeout, cleanup and five geometries");
    return 0;
}
