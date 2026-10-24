/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * UI timers alone read snapshots; no network task calls LVGL.
 */
#include "xiaozhi.h"
#include "ui/text_font.h"
#include "ui/ui_i18n.h"
#include "ui/ui_pages.h"
#include "ui/fonts/app_symbols.h"
#include "ui/theme/ui_typography.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
typedef struct { lv_obj_t *button, *panel, *label; lv_timer_t *timer; } view_t;
static void refresh(lv_timer_t *timer)
{
    view_t *v = lv_timer_get_user_data(timer);
    xiaozhi_status_t s; xiaozhi_status(&s);
    if (lv_obj_is_hidden(v->button) == s.enabled) {
        lv_obj_set_hidden(v->button, !s.enabled);
        ui_pages_set_topbar_auxiliary(v->button);
    }
    if (!s.enabled) lv_obj_set_hidden(v->panel, true);
    if (lv_obj_is_hidden(v->panel)) return;
    char key[80]; snprintf(key, sizeof(key), "xiaozhi.state.%s", s.state);
    char text[768]; snprintf(text, sizeof(text), "%s\n%s\n%s", ui_i18n_get(key, s.state), s.code, s.text);
    if (strcmp(lv_label_get_text(v->label), text)) lv_label_set_text(v->label, text);
}
static void toggle(lv_event_t *e)
{
    view_t *v = lv_event_get_user_data(e);
    if (lv_obj_is_hidden(v->panel)) lv_obj_set_hidden(v->panel, false);
    else { lv_obj_set_hidden(v->panel, true); xiaozhi_command("disconnect", 0); }
}
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
void xiaozhi_ui_create(lv_obj_t *screen)
{
    view_t *v = lv_malloc_zeroed(sizeof(*v)); if (!v) return;
    /* A dashboard rebuild cleans the screen's children without deleting the
     * screen itself. Own the timer on a child root so cleanup cannot leave it
     * running against deleted controls (even while the assistant is disabled). */
    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_clickable(root, false);
    lv_obj_set_scrollable(root, false);
    v->button = ui_pages_create_topbar_action(root, APP_SYMBOL_AI_AGENT, toggle, v);
    lv_obj_set_hidden(v->button, true);
    ui_pages_set_topbar_auxiliary(v->button);
    v->panel = lv_obj_create(root); lv_obj_set_size(v->panel, LV_PCT(80), LV_PCT(75)); lv_obj_center(v->panel);
    lv_obj_set_style_bg_opa(v->panel, LV_OPA_COVER, 0); lv_obj_set_hidden(v->panel, true);
    lv_obj_set_style_text_font(v->panel, ui_font_role(UI_TEXT_LABEL), 0);
    v->label = lv_label_create(v->panel); lv_obj_set_size(v->label, LV_PCT(95), LV_PCT(75));
    lv_label_set_long_mode(v->label, LV_LABEL_LONG_WRAP);
    lv_obj_t *talk = lv_button_create(v->panel); lv_obj_set_size(talk, 240, 60); lv_obj_align(talk, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_t *label = lv_label_create(talk); lv_label_set_text(label, ui_i18n_get("xiaozhi.talk", "Hold to talk")); lv_obj_center(label);
    lv_obj_add_event_cb(talk, press, LV_EVENT_ALL, v);
    v->timer = lv_timer_create(refresh, 250, v);
    if (!v->timer) { lv_obj_delete(root); lv_free(v); return; }
    lv_obj_add_event_cb(root, destroy, LV_EVENT_DELETE, v);
}
#else
void xiaozhi_ui_create(lv_obj_t *screen) { (void)screen; }
#endif
