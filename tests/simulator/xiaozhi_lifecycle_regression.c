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
#include "ui/ui_transition.h"
#include "xiaozhi/xiaozhi_emoji.h"
#include "xiaozhi/xiaozhi_spectrum.h"

/* Fail in the test runner instead of opening the Windows CRT assertion dialog. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "Xiaozhi check %d: %s\n", __LINE__, #condition); fflush(stderr); _Exit(1); \
} } while (0)

static bool enabled;
static bool snapshot_busy;
static const char *reject_command;
static xiaozhi_status_t mock;
static unsigned wakeups;
static const theme_palette_t palette = {.content_bg=0x101820,.text_primary=0xeeeeee,
    .nav_btn_bg_active=0x365f74,.nav_tab_active=0xfdd835};
const theme_palette_t *theme_palette_active(void) { return &palette; }
ui_style_kind_t ui_style_get(void) { return UI_STYLE_MATTE; }
void display_note_activity(void) { ++wakeups; }
int ui_fonts_role_px(ui_text_role_t role) { (void)role; return 48; }
const lv_font_t *ui_font_emoji_for_text(int px) { (void)px; return LV_FONT_DEFAULT; }
const lv_font_t *ui_font_emotion(int height) { (void)height; return LV_FONT_DEFAULT; }
const lv_font_t *ui_font_reply(const lv_font_t *base) { return base; }
void ui_popup_style_frame(lv_obj_t *card) { (void)card; }
lv_obj_t *ui_popup_card_create(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card, lv_color_hex(palette.content_bg), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(card, false);
    return card;
}
lv_obj_t *ui_popup_label_create(lv_obj_t *parent, const char *text, ui_popup_label_role_t role)
{
    (void)role;
    lv_obj_t *label = lv_label_create(parent); lv_label_set_text(label, text);
    return label;
}
void ui_style_action_button(lv_obj_t *button, lv_obj_t *label, ui_control_role_t role, bool available)
{ (void)button; (void)label; (void)role; (void)available; }
void xiaozhi_spectrum_snapshot(xiaozhi_spectrum_snapshot_t *out)
{
    memcpy(out->microphone, mock.spectrum_mic, sizeof(out->microphone));
    memcpy(out->speaker, mock.spectrum_speaker, sizeof(out->speaker));
    out->mic_active = mock.spectrum_mic_active;
    out->speaker_active = mock.spectrum_speaker_active;
}
lv_obj_t *ui_popup_close_button(lv_obj_t *parent, lv_color_t color, lv_event_cb_t cb, void *data)
{
    (void)color; lv_obj_t *b=lv_button_create(parent);lv_obj_set_size(b,48,48);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,data);return b;
}
static unsigned disconnects, opens, starts, stops;
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
const char *ui_i18n_get(const char *key, const char *fallback)
{
    return !strcmp(key, "xiaozhi.state.speaking_listening") ? "Speaking and listening" : fallback;
}
void xiaozhi_status(xiaozhi_status_t *s)
{
    *s=mock; s->supported = true; s->enabled = enabled;
    if (!enabled || !s->state[0]) strcpy(s->state, enabled ? "ready" : "disabled");
}
bool xiaozhi_status_try(xiaozhi_status_t *s)
{ if (snapshot_busy) return false; xiaozhi_status(s); return true; }
static void mock_open(bool voice)
{
    ++mock.dialog_seq; mock.dialog_active = true; mock.dialog_voice_opened = voice;
    mock.silence_remaining_ms = 30000; mock.dialog_timeout = 30;
    mock.dialog_end_reason[0] = 0;
}
static void mock_end(const char *reason)
{
    mock.dialog_active = false; mock.dialog_end_seq = mock.dialog_seq;
    mock.silence_remaining_ms = 0;
    snprintf(mock.dialog_end_reason, sizeof(mock.dialog_end_reason), "%s", reason);
}
static void reset_mock(void)
{
    /* Production generations survive settings and view rebuilds for this boot. */
    uint32_t sequence = mock.dialog_seq;
    memset(&mock, 0, sizeof(mock)); mock.dialog_seq = sequence;
}
esp_err_t xiaozhi_command(const char *action, int value)
{
    if (reject_command && !strcmp(action, reject_command)) return ESP_ERR_TIMEOUT;
    if (!strcmp(action,"open")) ++opens;
    else if (!strcmp(action,"dialog_turn")) {
        assert((uint32_t)value == mock.dialog_seq && !mock.dialog_active); mock_open(false);
    }
    else if (!strcmp(action,"dismiss")) {
        assert((uint32_t)value == mock.dialog_seq);
        ++disconnects; mock_end("manual");
    }
    else if (!strcmp(action,"start_turn")) { assert((uint32_t)value == mock.dialog_seq); ++starts; }
    else if (!strcmp(action,"stop_turn")) { assert((uint32_t)value == mock.dialog_seq); ++stops; }
    else assert(!"Unexpected assistant command");
    return ESP_OK;
}
static unsigned timer_count(void)
{
    unsigned n = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++n;
    return n;
}
static void assert_header_active(lv_obj_t *button, bool active)
{
    assert(lv_obj_has_state(button, LV_STATE_CHECKED) == active);
    assert(lv_obj_has_state(lv_obj_get_child(button, 0), LV_STATE_CHECKED) == active);
}

