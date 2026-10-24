/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "ui/ui_analog_clock.h"
#include <math.h>

typedef struct { int hour, minute, second; bool valid, seconds; } clock_face_t;

static lv_point_precise_t point(float cx, float cy, float radius, float degrees)
{
    const float radians = (degrees - 90.0f) * (3.14159265358979323846f / 180.0f);
    return (lv_point_precise_t){cx + radius * cosf(radians), cy + radius * sinf(radians)};
}

static void stroke(lv_layer_t *layer, float cx, float cy, float inner, float outer,
                   float degrees, int width, uint32_t color)
{
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.p1 = point(cx, cy, inner, degrees);
    line.p2 = point(cx, cy, outer, degrees);
    line.width = width;
    line.color = lv_color_hex(color);
    line.round_start = line.round_end = 1;
    lv_draw_line(layer, &line);
}

static void draw(lv_event_t *event)
{
    lv_obj_t *dial = lv_event_get_target_obj(event);
    clock_face_t *face = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_DELETE) { lv_free(face); return; }
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(dial, &area);
    const int diameter = LV_MIN(lv_area_get_width(&area), lv_area_get_height(&area));
    const float cx = (area.x1 + area.x2) * 0.5f, cy = (area.y1 + area.y2) * 0.5f;
    const int radius = diameter / 2 - 4;
    lv_draw_arc_dsc_t ring;
    lv_draw_arc_dsc_init(&ring);
    ring.center.x = cx; ring.center.y = cy;
    ring.radius = radius; ring.width = 1;
    ring.start_angle = 0; ring.end_angle = 360;
    ring.color = lv_color_hex(0x30353B);
    lv_draw_arc(layer, &ring);
    for (int i = 0; i < 12; ++i) {
        bool major = i % 3 == 0;
        stroke(layer, cx, cy, radius * (major ? .87f : .91f), radius * .94f,
               i * 30, major ? 3 : 1, major ? 0xD8DEE5 : 0x68717B);
    }
    /* An unsynchronised device shows the dial without a fabricated time. */
    if (!face->valid) return;
    const float minute = face->minute + (face->seconds ? face->second / 60.0f : 0);
    stroke(layer, cx, cy, -radius * .08f, radius * .51f,
           (face->hour % 12 + minute / 60.0f) * 30, LV_MAX(3, diameter / 75), 0xF1F4F8);
    stroke(layer, cx, cy, -radius * .08f, radius * .75f,
           minute * 6, LV_MAX(2, diameter / 100), 0xF1F4F8);
    if (face->seconds) stroke(layer, cx, cy, -radius * .18f, radius * .82f,
                             face->second * 6, 2, 0xEFA49B);
    lv_draw_rect_dsc_t cap;
    lv_draw_rect_dsc_init(&cap);
    cap.radius = LV_RADIUS_CIRCLE;
    cap.bg_color = lv_color_hex(0xF1F4F8); cap.bg_opa = LV_OPA_COVER;
    const int size = LV_MAX(3, diameter / 100);
    lv_area_t center = {(int)cx - size, (int)cy - size, (int)cx + size, (int)cy + size};
    lv_draw_rect(layer, &cap, &center);
}

lv_obj_t *ui_analog_clock_create(lv_obj_t *parent)
{
    clock_face_t *face = lv_malloc_zeroed(sizeof(*face));
    if (!face) return NULL;
    lv_obj_t *dial = lv_obj_create(parent);
    if (!dial) { lv_free(face); return NULL; }
    lv_obj_remove_style_all(dial);
    lv_obj_set_clickable(dial, false);
    lv_obj_set_scrollable(dial, false);
    lv_obj_set_user_data(dial, face);
    lv_obj_add_event_cb(dial, draw, LV_EVENT_ALL, face);
    return dial;
}

void ui_analog_clock_update(lv_obj_t *dial, const struct tm *local, bool seconds)
{
    if (!dial) return;
    clock_face_t *face = lv_obj_get_user_data(dial);
    face->valid = local && local->tm_year >= 120;
    face->seconds = seconds;
    if (face->valid) {
        face->hour = local->tm_hour; face->minute = local->tm_min; face->second = local->tm_sec;
    }
    lv_obj_invalidate(dial);
}
