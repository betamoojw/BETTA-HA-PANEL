/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * The view owns its timer and drawing. Audio workers only publish snapshots.
 */
#include "xiaozhi.h"
#include "xiaozhi_dialog.h"
#include "xiaozhi_emoji.h"
#include "xiaozhi_spectrum.h"
#include "ui/text_font.h"
#include "ui/ui_i18n.h"
#include "ui/ui_pages.h"
#include "ui/ui_popup.h"
#include "ui/ui_transition.h"
#include "ui/fonts/app_symbols.h"
#include "ui/theme/ui_typography.h"
#include "ui/theme/ui_style.h"
#include "ui/theme/theme_default.h"
#include "drivers/display_init.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *button, *panel, *state, *emoji, *recognized, *reply, *chat, *talk;
    lv_obj_t *title, *hint, *spectrum, *question_row, *answer_row, *question, *answer;
    lv_timer_t *timer;
    uint32_t shown_seq, activity_seq, polled_at, press_seq, pending_seq;
    bool voice_opened, dialog_visible, spectrum_active, pressed, pending_talk, pending_stop;
    uint8_t levels[XIAOZHI_SPECTRUM_BANDS];
    xiaozhi_status_t snapshot;
    lv_obj_t *otto;
    const lv_image_dsc_t *otto_asset;
} view_t;
/* A view can be rebuilt before the worker acknowledges an explicit close.
 * Keep that local intent for this boot; a different dialogue may show again. */
static uint32_t dismissed_seq;
static bool dismissed_seq_valid;

static bool hands_free(const view_t *v)
{
    return v->voice_opened && v->snapshot.wake_enabled && v->snapshot.wake_ready;
}
static bool connection_error(const xiaozhi_status_t *s)
{
    return !strcmp(s->state, "connection_error") || !strcmp(s->state, "audio_or_connection_error");
}
static bool dialog_needs_view(const xiaozhi_status_t *s)
{
    /* A synchronous connection failure can end a new dialogue before the UI
     * reads its first active snapshot. Keep that generation's recovery view. */
    if (dismissed_seq_valid && dismissed_seq == s->dialog_seq) return false;
    return s->dialog_active || (s->dialog_end_seq == s->dialog_seq &&
        !strcmp(s->dialog_end_reason, "error") && connection_error(s));
}
static bool quiet_hint(const xiaozhi_status_t *s)
{
    return s->dialog_active && s->silence_remaining_ms > 0 && s->silence_remaining_ms <= 5000 && !connection_error(s);
}

