/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Exercise real LVGL cleanup, timers and the production assistant overlay.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xiaozhi/xiaozhi.h"
#include "ui/text_font.h"

static bool enabled;
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
    memset(s, 0, sizeof(*s)); s->enabled = enabled;
    strcpy(s->state, enabled ? "ready" : "disabled");
}
esp_err_t xiaozhi_command(const char *action, int value)
{
    (void)value; assert(strcmp(action, "disconnect") == 0); ++disconnects; return ESP_OK;
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
    lv_display_delete(display);
    lv_deinit();
    puts("PASS: 100 dashboard rebuilds, enabled/disabled timers, screen deletion");
    return 0;
}
