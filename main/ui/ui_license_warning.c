/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/ui_license_warning.h"
#include <string.h>
#include "ui/text_font.h"
#include "ui/ui_i18n.h"

typedef struct warning_view {
    struct warning_view *next;
    lv_obj_t *owner, *keep_clear;
    int height;
    license_key_state_t state;
    bool visible;
    const lv_font_t *font;
    char language[16];
} warning_view_t;
static warning_view_t *s_views;
static lv_timer_t *s_timer;
static ui_license_warning_source_t s_source;

void ui_license_warning_set_source(ui_license_warning_source_t source) { s_source = source; }

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    ui_license_warning_status_t status = {.state = LICENSE_VALID};
    if (s_source) status = s_source();
    const bool visible = license_warning_due(status.state, status.uptime_us) && !status.suppressed;
    const char *language = ui_i18n_get_language();
    const lv_font_t *font = ui_font_role(UI_TEXT_META);
    for (warning_view_t *v = s_views; v; v = v->next) {
        if (v->visible != visible || (visible && (v->state != status.state ||
            v->font != font || strcmp(v->language, language)))) {
            v->visible = visible;
            v->state = status.state;
            v->font = font;
            /* Built-in language codes fit; custom codes already have a firmware limit. */
            strncpy(v->language, language, sizeof(v->language) - 1);
            v->language[sizeof(v->language) - 1] = 0;
            lv_area_t strip;
            lv_obj_get_coords(v->owner, &strip);
            strip.y2 = LV_MIN(strip.y2, strip.y1 + v->height - 1);
            lv_obj_invalidate_area(v->owner, &strip);
        }
    }
}

static void rectangle(lv_layer_t *layer, lv_area_t area, uint32_t color, lv_opa_t opacity)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color);
    d.bg_opa = opacity;
    lv_draw_rect(layer, &d, &area);
}

/* Narrow, bounded native primitives; no image, canvas or opacity group surface. */
static void rail(lv_layer_t *layer, int x1, int x2, int y)
{
    rectangle(layer, (lv_area_t){x1, y, x2, y + 2}, 0x222222, LV_OPA_80);
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.color = lv_color_hex(0xf4c542);
    d.opa = LV_OPA_90;
    for (int x = x1; x + 12 <= x2; x += 24) {
        d.p[0] = (lv_point_precise_t){x, y + 2};
        d.p[1] = (lv_point_precise_t){x + 2, y};
        d.p[2] = (lv_point_precise_t){x + 12, y};
        lv_draw_triangle(layer, &d);
        d.p[1] = (lv_point_precise_t){x + 10, y + 2};
        lv_draw_triangle(layer, &d);
    }
}

static const char *message(license_key_state_t state)
{
    switch (state) {
    case LICENSE_MISSING: return ui_i18n_get("license_lcd.missing", "No license");
    case LICENSE_INVALID: return ui_i18n_get("license_lcd.invalid", "Invalid license");
    case LICENSE_UNCONFIGURED: return ui_i18n_get("license_lcd.unconfigured", "Issuer not configured");
    default: return ui_i18n_get("license_lcd.error", "License status unavailable");
    }
}

static void paint(lv_event_t *event, warning_view_t *v)
{
    if (!v->visible) return;
    lv_area_t bounds;
    lv_obj_get_coords(v->owner, &bounds);
    bounds.y2 = LV_MIN(bounds.y2, bounds.y1 + v->height - 1);
    if (lv_area_get_height(&bounds) < 12) return;
    lv_layer_t *layer = lv_event_get_layer(event);
    rail(layer, bounds.x1, bounds.x2, bounds.y1);
    rail(layer, bounds.x1, bounds.x2, bounds.y2 - 2);
    int right = bounds.x2 - 8;
    if (v->keep_clear) {
        lv_area_t control;
        lv_obj_get_coords(v->keep_clear, &control);
        right = LV_MIN(right, control.x1 - 8);
    }
    lv_area_t plate = {bounds.x1 + 8, bounds.y1 + 6, right, bounds.y2 - 6};
    if (lv_area_get_width(&plate) < 16) return; /* The rails remain visible. */
    rectangle(layer, plate, 0x171717, (lv_opa_t)191);
    rectangle(layer, (lv_area_t){plate.x1, plate.y1, plate.x1 + 2, plate.y2}, 0xf4c542, LV_OPA_90);
    rectangle(layer, (lv_area_t){plate.x2 - 2, plate.y1, plate.x2, plate.y2}, 0xf4c542, LV_OPA_90);
    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init(&text);
    text.font = ui_font_role(UI_TEXT_META);
    text.color = lv_color_hex(0xffe083);
    text.text = message(v->state);
    const int available = lv_area_get_width(&plate) - 16;
    lv_point_t size;
    lv_text_get_size(&size, text.text, text.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > available) {
        text.text = ui_i18n_get("license_lcd.compact", "License warning");
        lv_text_get_size(&size, text.text, text.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    if (size.x > available) {
        text.text = "!";
        lv_text_get_size(&size, text.text, text.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    if (size.x > available || size.y > lv_area_get_height(&plate)) return;
    const int x = plate.x1 + (lv_area_get_width(&plate) - size.x) / 2;
    const int y = plate.y1 + (lv_area_get_height(&plate) - size.y) / 2;
    const lv_area_t label = {x, y, x + size.x - 1, y + size.y - 1};
    /* LVGL may defer draw tasks past translation replacement. Own the short text. */
    text.text_local = 1;
    lv_draw_label(layer, &text, &label);
}

static void event_cb(lv_event_t *event)
{
    warning_view_t *v = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_DRAW_POST) paint(event, v);
    if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
    warning_view_t **link = &s_views;
    while (*link && *link != v) link = &(*link)->next;
    if (*link) *link = v->next;
    lv_free(v);
    if (!s_views && s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
}

bool ui_license_warning_attach(lv_obj_t *header, lv_obj_t *keep_clear, int height)
{
    if (!header || height < 12) return false;
    for (warning_view_t *v = s_views; v; v = v->next) if (v->owner == header) return true;
    warning_view_t *v = lv_malloc_zeroed(sizeof(*v));
    if (!v) return false;
    if (!s_timer) s_timer = lv_timer_create(refresh, 1000, NULL);
    if (!s_timer) { lv_free(v); return false; }
    v->owner = header; v->keep_clear = keep_clear; v->height = height;
    if (!lv_obj_add_event_cb(header, event_cb, LV_EVENT_ALL, v)) {
        lv_free(v);
        if (!s_views) { lv_timer_delete(s_timer); s_timer = NULL; }
        return false;
    }
    v->next = s_views; s_views = v;
    refresh(NULL);
    return true;
}