static void set_dialog_visible(view_t *v, bool visible)
{
    if (v->dialog_visible == visible) return;
    v->dialog_visible = visible;
    if (visible) ui_transition_fade_in(v->panel);
    else {
        v->pressed = v->pending_talk = v->pending_stop = false;
        memset(v->levels, 0, sizeof(v->levels)); v->spectrum_active = false;
        lv_obj_invalidate(v->spectrum); ui_transition_hide(v->panel);
    }
    lv_obj_t *glyph = lv_obj_get_child(v->button, 0);
    if (visible) {
        lv_obj_add_state(v->button, LV_STATE_CHECKED); lv_obj_add_state(glyph, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(v->button, LV_STATE_CHECKED); lv_obj_remove_state(glyph, LV_STATE_CHECKED);
    }
}

static void stop_animation(view_t *v)
{
    if (v->otto) lv_obj_delete(v->otto);
    v->otto = NULL; v->otto_asset = NULL;
}

static void hide(view_t *v)
{
    if (!v->dialog_visible) return;
    /* Keep the recording controls visible if the worker queue is temporarily
     * full. The next click can retry without leaving hidden capture running. */
    if (xiaozhi_command("dismiss", (int)v->shown_seq) != ESP_OK) return;
    dismissed_seq = v->shown_seq; dismissed_seq_valid = true;
    set_dialog_visible(v, false);
    if (lv_obj_is_hidden(v->panel)) stop_animation(v);
}

static bool label(lv_obj_t *obj, const char *value)
{
    if (!strcmp(lv_label_get_text(obj), value)) return false;
    lv_label_set_text(obj, value); return true;
}

static lv_obj_t *text_label(lv_obj_t *parent, ui_popup_label_role_t role)
{
    lv_obj_t *obj = ui_popup_label_create(parent, "", role);
    lv_obj_set_width(obj, lv_pct(100)); lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    return obj;
}

static void bubble_width(lv_obj_t *bubble, lv_obj_t *body, int available, int percent)
{
    lv_point_t size;
    lv_text_get_size(&size, lv_label_get_text(body), lv_obj_get_style_text_font(body, 0),
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int padding = lv_obj_get_style_pad_left(bubble, 0) + lv_obj_get_style_pad_right(bubble, 0);
    lv_obj_set_width(bubble, LV_MIN(available * percent / 100, LV_MAX(100, size.x + padding + 2)));
}

static void arrange(view_t *v)
{
    bool manual = !hands_free(v);
    lv_obj_set_hidden(v->talk, !manual);
    bool manual_hint = quiet_hint(&v->snapshot) || connection_error(&v->snapshot);
    lv_obj_set_hidden(v->hint, manual && !manual_hint);
    lv_obj_update_layout(v->panel);
    int width = lv_obj_get_content_width(v->panel), height = lv_obj_get_content_height(v->panel);
    int avatar = LV_MIN(64, LV_MAX(44, height / 7));
    int left = avatar + 14, text_width = LV_MAX(40, width - left - UI_POPUP_CLOSE_SIZE - 12);
    lv_obj_set_pos(v->emoji, 0, 0);
    const lv_font_t *face_font = ui_font_emotion(avatar);
    lv_obj_set_style_text_font(v->emoji, face_font ? face_font : ui_font_role(UI_TEXT_HERO), 0);
    lv_obj_set_pos(v->title, left, 0); lv_obj_set_width(v->title, text_width);
    lv_obj_set_pos(v->state, left, lv_font_get_line_height(ui_font_role(UI_TEXT_LABEL)) + 4);
    lv_obj_set_width(v->state, text_width);
    if (v->otto) {
        lv_obj_set_size(v->otto, avatar, avatar); lv_obj_set_pos(v->otto, 0, 0);
    }
    lv_obj_update_layout(v->panel);
    int header = LV_MAX(avatar, lv_obj_get_y(v->state) + lv_obj_get_height(v->state)) + 18;
    lv_obj_set_width(v->hint, width); lv_obj_update_layout(v->hint);
    int talk_height = LV_MAX(48, lv_obj_get_height(lv_obj_get_child(v->talk, 0)) +
        lv_obj_get_style_pad_top(v->talk, 0) + lv_obj_get_style_pad_bottom(v->talk, 0));
    lv_obj_set_height(v->talk, talk_height);
    int hint_height = !manual || manual_hint ? lv_obj_get_height(v->hint) : 0;
    int spectrum_height = LV_MIN(96, LV_MAX(44, height / 6));
    int footer = spectrum_height + (hint_height ? hint_height + 8 : 0) + (manual ? talk_height + 8 : 0);
    /* The text region owns scrolling; neither the footer nor Talk can be
     * displaced by a long transcript, response or larger text setting. */
    lv_obj_set_pos(v->chat, 0, header);
    lv_obj_set_size(v->chat, width, LV_MAX(24, height - header - footer - 8));
    lv_obj_set_size(v->spectrum, width, spectrum_height);
    lv_obj_set_pos(v->spectrum, 0, height - footer);
    lv_obj_set_width(v->hint, width); lv_obj_align(v->hint, LV_ALIGN_BOTTOM_MID, 0, manual ? -talk_height - 8 : 0);
    lv_obj_align(v->talk, LV_ALIGN_BOTTOM_MID, 0, 0);
    bubble_width(v->question, v->recognized, width - 6, 86);
    bubble_width(v->answer, v->reply, width - 6, 94);
}

static void show(view_t *v, bool voice_opened)
{
    if (dismissed_seq_valid && dismissed_seq != v->snapshot.dialog_seq) dismissed_seq_valid = false;
    v->voice_opened = voice_opened;
    set_dialog_visible(v, true); arrange(v); lv_obj_move_foreground(lv_obj_get_parent(v->panel));
    display_note_activity();
}

static void draw_spectrum(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t box; lv_obj_get_coords(v->spectrum, &box);
    int pitch = LV_MAX(3, LV_MIN(22, lv_area_get_width(&box) / XIAOZHI_SPECTRUM_BANDS));
    int height = lv_area_get_height(&box);
    /* Full scale has nine cells and eight two-pixel gaps. */
    int segment = LV_MAX(2, LV_MIN(7, (height - 16) / 9));
    int bar_width = LV_MAX(2, pitch * 3 / 5);
    int start = box.x1 + (lv_area_get_width(&box) - pitch * XIAOZHI_SPECTRUM_BANDS) / 2;
    int middle = box.y1 + (height - (segment & 1)) / 2;
    lv_draw_rect_dsc_t rect; lv_draw_rect_dsc_init(&rect);
    rect.base.obj = v->spectrum;
    rect.bg_color = lv_color_hex(APP_UI_COLOR_CARD_ICON_ON);
    rect.radius = ui_style_radius(1);
    for (unsigned band = 0; band < XIAOZHI_SPECTRUM_BANDS; ++band) {
        int count = v->levels[band] * 4 / 255;
        for (int row = -count; row <= count; ++row) {
            rect.bg_opa = v->spectrum_active ? LV_OPA_COVER : LV_OPA_30;
            lv_area_t cell = {start + (int)band * pitch, middle + row * (segment + 2) - segment / 2,
                             start + (int)band * pitch + bar_width - 1,
                             middle + row * (segment + 2) - segment / 2 + segment - 1};
            lv_draw_rect(layer, &rect, &cell);
        }
    }
}

static void spectrum_update(view_t *v)
{
    xiaozhi_spectrum_snapshot_t sound;
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280 && !defined(BETTA_SIMULATOR)
    xiaozhi_spectrum_snapshot(&sound);
#else
    memcpy(sound.microphone, v->snapshot.spectrum_mic, sizeof(sound.microphone));
    memcpy(sound.speaker, v->snapshot.spectrum_speaker, sizeof(sound.speaker));
    sound.mic_active = v->snapshot.spectrum_mic_active;
    sound.speaker_active = v->snapshot.spectrum_speaker_active;
#endif
    bool active = sound.mic_active || sound.speaker_active;
    const uint8_t *source = sound.speaker_active ? sound.speaker : sound.microphone;
    bool changed = v->spectrum_active != active;
    v->spectrum_active = active;
    for (unsigned i = 0; i < XIAOZHI_SPECTRUM_BANDS; ++i) {
        /* Immediate attack, short release; inactive snapshots clear at once.
         * No invented movement during silence, thinking or after disconnect. */
        uint8_t next = !active ? 0 : source[i] >= v->levels[i] ? source[i] : (v->levels[i] + source[i]) / 2;
        changed |= next * 4 / 255 != v->levels[i] * 4 / 255;
        v->levels[i] = next;
    }
    if (changed) lv_obj_invalidate(v->spectrum);
}

static const char *hint(const view_t *v)
{
    const xiaozhi_status_t *s = &v->snapshot;
    if (connection_error(s))
        return hands_free(v) ? ui_i18n_get("xiaozhi.hint_error", "Wake me again to retry") :
            ui_i18n_get("xiaozhi.hint_manual_error", "Hold to talk to retry");
    if (s->duplex_active && s->speaking && s->listening)
        return ui_i18n_get("xiaozhi.hint_duplex", "I'm speaking and listening");
    if (s->listening) return ui_i18n_get("xiaozhi.hint_listening", "Go ahead, I'm listening");
    if (s->conversation && !s->speaking)
        return ui_i18n_get("xiaozhi.hint_thinking", "One moment");
    if (!s->conversation && !strcmp(s->state, "ready"))
        return ui_i18n_get("xiaozhi.hint_ready", "Say the wake word to start a conversation");
    return "";
}

static void refresh(lv_timer_t *timer)
{
    view_t *v = lv_timer_get_user_data(timer);
    if (lv_tick_elaps(v->polled_at) < 250) {
        if (v->dialog_visible) spectrum_update(v);
        else if (lv_obj_is_hidden(v->panel)) stop_animation(v);
        return;
    }
    v->polled_at = lv_tick_get();
    xiaozhi_status_t *s = &v->snapshot;
    if (!xiaozhi_status_try(s)) {
        /* Only the worker can expire a conversation. An old UI snapshot must
         * not close a newly active voice session while the status lock is busy. */
        if (v->dialog_visible) spectrum_update(v);
        else if (lv_obj_is_hidden(v->panel)) stop_animation(v);
        return;
    }
    if (lv_obj_is_hidden(v->button) == s->enabled) {
        lv_obj_set_hidden(v->button, !s->enabled); ui_pages_set_topbar_auxiliary(v->button);
    }
    if (!s->enabled) set_dialog_visible(v, false);
    if (s->dialog_seq != v->shown_seq) {
        bool pending = v->pending_talk && s->dialog_seq == v->pending_seq + 1 && !s->dialog_voice_opened;
        v->pressed = v->pending_talk = v->pending_stop = false;
        v->shown_seq = s->dialog_seq;
        if (s->enabled && dialog_needs_view(s)) {
            show(v, s->dialog_voice_opened); ESP_LOGI("xiaozhi", "AI Assistant dialog shown");
            if (pending && s->dialog_active && xiaozhi_command("start_turn", (int)s->dialog_seq) == ESP_OK) {
                v->press_seq = s->dialog_seq; v->pressed = true;
            }
        }
    }
    if (!s->dialog_active && s->dialog_end_seq == v->shown_seq &&
        s->dialog_end_reason[0] && strcmp(s->dialog_end_reason, "error")) {
        /* Normal close already stopped audio. A second queued disconnect could
         * cancel the next wake request. */
        set_dialog_visible(v, false);
    }
    if (v->pending_stop && xiaozhi_command("stop_turn", (int)v->press_seq) == ESP_OK)
        v->pending_stop = false;
    if (s->activity_seq != v->activity_seq) {
        v->activity_seq = s->activity_seq;
        if (v->dialog_visible) display_note_activity();
    }
    if (!v->dialog_visible) {
        if (lv_obj_is_hidden(v->panel)) stop_animation(v);
        return;
    }
    char key[80], text[128]; snprintf(key, sizeof(key), "xiaozhi.state.%s", s->state);
    snprintf(text, sizeof(text), "%s%s%s", ui_i18n_get(!strcmp(s->state, "wake_config_error") ? "xiaozhi.state.wake_error" : key, s->state), s->code[0] ? "\n" : "", s->code);
    label(v->state, text);
    if (quiet_hint(s)) {
        char countdown[160];
        const char *format = hands_free(v) ?
            ui_i18n_get("xiaozhi.hint_timeout", "Returning to home in %u s - Speak to continue") :
            ui_i18n_get("xiaozhi.hint_timeout_manual", "Returning to home in %u s - Hold to talk to continue");
        const char *marker = strstr(format, "%u");
        /* Custom translations are text, never executable printf formats. */
        if (marker) snprintf(countdown, sizeof(countdown), "%.*s%u%s", (int)(marker - format), format,
            (unsigned)((s->silence_remaining_ms + 999) / 1000), marker + 2);
        else snprintf(countdown, sizeof(countdown), "%s", format);
        label(v->hint, countdown);
    } else label(v->hint, hint(v));
    lv_obj_set_style_text_color(v->state, lv_color_hex(connection_error(s) ? APP_UI_COLOR_ERROR : APP_UI_COLOR_TEXT_MUTED), 0);
    label(v->emoji, xiaozhi_emotion_glyph(s->emotion));
    bool animated = false;
#if LV_USE_GIF && (CONFIG_APP_PANEL_VARIANT_10INCH_1280 || defined(BETTA_SIMULATOR))
    if (s->emoji_style == 0) {
        const lv_image_dsc_t *asset = xiaozhi_otto_asset(s->emotion);
        if (!v->otto) {
            v->otto = lv_gif_create(v->panel); lv_gif_set_color_format(v->otto, LV_COLOR_FORMAT_RGB565);
        }
        if (v->otto_asset != asset) { lv_gif_set_src(v->otto, asset); v->otto_asset = asset; }
        animated = lv_gif_is_loaded(v->otto); lv_obj_set_hidden(v->otto, !animated);
        lv_image_set_inner_align(v->otto, LV_IMAGE_ALIGN_CONTAIN);
    } else stop_animation(v);
#endif
    lv_obj_set_hidden(v->emoji, animated);
    lv_obj_set_style_text_font(v->recognized, ui_font_reply(ui_font_role(UI_TEXT_LABEL)), 0);
    lv_obj_set_style_text_font(v->reply, ui_font_reply(ui_font_role(UI_TEXT_VALUE)), 0);
    bool changed = label(v->recognized, s->recognized);
    changed |= label(v->reply, s->reply);
    lv_obj_set_hidden(v->question_row, !s->recognized[0]);
    lv_obj_set_hidden(v->answer_row, !s->reply[0]);
    arrange(v);
    if (changed) lv_obj_scroll_to_y(v->chat, 0, LV_ANIM_OFF);
    spectrum_update(v);
}

static void toggle(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e);
    if (!v->dialog_visible) xiaozhi_command("dialog_turn", (int)v->shown_seq); else hide(v);
}
static void close_clicked(lv_event_t *e) { hide(lv_event_get_user_data(e)); }
static void press(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e);
    if (!v->dialog_visible) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED && !lv_obj_is_hidden(v->talk) && !v->pending_stop) {
        if (!v->snapshot.dialog_active) {
            /* Retry a failed manual conversation only while the finger stays
             * down; the worker first supplies the new generation. */
            if (xiaozhi_command("dialog_turn", (int)v->shown_seq) == ESP_OK) {
                v->pending_seq = v->shown_seq; v->pending_talk = true;
            }
        } else if (xiaozhi_command("start_turn", (int)v->shown_seq) == ESP_OK) {
            v->press_seq = v->shown_seq; v->pressed = true;
        }
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        v->pending_talk = false;
        if (v->pressed) {
            v->pressed = false;
            v->pending_stop = xiaozhi_command("stop_turn", (int)v->press_seq) != ESP_OK;
        }
    }
}
static void destroy(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e);
    if (v->dialog_visible) xiaozhi_command("dismiss", (int)v->shown_seq);
    lv_timer_delete(v->timer); lv_free(v);
}