static void count_style_change(lv_event_t *event)
{
    unsigned *count = lv_event_get_user_data(event);
    ++*count;
}

static void spectrum_drawn(lv_event_t *event)
{
    unsigned *cells = lv_event_get_user_data(event);
    lv_draw_task_t *task = lv_event_get_draw_task(event);
    assert(task && lv_draw_task_get_type(task) == LV_DRAW_TASK_TYPE_FILL);
    lv_area_t cell, bounds;
    lv_draw_task_get_area(task, &cell);
    lv_obj_get_coords(lv_event_get_target_obj(event), &bounds);
    assert(cell.x1 >= bounds.x1 && cell.x2 <= bounds.x2);
    assert(cell.y1 >= bounds.y1 && cell.y2 <= bounds.y2);
    ++*cells;
}

static void assert_spectrum_draw_bounds(lv_obj_t *spectrum)
{
    /* Inspect the production renderer's actual rectangles before clipping.
     * The cell count also catches fully clipped cells omitted by LVGL. */
    const int heights[] = {44,45,46,47,48,49,50,51,52,61,70,79,96};
    int original_height = lv_obj_get_height(spectrum);
    unsigned cells = 0;
    lv_obj_set_send_draw_task_events(spectrum, true);
    lv_obj_add_event_cb(spectrum, spectrum_drawn, LV_EVENT_DRAW_TASK_ADDED, &cells);
    for (unsigned i = 0; i < sizeof(heights) / sizeof(heights[0]); ++i) {
        lv_obj_set_height(spectrum, heights[i]); lv_obj_update_layout(spectrum);
        cells = 0;
        lv_draw_buf_t *shot = lv_snapshot_take(spectrum, LV_COLOR_FORMAT_RGB888);
        assert(shot);
        assert(cells == XIAOZHI_SPECTRUM_BANDS * 9);
        lv_draw_buf_destroy(shot);
    }
    lv_obj_remove_event_cb(spectrum, spectrum_drawn);
    lv_obj_set_send_draw_task_events(spectrum, false);
    lv_obj_set_height(spectrum, original_height);
}

static void count_spectrum_level(lv_event_t *event)
{
    unsigned *counts = lv_event_get_user_data(event);
    lv_draw_fill_dsc_t *fill = lv_draw_task_get_fill_dsc(lv_event_get_draw_task(event));
    assert(fill);
    counts[0]++;
    if (fill->opa > LV_OPA_30) counts[1]++;
}

