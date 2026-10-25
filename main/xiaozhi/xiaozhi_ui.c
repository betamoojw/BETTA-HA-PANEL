/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * The child root owns all controls and its timer; network tasks never call LVGL.
 */
#include "xiaozhi.h"
#include "xiaozhi_dialog.h"
#include "xiaozhi_emoji.h"
#include "ui/text_font.h"
#include "ui/ui_i18n.h"
#include "ui/ui_pages.h"
#include "ui/ui_popup.h"
#include "ui/fonts/app_symbols.h"
#include "ui/theme/ui_typography.h"
#include "ui/theme/ui_style.h"
#include "ui/theme/theme_default.h"
#include "drivers/display_init.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *button, *panel, *state, *emoji, *recognized, *reply, *chat, *talk;
    lv_timer_t *timer;
    uint32_t shown_seq, activity_seq, idle_since;
    xiaozhi_status_t snapshot;
    lv_obj_t *otto;
    const lv_image_dsc_t *otto_asset;
} view_t;
static void stop_animation(view_t *v)
{
    if (v->otto) lv_obj_delete(v->otto);
    v->otto = NULL; v->otto_asset = NULL;
}
static void hide(view_t *v)
{
    stop_animation(v);
    lv_obj_set_hidden(v->panel, true); xiaozhi_command("disconnect", 0);
}
static void show(view_t *v)
{
    lv_obj_set_hidden(v->panel, false); lv_obj_move_foreground(lv_obj_get_parent(v->panel));
    v->idle_since = lv_tick_get(); display_note_activity();
}
static void label(lv_obj_t *obj, const char *value)
{
    if (strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value);
}
static void refresh(lv_timer_t *timer)
{
    view_t *v = lv_timer_get_user_data(timer);
    xiaozhi_status_t *s = &v->snapshot; xiaozhi_status(s);
    if (lv_obj_is_hidden(v->button) == s->enabled) {
        lv_obj_set_hidden(v->button, !s->enabled); ui_pages_set_topbar_auxiliary(v->button);
    }
    if (!s->enabled) lv_obj_set_hidden(v->panel, true);
    if (s->dialog_seq != v->shown_seq) {
        v->shown_seq = s->dialog_seq;
        if (s->enabled) show(v);
    }
    if (s->activity_seq != v->activity_seq) {
        v->activity_seq = s->activity_seq; v->idle_since = lv_tick_get();
        if (!lv_obj_is_hidden(v->panel)) display_note_activity();
    }
    if (lv_obj_is_hidden(v->panel)) { stop_animation(v); return; }
    unsigned timeout = s->dialog_timeout >= 10 && s->dialog_timeout <= 300 ? s->dialog_timeout : 30;
    if (xiaozhi_dialog_expired(lv_tick_elaps(v->idle_since), 0, timeout)) { hide(v); return; }
    char key[80], text[128]; snprintf(key, sizeof(key), "xiaozhi.state.%s", s->state);
    snprintf(text, sizeof(text), "%s%s%s", ui_i18n_get(key, s->state), s->code[0] ? "\n" : "", s->code);
    label(v->state, text);
    label(v->emoji, xiaozhi_emotion_glyph(s->emotion));
    bool animated = false;
#if LV_USE_GIF && (CONFIG_APP_PANEL_VARIANT_10INCH_1280 || defined(BETTA_SIMULATOR))
    if (s->emoji_style == 0) {
        const lv_image_dsc_t *asset = xiaozhi_otto_asset(s->emotion);
        if (!v->otto) {
            v->otto = lv_gif_create(v->panel);
            lv_gif_set_color_format(v->otto, LV_COLOR_FORMAT_RGB565);
            /* Visibility is owned here: close deletes the decoder. LVGL's
             * auto-pause can stop the first frame before layout gives it bounds. */
        }
        if (v->otto_asset != asset) { lv_gif_set_src(v->otto, asset); v->otto_asset = asset; }
        animated = lv_gif_is_loaded(v->otto);
        lv_obj_set_hidden(v->otto, !animated);
        int y = (lv_display_get_vertical_resolution(lv_display_get_default()) - 160) / 5;
        lv_obj_set_size(v->otto, LV_MIN(128, y), LV_MIN(128, y));
        lv_image_set_inner_align(v->otto, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_align(v->otto, LV_ALIGN_TOP_MID, 0, y);
    } else stop_animation(v);
#endif
    lv_obj_set_hidden(v->emoji, animated);
    label(v->recognized, s->recognized);
    label(v->reply, s->reply);
    bool hands_free = s->wake_enabled && s->wake_ready;
    lv_obj_set_hidden(v->talk, hands_free);
    lv_obj_set_height(v->chat, lv_pct(hands_free ? 52 : 40));
}
static void toggle(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e);
    if (lv_obj_is_hidden(v->panel)) { show(v); xiaozhi_command("open", 0); }
    else hide(v);
}
static void close_clicked(lv_event_t *e) { hide(lv_event_get_user_data(e)); }
static void press(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) xiaozhi_command("start", 0);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) xiaozhi_command("stop", 0);
}
static void destroy(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e); lv_timer_delete(v->timer); lv_free(v);
    xiaozhi_command("disconnect", 0);
}
static lv_obj_t *text_label(lv_obj_t *parent, ui_text_role_t role)
{
    lv_obj_t *obj = lv_label_create(parent); lv_label_set_text(obj, "");
    lv_obj_set_width(obj, lv_pct(100)); lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(obj, ui_font_role(role), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(APP_UI_COLOR_TEXT_PRIMARY), 0);
    return obj;
}
void xiaozhi_ui_create(lv_obj_t *screen)
{
    view_t *v = lv_malloc_zeroed(sizeof(*v)); if (!v) return;
    xiaozhi_status(&v->snapshot);
    if (!v->snapshot.supported) { lv_free(v); return; }
    v->shown_seq = v->snapshot.dialog_seq;
    v->activity_seq = v->snapshot.activity_seq;
    lv_obj_t *root = lv_obj_create(screen); lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_clickable(root, false); lv_obj_set_scrollable(root, false);
    v->button = ui_pages_create_topbar_action(root, APP_SYMBOL_AI_AGENT, toggle, v);
    lv_obj_set_hidden(v->button, true); ui_pages_set_topbar_auxiliary(v->button);
    v->panel = lv_obj_create(root);
    int w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int h = lv_display_get_vertical_resolution(lv_display_get_default());
    lv_obj_set_size(v->panel, LV_MIN(w - 24, 960), h - 136); lv_obj_center(v->panel);
    lv_obj_set_scrollable(v->panel, false); lv_obj_set_style_pad_all(v->panel, 12, 0);
    lv_obj_set_style_radius(v->panel, ui_style_radius(14), 0);
    lv_obj_set_style_bg_color(v->panel, lv_color_hex(APP_UI_COLOR_CONTENT_BG), 0);
    lv_obj_set_style_bg_opa(v->panel, LV_OPA_COVER, 0); ui_popup_style_frame(v->panel);
    lv_obj_set_hidden(v->panel, true);
    v->state = text_label(v->panel, UI_TEXT_META); lv_obj_set_width(v->state, lv_pct(78));
    lv_obj_t *close = ui_popup_close_button(v->panel, lv_color_hex(APP_UI_COLOR_NAV_BTN_BG_IDLE), close_clicked, v);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
    v->emoji = lv_label_create(v->panel); lv_label_set_text(v->emoji, xiaozhi_emotion_glyph("neutral"));
    const lv_font_t *font = ui_font_emotion((h - 160) / 5);
    lv_obj_set_style_text_font(v->emoji, font ? font : ui_font_role(UI_TEXT_HERO), 0);
    lv_obj_set_style_text_color(v->emoji, lv_color_hex(APP_UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(v->emoji, LV_ALIGN_TOP_MID, 0, (h - 160) / 5);
    v->chat = lv_obj_create(v->panel); lv_obj_remove_style_all(v->chat);
    lv_obj_set_size(v->chat, lv_pct(100), lv_pct(40));
    lv_obj_set_pos(v->chat, 0, (h - 160) * 43 / 100);
    lv_obj_set_flex_flow(v->chat, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(v->chat, 8, 0);
    lv_obj_set_scroll_dir(v->chat, LV_DIR_VER);
    label(text_label(v->chat, UI_TEXT_META), ui_i18n_get("xiaozhi.recognized", "Recognized speech"));
    v->recognized = text_label(v->chat, UI_TEXT_LABEL);
    label(text_label(v->chat, UI_TEXT_META), ui_i18n_get("xiaozhi.reply", "AI reply"));
    v->reply = text_label(v->chat, UI_TEXT_LABEL);
    v->talk = lv_button_create(v->panel); lv_obj_set_size(v->talk, LV_MIN(w - 80, 240), 48);
    lv_obj_set_style_bg_color(v->talk, lv_color_hex(APP_UI_COLOR_NAV_BTN_BG_IDLE), 0);
    lv_obj_set_style_radius(v->talk, ui_style_radius(8), 0);
    lv_obj_align(v->talk, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_t *talk_label = text_label(v->talk, UI_TEXT_META);
    label(talk_label, ui_i18n_get("xiaozhi.talk", "Hold to talk")); lv_obj_center(talk_label);
    lv_obj_set_style_text_align(talk_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(v->talk, press, LV_EVENT_ALL, v);
    v->timer = lv_timer_create(refresh, 250, v);
    if (!v->timer) { lv_obj_delete(root); lv_free(v); return; }
    lv_obj_add_event_cb(root, destroy, LV_EVENT_DELETE, v);
}
