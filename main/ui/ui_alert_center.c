/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/ui_transition.h"
#include "ui/ui_alert_center.h"
#include "ui/ui_pages.h"
#include "ui/ui_popup.h"
#include "ui/ui_i18n.h"
#include "ui/theme/ui_typography.h"
#include "ui/theme/ui_style.h"
#include "ui/theme/theme_default.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

typedef struct { char title[128], detail[1536]; uint64_t expires; } alert_t;
/* LVGL uses PSRAM first. Allocate only once, outside the UI task stack. */
static alert_t *s_alerts;
static lv_obj_t *s_root, *s_body;
static bool s_dirty;

static const char *tr(const char *key, const char *fallback) { return ui_i18n_get(key, fallback); }
void ui_alert_center_clear(unsigned slot)
{
    if (s_alerts && slot < UI_ALERT_SLOTS && s_alerts[slot].title[0]) {
        s_alerts[slot].title[0] = 0; s_dirty = true;
    }
}
void ui_alert_center_set(unsigned slot, const char *title, const char *detail, uint64_t expires)
{
    if (slot >= UI_ALERT_SLOTS) return;
    if (!title || !title[0]) { ui_alert_center_clear(slot); return; }
    if (!s_alerts) s_alerts = lv_malloc_zeroed(sizeof(*s_alerts) * UI_ALERT_SLOTS);
    if (!s_alerts) return;
    alert_t *a = &s_alerts[slot];
    if (strcmp(a->title, title) || strcmp(a->detail, detail ? detail : "")) s_dirty = true;
    snprintf(a->title, sizeof(a->title), "%s", title);
    snprintf(a->detail, sizeof(a->detail), "%s", detail ? detail : "");
    a->expires = expires;
}
unsigned ui_alert_center_count(void)
{
    unsigned count = 0;
    for (unsigned i = 0; s_alerts && i < UI_ALERT_SLOTS; i++) count += s_alerts[i].title[0] != 0;
    return count;
}
static lv_obj_t *text(lv_obj_t *parent, const char *value, bool heading)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    ui_text_set(label, heading ? UI_TEXT_LABEL : UI_TEXT_META, true);
    lv_obj_set_style_text_color(label, lv_color_hex(APP_UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}
static void render(void)
{
    if (!s_body) return;
    int scroll = lv_obj_get_scroll_y(s_body);
    lv_obj_clean(s_body);
    if (!ui_alert_center_count()) text(s_body, tr("alerts.empty", "No alerts"), false);
    for (unsigned i = 0; s_alerts && i < UI_ALERT_SLOTS; i++) {
        if (!s_alerts[i].title[0]) continue;
        lv_obj_t *card = lv_obj_create(s_body);
        lv_obj_set_width(card, lv_pct(100)); lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_scrollable(card, false);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 12, 0);
        lv_obj_set_style_radius(card, ui_style_radius(10), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(APP_UI_COLOR_TOPBAR_CHIP_BG), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(APP_UI_COLOR_TOPBAR_CHIP_BORDER), 0);
        text(card, s_alerts[i].title, true);
        text(card, s_alerts[i].detail, false);
    }
    lv_obj_update_layout(s_body);
    lv_obj_scroll_to_y(s_body, scroll, LV_ANIM_OFF);
    s_dirty = false;
}
void ui_alert_center_refresh(uint64_t now_ms)
{
    for (unsigned i = 0; s_alerts && i < UI_ALERT_SLOTS; i++)
        if (s_alerts[i].expires && now_ms >= s_alerts[i].expires) ui_alert_center_clear(i);
    ui_pages_set_alert_warning(ui_alert_center_count() != 0);
    if (s_dirty) render();
}
static void deleted(lv_event_t *event)
{
    (void)event; s_root = NULL; s_body = NULL;
}
void ui_alert_center_close(void) { if (s_root) lv_obj_delete(s_root); }
lv_obj_t *ui_alert_center_root(void) { return s_root; }
static void close_clicked(lv_event_t *event) { (void)event; ui_transition_close(s_root); }
static void backdrop(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) ui_transition_close(s_root);
}
void ui_alert_center_open(void)
{
    if (s_root) { lv_obj_move_foreground(s_root); return; }
    s_root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_60, 0);
    lv_obj_set_scrollable(s_root, false);
    lv_obj_add_event_cb(s_root, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(s_root, backdrop, LV_EVENT_CLICKED, NULL);
    int w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int h = lv_display_get_vertical_resolution(lv_display_get_default());
    lv_obj_t *panel = lv_obj_create(s_root);
    lv_obj_set_size(panel, LV_MIN(w - 20, 900), h - 20);
    lv_obj_center(panel); lv_obj_set_scrollable(panel, false);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_style_radius(panel, ui_style_radius(12), 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(APP_UI_COLOR_CONTENT_BG), 0);
    ui_popup_style_frame(panel);
    lv_obj_t *title = text(panel, tr("alerts.title", "Alerts"), true);
    lv_obj_set_width(title, lv_pct(70));
    lv_obj_t *close = ui_popup_close_button(panel, lv_color_hex(APP_UI_COLOR_NAV_BTN_BG_IDLE), close_clicked, NULL);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_update_layout(panel);
    int top = LV_MAX(lv_obj_get_height(title), UI_POPUP_CLOSE_SIZE) + 12;
    s_body = lv_obj_create(panel); lv_obj_remove_style_all(s_body);
    lv_obj_set_pos(s_body, 0, top);
    lv_obj_set_size(s_body, lv_obj_get_content_width(panel), lv_obj_get_content_height(panel) - top);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_body, 12, 0);
    lv_obj_set_scroll_dir(s_body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_body, LV_SCROLLBAR_MODE_AUTO);
    render();
    ui_transition_fade_in(s_root);
}
void ui_alert_center_crash(unsigned index, const coredump_record_t *r)
{
    if (index >= 2) return;
    if (!r || !r->id[0]) { ui_alert_center_clear(index); return; }
    /* Temporary formatting is PSRAM-backed rather than a large UI stack buffer. */
    char *body = lv_malloc(1536); if (!body) return;
    char date[40]; snprintf(date, sizeof(date), "%s", tr("alerts.unknown", "Unknown"));
    if (r->crashed_at > 0) { time_t t = (time_t)r->crashed_at; struct tm tm;
        localtime_r(&t, &tm); strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &tm); }
    const char *cause = tr("alerts.crash", "Retained crash report");
    if (r->diagnostic.cause == COREDUMP_CAUSE_PPA_TIMEOUT) cause = tr("alerts.ppa", "Display accelerator timeout");
    else if (r->diagnostic.cause == COREDUMP_CAUSE_LVGL_STALL) cause = tr("alerts.lvgl", "Display task stalled");
    else if (r->diagnostic.cause == COREDUMP_CAUSE_RUNTIME_STALL) cause = tr("alerts.runtime", "UI task stalled");
    const char *unknown = tr("alerts.unknown", "Unknown");
    snprintf(body, 1536, "%s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\nELF: %s\nID: %s\nPC: 0x%08" PRIx32 " / mcause: 0x%08" PRIx32 "\n%s",
        cause, tr("alerts.time", "Time"), date, tr("alerts.task", "Task"), r->task[0] ? r->task : unknown,
        tr("alerts.reason", "Reason"), r->panic_reason[0] ? r->panic_reason : unknown, tr("alerts.version", "Version"), r->version[0] ? r->version : unknown,
        tr("alerts.build", "Build"), r->git_commit[0] ? r->git_commit : unknown, r->elf_sha256[0] ? r->elf_sha256 : unknown, r->id, r->pc, r->mcause,
        tr("alerts.retained", "Historical report, not necessarily a current fault. Download or erase it in Web Settings > Device."));
    ui_alert_center_set(index, tr("alerts.crash", "Retained crash report"), body, 0);
    lv_free(body);
}