static lv_obj_t *bubble(lv_obj_t *chat, bool user, lv_obj_t **row, lv_obj_t **body)
{
    *row = lv_obj_create(chat); lv_obj_remove_style_all(*row);
    lv_obj_set_size(*row, lv_pct(100), LV_SIZE_CONTENT); lv_obj_set_flex_flow(*row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(*row, user ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *box = lv_obj_create(*row); lv_obj_remove_style_all(box);
    lv_obj_set_height(box, LV_SIZE_CONTENT); lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(box, 14, 0); lv_obj_set_style_pad_row(box, 6, 0);
    lv_obj_set_style_radius(box, ui_style_radius(16), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, lv_color_mix(lv_color_hex(APP_UI_COLOR_TEXT_PRIMARY),
        lv_color_hex(APP_UI_COLOR_CONTENT_BG), user ? 18 : 10), 0);
    lv_obj_t *name = text_label(box, UI_POPUP_LABEL_HINT);
    label(name, ui_i18n_get(user ? "xiaozhi.you" : "xiaozhi.assistant", user ? "You" : "AI Assistant"));
    if (!user) lv_obj_set_style_text_color(name, lv_color_hex(APP_UI_COLOR_CARD_ICON_ON), 0);
    *body = text_label(box, user ? UI_POPUP_LABEL_BODY : UI_POPUP_LABEL_VALUE);
    return box;
}

void xiaozhi_ui_create(lv_obj_t *screen)
{
    view_t *v = lv_malloc_zeroed(sizeof(*v)); if (!v) return;
    /* A busy worker must not permanently suppress a supported UI at boot. */
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280 || defined(BETTA_SIMULATOR)
    v->snapshot.supported = true;
#endif
    (void)xiaozhi_status_try(&v->snapshot);
    if (!v->snapshot.supported) { lv_free(v); return; }
    /* Rebuilding a view must not orphan an active conversation or hide a
     * current failure before its recovery guidance can be displayed. */
    v->shown_seq = v->snapshot.dialog_seq - (dialog_needs_view(&v->snapshot) ? 1U : 0U);
    v->activity_seq = v->snapshot.activity_seq;
    v->polled_at = lv_tick_get();
    lv_obj_t *root = lv_obj_create(screen); lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_clickable(root, false); lv_obj_set_scrollable(root, false);
    v->button = ui_pages_create_topbar_action(root, APP_SYMBOL_AI_AGENT, toggle, v);
    lv_obj_set_style_bg_color(v->button, lv_color_hex(APP_UI_COLOR_NAV_BTN_BG_ACTIVE), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(v->button, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(v->button, lv_color_hex(APP_UI_COLOR_NAV_TAB_ACTIVE), LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(v->button, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(lv_obj_get_child(v->button, 0), lv_color_hex(APP_UI_COLOR_NAV_TAB_ACTIVE), LV_STATE_CHECKED);
    lv_obj_set_hidden(v->button, true); ui_pages_set_topbar_auxiliary(v->button);
    v->panel = ui_popup_card_create(root);
    int w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int h = lv_display_get_vertical_resolution(lv_display_get_default());
    lv_obj_set_size(v->panel, LV_MIN(w - 24, 960), h - 136); lv_obj_center(v->panel);
    lv_obj_set_style_pad_all(v->panel, w < 600 ? 14 : 24, 0);
    lv_obj_set_style_radius(v->panel, ui_style_radius(18), 0); lv_obj_set_hidden(v->panel, true);
    v->state = text_label(v->panel, UI_POPUP_LABEL_HINT);
    lv_obj_t *close = ui_popup_close_button(v->panel, lv_color_hex(APP_UI_COLOR_NAV_BTN_BG_IDLE), close_clicked, v);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
    v->emoji = lv_label_create(v->panel); lv_label_set_text(v->emoji, xiaozhi_emotion_glyph("neutral"));
    lv_obj_set_style_text_color(v->emoji, lv_color_hex(APP_UI_COLOR_TEXT_PRIMARY), 0);
    v->chat = lv_obj_create(v->panel); lv_obj_remove_style_all(v->chat);
    lv_obj_set_flex_flow(v->chat, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(v->chat, 18, 0);
    lv_obj_set_style_pad_right(v->chat, 6, 0); lv_obj_set_scroll_dir(v->chat, LV_DIR_VER);
    v->question = bubble(v->chat, true, &v->question_row, &v->recognized);
    v->answer = bubble(v->chat, false, &v->answer_row, &v->reply);
    v->talk = lv_button_create(v->panel); lv_obj_set_size(v->talk, LV_MIN(w - 80, 240), 48);
    lv_obj_t *talk_label = text_label(v->talk, UI_POPUP_LABEL_HINT);
    label(talk_label, ui_i18n_get("xiaozhi.talk", "Hold to talk")); lv_obj_center(talk_label);
    lv_obj_set_style_text_align(talk_label, LV_TEXT_ALIGN_CENTER, 0);
    ui_style_action_button(v->talk, talk_label, UI_CONTROL_ACTION, true);
    lv_obj_add_event_cb(v->talk, press, LV_EVENT_ALL, v);
    v->title = text_label(v->panel, UI_POPUP_LABEL_TITLE);
    label(v->title, ui_i18n_get("xiaozhi.assistant", "AI Assistant"));
    v->spectrum = lv_obj_create(v->panel); lv_obj_remove_style_all(v->spectrum);
    lv_obj_set_clickable(v->spectrum, false); lv_obj_set_scrollable(v->spectrum, false);
    lv_obj_add_event_cb(v->spectrum, draw_spectrum, LV_EVENT_DRAW_MAIN, v);
    v->hint = text_label(v->panel, UI_POPUP_LABEL_HINT);
    lv_obj_set_style_text_align(v->hint, LV_TEXT_ALIGN_CENTER, 0);
    v->timer = lv_timer_create(refresh, 50, v);
    if (!v->timer) { lv_obj_delete(root); lv_free(v); return; }
    lv_obj_add_event_cb(root, destroy, LV_EVENT_DELETE, v);
}