static void assert_spectrum_level(lv_obj_t *spectrum, unsigned cells, unsigned bright)
{
    unsigned counts[2] = {0};
    lv_obj_set_send_draw_task_events(spectrum,true);
    lv_obj_add_event_cb(spectrum,count_spectrum_level,LV_EVENT_DRAW_TASK_ADDED,counts);
    lv_draw_buf_t *shot=lv_snapshot_take(spectrum,LV_COLOR_FORMAT_RGB888);
    assert(shot);lv_draw_buf_destroy(shot);
    lv_obj_remove_event_cb(spectrum,count_spectrum_level);
    lv_obj_set_send_draw_task_events(spectrum,false);
    assert(counts[0]==cells&&counts[1]==bright);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(1280, 800);
    /* Refresh is explicit; snapshots below exercise real spectrum drawing. */
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
        assert(disconnects == 0); /* hidden views cannot cancel another conversation */
        assert(timer_count() == baseline);
        lv_tick_inc(300); lv_timer_handler();
    }
    lv_obj_t *other = lv_obj_create(NULL);
    xiaozhi_ui_create(other);
    lv_obj_t *root = lv_obj_get_child(other, 0);
    lv_obj_t *button = lv_obj_get_child(root, 0), *panel = lv_obj_get_child(root, 1);
    assert(lv_obj_is_hidden(panel));
    assert_header_active(button, false);
    lv_color_t idle_bg = lv_obj_get_style_bg_color(button, LV_PART_MAIN);
    lv_color_t idle_glyph = lv_obj_get_style_text_color(lv_obj_get_child(button, 0), LV_PART_MAIN);
    lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
    lv_tick_inc(300); lv_timer_handler();
    assert(!lv_obj_is_hidden(panel));
    assert_header_active(button, true);
    assert(lv_color_eq(lv_obj_get_style_bg_color(button, LV_PART_MAIN), lv_color_hex(palette.nav_btn_bg_active)));
    assert(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(button, 0), LV_PART_MAIN), lv_color_hex(palette.nav_tab_active)));
    lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
    assert(lv_obj_is_hidden(panel) && disconnects == 1);
    assert_header_active(button, false);
    assert(lv_color_eq(lv_obj_get_style_bg_color(button, LV_PART_MAIN), idle_bg));
    assert(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(button, 0), LV_PART_MAIN), idle_glyph));
    lv_obj_delete(other);
    assert(disconnects == 1 && timer_count() == baseline);
    const int sizes[][2]={{480,480},{720,720},{1280,800},{800,1280},{720,1280}};
    for (unsigned i=0;i<5;++i) {
        lv_display_set_resolution(display,sizes[i][0],sizes[i][1]);
        enabled=true;reset_mock();mock.dialog_timeout=30;
        xiaozhi_ui_create(screen);
        root=lv_obj_get_child(screen,0);panel=lv_obj_get_child(root,1);
        button=lv_obj_get_child(root,0);
        assert_header_active(button, false);
        lv_obj_t *talk=lv_obj_get_child(panel,4), *chat=lv_obj_get_child(panel,3);
        mock.wake_enabled=mock.wake_ready=true;
        unsigned prior_opens=opens, prior_starts=starts, prior_stops=stops;
        lv_tick_inc(300);lv_timer_handler();
        /* Clicking the header is manual even with a ready wake detector. */
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(!lv_obj_is_hidden(panel)&&!lv_obj_is_hidden(talk));
        assert_header_active(button, true);
        assert(opens==prior_opens&&starts==prior_starts);
        assert(lv_obj_get_y(chat)+lv_obj_get_height(chat)<=lv_obj_get_y(talk));
        lv_obj_send_event(talk,LV_EVENT_PRESSED,NULL);assert(starts==prior_starts+1);
        lv_obj_send_event(talk,LV_EVENT_RELEASED,NULL);assert(stops==prior_stops+1);
        lv_obj_send_event(talk,LV_EVENT_PRESSED,NULL);
        lv_obj_send_event(talk,LV_EVENT_PRESS_LOST,NULL);
        assert(starts==prior_starts+2&&stops==prior_stops+2);
        /* Queue contention must not hide active capture or lose a release. */
        reject_command="dismiss";
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);
        assert(!lv_obj_is_hidden(panel)&&mock.dialog_active);
        reject_command=NULL;
        lv_obj_send_event(talk,LV_EVENT_PRESSED,NULL);
        unsigned before_busy_stop=stops;
        reject_command="stop_turn";
        lv_obj_send_event(talk,LV_EVENT_RELEASED,NULL);
        lv_tick_inc(300);lv_timer_handler();assert(stops==before_busy_stop);
        reject_command=NULL;
        lv_tick_inc(300);lv_timer_handler();assert(stops==before_busy_stop+1);
        /* A release from an old hold cannot stop a newly opened voice turn. */
        lv_obj_send_event(talk,LV_EVENT_PRESSED,NULL);
        unsigned prior_release=stops;
        mock_open(true);lv_tick_inc(300);lv_timer_handler();
        lv_obj_send_event(talk,LV_EVENT_RELEASED,NULL);assert(stops==prior_release);
        /* A failure before the first active snapshot must still open recovery
         * guidance. A quick retry tap must not start capture after release. */
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);assert(lv_obj_is_hidden(panel));
        mock_open(false);mock_end("error");strcpy(mock.state,"connection_error");
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(!lv_obj_is_hidden(panel)&&!lv_obj_is_hidden(talk));
        lv_obj_t *manual_hint=lv_obj_get_child(panel,7);
        assert(!lv_obj_is_hidden(manual_hint)&&!strcmp(lv_label_get_text(manual_hint),"Hold to talk to retry"));
        assert(lv_obj_get_y(manual_hint)+lv_obj_get_height(manual_hint)<=lv_obj_get_y(talk));
        unsigned before_retry=starts;
        lv_obj_send_event(talk,LV_EVENT_PRESSED,NULL);
        lv_obj_send_event(talk,LV_EVENT_RELEASED,NULL);
        lv_tick_inc(300);lv_timer_handler();assert(starts==before_retry);
        strcpy(mock.state,"ready");lv_tick_inc(300);lv_timer_handler();
        mock.wake_ready=false;lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(talk));
        mock.wake_ready=true;lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(talk));
        mock.wake_enabled=false;lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(talk));
        mock.wake_enabled=true;lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(talk));
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);
        assert(lv_obj_is_hidden(panel)&&timer_count()==baseline+1);
        assert_header_active(button, false);
        mock_open(true);
        strcpy(mock.emotion,"happy");strcpy(mock.recognized,"hello");strcpy(mock.reply,"welcome");
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(!lv_obj_is_hidden(panel));assert(wakeups>0);
        assert_header_active(button, true);
        lv_obj_t *emoji=lv_obj_get_child(panel,2);
        assert(!strcmp(lv_label_get_text(emoji),xiaozhi_emotion_glyph("happy")));
        assert(lv_obj_is_hidden(talk));
        assert(lv_obj_get_y(chat)>lv_obj_get_y(emoji)+lv_obj_get_height(emoji));
        assert(lv_obj_get_y(chat)+lv_obj_get_height(chat)<=lv_obj_get_content_height(panel));
        lv_obj_t *question=lv_obj_get_child(lv_obj_get_child(chat,0),0);
        lv_obj_t *answer=lv_obj_get_child(lv_obj_get_child(chat,1),0);
        assert(lv_obj_get_x(question)>lv_obj_get_x(answer));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(question,0)),"You"));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(answer,0)),"AI Assistant"));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(question,1)),"hello"));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(answer,1)),"welcome"));
        lv_obj_t *spectrum=lv_obj_get_child(panel,6);
        assert(lv_obj_get_child_count(spectrum)==0); /* one bounded drawing surface */
        assert(lv_obj_get_y(chat)+lv_obj_get_height(chat)<=lv_obj_get_y(spectrum));
        assert(lv_obj_get_y(spectrum)+lv_obj_get_height(spectrum)<=lv_obj_get_content_height(panel));
        /* Full-duplex status must update while the voice-opened view keeps its
         * controls and content geometry; close/timeout below still work. */
        mock.duplex_enabled=mock.duplex_active=mock.listening=mock.speaking=true;
        strcpy(mock.state,"speaking_listening");
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        lv_obj_t *state_label=lv_obj_get_child(panel,0);
        assert(!strcmp(lv_label_get_text(state_label),"Speaking and listening"));
        assert(lv_obj_get_width(state_label)<=lv_obj_get_content_width(panel));
        assert(lv_obj_get_y(state_label)+lv_obj_get_height(state_label)<lv_obj_get_y(chat));
        assert(lv_obj_is_hidden(talk)&&!lv_obj_is_hidden(panel));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(panel,7)),"I'm speaking and listening"));
        unsigned before_status=disconnects;
        mock.spectrum_speaker_active=true;
        memset(mock.spectrum_speaker,255,sizeof(mock.spectrum_speaker));
        mock.spectrum_mic_active=true;mock.spectrum_mic[3]=170;
        lv_tick_inc(100);lv_timer_handler();
        assert(timer_count()==baseline+2&&lv_obj_get_child_count(spectrum)==0);
        assert_spectrum_draw_bounds(spectrum);
        mock.spectrum_mic_active=mock.spectrum_speaker_active=false;
        mock.listening=mock.speaking=mock.conversation=false;
        strcpy(mock.state,"ready");lv_tick_inc(300);lv_timer_handler();
        assert(!strcmp(lv_label_get_text(state_label),"ready")&&disconnects==before_status);
        strcpy(mock.state,"connection_error");lv_tick_inc(300);lv_timer_handler();
        assert(!strcmp(lv_label_get_text(state_label),"connection_error"));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(panel,7)),"Wake me again to retry"));
        assert(disconnects==before_status); /* presentation does not reconnect or abort */
        strcpy(mock.state,"audio_or_connection_error");lv_tick_inc(300);lv_timer_handler();
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(panel,7)),"Wake me again to retry"));
        assert(lv_color_eq(lv_obj_get_style_text_color(state_label,0),lv_color_hex(palette.error)));
        memset(mock.reply,'W',sizeof(mock.reply)-1);mock.reply[sizeof(mock.reply)-1]=0;
        for (unsigned line=1;line<sizeof(mock.reply)-1;line+=2) mock.reply[line]='\n';
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(lv_obj_get_scroll_bottom(chat)>0);
        assert(lv_obj_get_y(chat)+lv_obj_get_height(chat)<=lv_obj_get_y(spectrum));
        assert(lv_obj_get_width(answer)<=lv_obj_get_width(chat));
        strcpy(mock.reply,"welcome");strcpy(mock.state,"listening");mock.listening=true;
        lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(!strcmp(lv_label_get_text(state_label),"listening"));
        assert(lv_color_eq(lv_obj_get_style_bg_color(panel,0),lv_color_hex(palette.content_bg)));
        lv_obj_t *otto=lv_obj_get_child(panel,8);
        assert(lv_obj_check_type(otto,&lv_gif_class)&&lv_gif_is_loaded(otto));
        assert(lv_obj_is_hidden(emoji)&&lv_gif_get_frame_count(otto)>1);
        lv_tick_inc(100);lv_timer_handler();assert(lv_gif_get_current_frame_index(otto)>0);
        mock.emoji_style=1;lv_tick_inc(300);lv_timer_handler();
        assert(lv_obj_get_child_count(panel)==8&&!lv_obj_is_hidden(emoji));
        assert(timer_count()==baseline+1);
        mock.emoji_style=0;lv_tick_inc(300);lv_timer_handler();
        assert(lv_obj_get_child_count(panel)==9&&lv_obj_is_hidden(emoji));
        lv_obj_t *hint_label=lv_obj_get_child(panel,7);
        mock.silence_remaining_ms=5001;lv_tick_inc(300);lv_timer_handler();
        assert(!strcmp(lv_label_get_text(hint_label),"Go ahead, I'm listening"));
        mock.silence_remaining_ms=5000;lv_tick_inc(300);lv_timer_handler();
        assert(strstr(lv_label_get_text(hint_label),"5 s"));
        mock.silence_remaining_ms=1;lv_tick_inc(300);lv_timer_handler();
        assert(strstr(lv_label_get_text(hint_label),"1 s"));
        assert(!lv_obj_is_hidden(panel));
        mock.silence_remaining_ms=30000;++mock.activity_seq;lv_tick_inc(300);lv_timer_handler();
        assert(!strcmp(lv_label_get_text(hint_label),"Go ahead, I'm listening"));
        /* Time passing on the UI is never an independent session deadline. */
        lv_tick_inc(40000);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        unsigned before_end=disconnects;
        mock_end("silence");lv_tick_inc(300);lv_timer_handler();assert(lv_obj_is_hidden(panel));
        assert(disconnects==before_end); /* backend close must not enqueue a second stop */
        assert_header_active(button, false);
        mock_open(true);lv_tick_inc(300);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        mock_end("farewell");mock_open(true);
        strcpy(mock.dialog_end_reason,"farewell"); /* old end generation survives a fast wake */
        lv_tick_inc(300);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        assert(disconnects==before_end);
        mock_end("error");strcpy(mock.state,"connection_error");
        lv_tick_inc(300);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        assert(!strcmp(lv_label_get_text(hint_label),"Wake me again to retry"));
        lv_tick_inc(40000);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
        mock_open(true);strcpy(mock.state,"listening");lv_tick_inc(300);lv_timer_handler();
        mock_end("farewell");lv_tick_inc(300);lv_timer_handler();assert(lv_obj_is_hidden(panel));
        assert(disconnects==before_end);
        /* A voice-opened window must not leave the next header open hands-free. */
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);
        lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(panel)&&!lv_obj_is_hidden(talk));
        mock.silence_remaining_ms=4500;lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
        assert(!lv_obj_is_hidden(hint_label)&&!lv_obj_is_hidden(talk));
        assert(strstr(lv_label_get_text(hint_label),"Hold to talk to continue"));
        assert(lv_obj_get_y(hint_label)+lv_obj_get_height(hint_label)<=lv_obj_get_y(talk));
        assert(lv_obj_get_y(spectrum)+lv_obj_get_height(spectrum)<=lv_obj_get_y(hint_label));
        mock_open(true);lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(panel)&&lv_obj_is_hidden(talk));
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);
        assert(lv_obj_is_hidden(panel));
        mock.wake_ready=false;mock_open(true);lv_tick_inc(300);lv_timer_handler();
        assert(!lv_obj_is_hidden(panel)&&!lv_obj_is_hidden(talk));
        lv_obj_send_event(lv_obj_get_child(panel,1),LV_EVENT_CLICKED,NULL);assert(lv_obj_is_hidden(panel));
        assert_header_active(button, false);
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);lv_tick_inc(300);lv_timer_handler();assert_header_active(button, true);
        enabled=false;mock_end("disabled");lv_tick_inc(300);lv_timer_handler();
        assert(lv_obj_is_hidden(panel));assert_header_active(button, false);
        enabled=true;lv_tick_inc(300);lv_timer_handler();
        assert(lv_obj_is_hidden(panel));assert_header_active(button, false);
        lv_obj_clean(screen);assert(timer_count()==baseline);
    }
    assert(!strcmp(xiaozhi_emotion_glyph("unexpected"),xiaozhi_emotion_glyph("neutral")));
    assert(!xiaozhi_dialog_expired(9999,0,10));assert(xiaozhi_dialog_expired(10000,0,10));
    /* Real reusable dialog: fade, rapid header reversal, timeout, disable and
     * rebuild. Closing must disconnect once and reject a late Talk press. */
    for (unsigned style = 0; style < 2; style++) {
        ui_transition_configure(true);
        enabled = true; reset_mock();
        mock.emoji_style = style; mock.dialog_timeout = 30;
        xiaozhi_ui_create(screen);
        root = lv_obj_get_child(screen, 0);
        button = lv_obj_get_child(root, 0); panel = lv_obj_get_child(root, 1);
        lv_obj_t *talk = lv_obj_get_child(panel, 4);
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        lv_tick_inc(300); lv_timer_handler();
        assert(lv_obj_get_style_opa(panel, 0) == 0);
        lv_tick_inc(200); lv_timer_handler();
        assert(lv_obj_get_style_opa(panel, 0) == 255);
        unsigned before = disconnects, prior = starts;
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        assert(ui_transition_is_closing(panel) && !lv_obj_is_hidden(panel));
        lv_obj_send_event(talk, LV_EVENT_PRESSED, NULL); assert(starts == prior);
        assert(disconnects == before + 1);
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL); /* reopen during fade */
        lv_tick_inc(300); lv_timer_handler();
        lv_tick_inc(200); lv_timer_handler();
        assert(!lv_obj_is_hidden(panel) && !ui_transition_is_closing(panel));
        before=disconnects;mock_end("silence");lv_tick_inc(300); lv_timer_handler();
        assert(ui_transition_is_closing(panel));
        assert(disconnects==before);
        lv_tick_inc(200); lv_timer_handler(); assert(lv_obj_is_hidden(panel));
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        enabled = false; mock_end("disabled");lv_tick_inc(300); lv_timer_handler();
        ui_transition_configure(false); assert(lv_obj_is_hidden(panel));
        enabled = true; lv_tick_inc(300); lv_timer_handler();
        ui_transition_configure(true);
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        lv_tick_inc(300);lv_timer_handler();
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        lv_obj_clean(screen); lv_tick_inc(300); lv_timer_handler();
        assert(timer_count() == baseline);
    }
    ui_transition_configure(false);
    /* A contended first read must not permanently remove a supported action.
     * Later contention retains the last text and cannot invent an idle expiry. */
    enabled=true;reset_mock();mock.dialog_timeout=30;mock.emoji_style=1;
    snapshot_busy=true;xiaozhi_ui_create(screen);assert(timer_count()==baseline+1);
    root=lv_obj_get_child(screen,0);button=lv_obj_get_child(root,0);panel=lv_obj_get_child(root,1);
    snapshot_busy=false;lv_tick_inc(300);lv_timer_handler();assert(!lv_obj_is_hidden(button));
    lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);lv_tick_inc(300);lv_timer_handler();
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(panel,0)),"ready"));
    snapshot_busy=true;strcpy(mock.state,"connection_error");
    lv_tick_inc(300);lv_timer_handler();
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(panel,0)),"ready"));
    lv_tick_inc(31000);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
    mock_end("silence");snapshot_busy=false;lv_tick_inc(300);lv_timer_handler();assert(lv_obj_is_hidden(panel));
    lv_obj_clean(screen);assert(timer_count()==baseline);
    /* An existing active session remains reachable after a view rebuild. */
    mock_open(true);strcpy(mock.state,"ready");
    xiaozhi_ui_create(screen);lv_tick_inc(300);lv_timer_handler();
    root=lv_obj_get_child(screen,0);panel=lv_obj_get_child(root,1);
    assert(!lv_obj_is_hidden(panel));
    lv_obj_clean(screen);assert(timer_count()==baseline&&!mock.dialog_active);
    /* Rebuilds also retain a current failure that happened before any view
     * observed an active conversation. Closing it is sticky during ack delay. */
    mock_open(true);mock_end("error");strcpy(mock.state,"audio_or_connection_error");
    mock.wake_enabled=mock.wake_ready=true;
    xiaozhi_ui_create(screen);lv_tick_inc(300);lv_timer_handler();
    root=lv_obj_get_child(screen,0);panel=lv_obj_get_child(root,1);button=lv_obj_get_child(root,0);
    assert(!lv_obj_is_hidden(panel)&&lv_obj_is_hidden(lv_obj_get_child(panel,4)));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(panel,7)),"Wake me again to retry"));
    lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);assert(lv_obj_is_hidden(panel));
    mock_end("error"); /* Simulate the old snapshot until the queued close runs. */
    lv_obj_clean(screen);xiaozhi_ui_create(screen);lv_tick_inc(300);lv_timer_handler();
    root=lv_obj_get_child(screen,0);panel=lv_obj_get_child(root,1);
    assert(lv_obj_is_hidden(panel));
    mock_end("manual");strcpy(mock.state,"ready");lv_tick_inc(300);lv_timer_handler();
    assert(lv_obj_is_hidden(panel));
    mock_open(true);lv_tick_inc(300);lv_timer_handler();assert(!lv_obj_is_hidden(panel));
    lv_obj_clean(screen);assert(timer_count()==baseline);
    /* Ten virtual minutes with an unchanged long answer must not relayout
     * text at the 4 Hz status cadence, or reset the reader's scroll position. */
    reset_mock();mock_open(true);mock.emoji_style=1;
    mock.wake_enabled=mock.wake_ready=true;strcpy(mock.state,"speaking_listening");
    memset(mock.reply,'W',sizeof(mock.reply)-1);
    for (unsigned i=1;i<sizeof(mock.reply)-1;i+=2) mock.reply[i]='\n';
    xiaozhi_ui_create(screen);lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
    root=lv_obj_get_child(screen,0);panel=lv_obj_get_child(root,1);
    lv_obj_t *long_chat=lv_obj_get_child(panel,3);
    lv_obj_scroll_to_y(long_chat,80,LV_ANIM_OFF);
    lv_tick_inc(300);lv_timer_handler();lv_obj_update_layout(screen);
    int scroll=lv_obj_get_scroll_y(long_chat);
    assert(scroll>0);
    unsigned style_changes=0;
    lv_obj_t *long_answer=lv_obj_get_child(lv_obj_get_child(long_chat,1),0);
    lv_obj_t *long_body=lv_obj_get_child(long_answer,1);
    lv_obj_add_event_cb(long_body,count_style_change,LV_EVENT_STYLE_CHANGED,&style_changes);
    for (unsigned i=0;i<12000;i++) {lv_tick_inc(50);lv_timer_handler();}
    printf("Unchanged answer: %u style events in 600 virtual seconds\n",style_changes);
    assert(style_changes==0&&lv_obj_get_scroll_y(long_chat)==scroll);
    assert(timer_count()==baseline+1&&lv_obj_get_child_count(long_chat)==2);
    lv_obj_t *live_spectrum=lv_obj_get_child(panel,6);
    for (unsigned i=0;i<XIAOZHI_SPECTRUM_BANDS;i++) {
        memset(mock.spectrum_speaker,0,sizeof(mock.spectrum_speaker));
        memset(mock.spectrum_mic,0,sizeof(mock.spectrum_mic));
        mock.spectrum_speaker_active=(i&1)!=0;mock.spectrum_mic_active=true;
        (i&1?mock.spectrum_speaker:mock.spectrum_mic)[i]=255;
        for(unsigned step=0;step<3;step++) {lv_tick_inc(50);lv_timer_handler();}
        assert_spectrum_level(live_spectrum,32,9);
    }
    /* Fresh digital silence is dim too; freshness alone is not loudness. */
    memset(mock.spectrum_speaker,0,sizeof(mock.spectrum_speaker));
    for(unsigned step=0;step<3;step++) {lv_tick_inc(50);lv_timer_handler();}
    assert_spectrum_level(live_spectrum,24,0);
    mock.spectrum_speaker_active=mock.spectrum_mic_active=false;
    lv_tick_inc(50);lv_timer_handler();assert_spectrum_level(live_spectrum,24,0);
    lv_obj_clean(screen);assert(timer_count()==baseline);
    lv_display_delete(display);
    lv_deinit();
    puts("PASS: AI Assistant bubbles, spectrum bounds, authoritative countdown/end generations, error recovery, fast rewake, manual/wake origins, 100 rebuilds, Otto/classic, five geometries and popup fades");
    return 0;
}
