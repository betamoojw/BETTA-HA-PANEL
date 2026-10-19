/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 betamoojw
 * Adapted by andre68723 from betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3,
 * commit 57df6a3, for the shared LVGL 9 adapter and all panel profiles.
 */
#include "ui/ui_remote_display.h"
#if CONFIG_APP_REMOTE_DISPLAY
#include <string.h>
#include "drivers/display_init.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl.h"

#define INPUT_COUNT 24
#define LEASE_US 1000000LL
#define FRAME_US 200000LL
typedef struct { unsigned kind; int16_t x, y; } input_t;
static lv_display_t *s_display;
static lv_display_rotation_t s_rotation;
static bool s_geometry_changed, s_capturing;
static size_t s_copied_pixels, s_required_pixels;
static lv_area_t s_pending_area;
static bool s_pending;
static lv_indev_t *s_remote;
static uint8_t *s_pixels;
static uint16_t s_width, s_height;
static bool s_open, s_ready, s_want, s_dirty, s_control, s_down, s_queued_down, s_suspended;
static int64_t s_heartbeat, s_last_capture;
static lv_point_t s_point;
static input_t s_inputs[INPUT_COUNT];
static unsigned s_head, s_count;
static bool s_cancel_reset;

/* All module state is accessed from the LVGL task or under display_lock. */
static void cancel_input(void)
{
    s_count = s_head = 0;
    s_down = s_queued_down = false;
    if (s_remote) {
        s_cancel_reset = true;
        lv_indev_reset(s_remote, NULL); /* Cancel, never synthesize RELEASED/CLICKED. */
        s_cancel_reset = false;
    }
}

static bool physical_pressed(void)
{
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i)) {
        if (i != s_remote && lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER &&
            lv_indev_get_display(i) == s_display && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED) return true;
    }
    return false;
}

static void physical_event(lv_event_t *event)
{
    (void)event;
    s_control = false;
    cancel_input();
}

static void remote_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    if ((s_control && esp_timer_get_time() - s_heartbeat > LEASE_US) || physical_pressed()) {
        cancel_input();
        if (esp_timer_get_time() - s_heartbeat > LEASE_US) s_control = false;
    }
    if (s_control && s_count) {
        input_t p = s_inputs[s_head];
        s_head = (s_head + 1) % INPUT_COUNT;
        --s_count;
        s_point = (lv_point_t){p.x, p.y};
        s_down = p.kind != 2;
    }
    data->point = s_point;
    /* LVGL rotates raw input into logical coordinates; undo that transform
     * because browser coordinates already describe the displayed orientation. */
    switch (s_rotation) {
        case LV_DISPLAY_ROTATION_90: data->point = (lv_point_t){s_point.y, s_width - 1 - s_point.x}; break;
        case LV_DISPLAY_ROTATION_180: data->point = (lv_point_t){s_width - 1 - s_point.x, s_height - 1 - s_point.y}; break;
        case LV_DISPLAY_ROTATION_270: data->point = (lv_point_t){s_height - 1 - s_point.y, s_point.x}; break;
        default: break;
    }
    data->state = s_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = s_count > 0;
}

static void remote_activity(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) display_note_activity();
    else if (lv_event_get_code(event) == LV_EVENT_INDEV_RESET && s_cancel_reset) {
        /* LVGL reset clears pressed styling, but custom sliders also need PRESS_LOST
         * to clear their dragging flag. The reset event supplies the old active object.
         * Do not send RELEASED: that would commit a canceled slider value to HA. */
        lv_obj_t *obj = lv_event_get_param(event);
        if (obj && lv_obj_is_valid(obj)) lv_obj_send_event(obj, LV_EVENT_PRESS_LOST, s_remote);
    }
}

/* Observe LVGL's logical draw buffer before the adapter swaps bytes or
 * rotates it. Never replace its flush callback or signal flush-ready. */
static void mirror_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_RESOLUTION_CHANGED) {
        s_geometry_changed = true;
        s_control = false;
        cancel_input();
        return;
    }
    if (!s_open || s_suspended || s_geometry_changed) return;
    if (code == LV_EVENT_REFR_START) {
        s_capturing = s_want && !s_ready;
        s_copied_pixels = 0;
        return;
    }
    if (code == LV_EVENT_FLUSH_START) {
        s_dirty = true;
        const lv_area_t *area = lv_event_get_param(event);
        lv_draw_buf_t *buf = lv_display_get_buf_active(s_display);
        if (!area || !buf || !buf->data) return;
        /* FLUSH_START adds the raw display offset; LVGL's public offset
         * getters rotate it. Recover the raw offset before subtracting it. */
        int ox = lv_display_get_offset_x(s_display), oy = lv_display_get_offset_y(s_display);
        int pw = lv_display_get_physical_horizontal_resolution(s_display);
        int ph = lv_display_get_physical_vertical_resolution(s_display);
        if (s_rotation == LV_DISPLAY_ROTATION_90) { int t = ox; ox = oy; oy = t; }
        else if (s_rotation == LV_DISPLAY_ROTATION_180) { ox = pw - ox; oy = ph - oy; }
        else if (s_rotation == LV_DISPLAY_ROTATION_270) { int t = ox; ox = ph - oy; oy = pw - t; }
        int x1 = area->x1 - ox, y1 = area->y1 - oy;
        int x2 = area->x2 - ox, y2 = area->y2 - oy;
        if (x1 < 0 || y1 < 0 || x2 >= s_width || y2 >= s_height) return;
        lv_area_t logical = {x1, y1, x2, y2};
        if (!s_capturing) {
            if (s_pending) {
                s_pending_area.x1 = LV_MIN(s_pending_area.x1, x1);
                s_pending_area.y1 = LV_MIN(s_pending_area.y1, y1);
                s_pending_area.x2 = LV_MAX(s_pending_area.x2, x2);
                s_pending_area.y2 = LV_MAX(s_pending_area.y2, y2);
            }
            else s_pending_area = logical;
            s_pending = true;
            return;
        }
        bool partial = lv_display_get_render_mode(s_display) == LV_DISPLAY_RENDER_MODE_PARTIAL;
        const uint8_t *src = buf->data;
        if (!partial) src += (size_t)y1 * buf->header.stride + x1 * 2U;
        for (int y = y1; y <= y2; ++y) {
            memcpy(s_pixels + ((size_t)y * s_width + x1) * 2U,
                   src, (size_t)(x2 - x1 + 1) * 2U);
            src += buf->header.stride;
        }
        s_copied_pixels += (size_t)(x2 - x1 + 1) * (y2 - y1 + 1);
    } else if (code == LV_EVENT_REFR_READY && s_capturing) {
        s_capturing = false;
        s_ready = s_copied_pixels >= s_required_pixels;
        s_want = false;
        s_dirty = !s_ready;
        s_last_capture = esp_timer_get_time();
    }
}

esp_err_t ui_remote_display_init(void)
{
    if (!display_lock(1000)) return ESP_ERR_TIMEOUT;
    if (s_display) { display_unlock(); return ESP_OK; }
    lv_display_t *d = lv_display_get_default();
    if (!d || lv_display_get_color_format(d) != LV_COLOR_FORMAT_RGB565) {
        display_unlock(); return ESP_ERR_NOT_SUPPORTED;
    }
    s_remote = lv_indev_create();
    if (!s_remote) { display_unlock(); return ESP_ERR_NO_MEM; }
    s_display = d;
    s_width = lv_display_get_horizontal_resolution(d);
    s_height = lv_display_get_vertical_resolution(d);
    lv_indev_set_type(s_remote, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(s_remote, d);
    lv_indev_set_read_cb(s_remote, remote_read);
    lv_indev_add_event_cb(s_remote, remote_activity, LV_EVENT_ALL, NULL);
    lv_timer_set_period(lv_indev_get_read_timer(s_remote), 10);
    lv_indev_enable(s_remote, false);
    lv_timer_pause(lv_indev_get_read_timer(s_remote));
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i)) {
        if (i != s_remote && lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER)
            lv_indev_add_event_cb(i, physical_event, LV_EVENT_PRESSED, NULL);
    }
    lv_display_add_event_cb(d, mirror_event, LV_EVENT_ALL, NULL);
    display_unlock();
    return ESP_OK;
}

esp_err_t ui_remote_display_open(void)
{
    if (!display_lock(1000)) return ESP_ERR_TIMEOUT;
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_display && !s_open && !s_suspended) {
        s_width = lv_display_get_horizontal_resolution(s_display);
        s_height = lv_display_get_vertical_resolution(s_display);
        s_rotation = lv_display_get_rotation(s_display);
        s_geometry_changed = false;
        size_t bytes = (size_t)s_width * s_height * 2;
        /* Leave reserve for the UI, wallpaper and networking; no internal-RAM fallback. */
        if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > bytes + 512 * 1024U)
            s_pixels = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        err = s_pixels ? ESP_OK : ESP_ERR_NO_MEM;
        if (s_pixels) {
            lv_indev_enable(s_remote, true);
            lv_timer_resume(lv_indev_get_read_timer(s_remote));
            s_open = s_dirty = true;
            s_ready = s_want = s_control = false;
            s_last_capture = 0;
            s_pending = true;
            s_pending_area = (lv_area_t){0, 0, s_width - 1, s_height - 1};
        }
    }
    display_unlock();
    return err;
}

void ui_remote_display_close(void)
{
    if (!display_lock(0)) return; /* Port timeout 0 means wait indefinitely. */
    s_open = s_ready = s_want = s_control = s_capturing = false;
    cancel_input();
    if (s_remote) {
        lv_indev_enable(s_remote, false);
        lv_timer_pause(lv_indev_get_read_timer(s_remote));
    }
    heap_caps_free(s_pixels);
    s_pixels = NULL;
    display_unlock();
}

bool ui_remote_display_capture(const uint8_t **pixels, uint16_t *width, uint16_t *height, unsigned *retry_ms)
{
    *retry_ms = 200;
    if (!display_lock(100)) return false;
    bool ready = s_open && s_ready && !s_suspended;
    if (ready) { *pixels = s_pixels; *width = s_width; *height = s_height; }
    else if (s_open && !s_suspended && s_dirty && !s_want &&
             esp_timer_get_time() - s_last_capture >= FRAME_US) {
        s_want = true;
        /* Keep the acknowledged pixels and replay only regions missed while the
         * immutable frame was in flight. A full-screen redraw on every clock or
         * sensor update competes with continuous RGB scanout on the S3. */
        lv_area_t area = s_pending ? s_pending_area :
            (lv_area_t){0, 0, s_width - 1, s_height - 1};
        s_required_pixels = lv_area_get_size(&area);
        s_pending = false;
        lv_obj_invalidate_area(lv_display_get_screen_active(s_display), &area);
        lv_obj_invalidate_area(lv_display_get_layer_top(s_display), &area);
        lv_obj_invalidate_area(lv_display_get_layer_sys(s_display), &area);
    }
    if (s_want) *retry_ms = 20;
    display_unlock();
    return ready;
}

void ui_remote_display_ack(void)
{
    if (!display_lock(0)) return;
    s_ready = false;
    display_unlock();
}

bool ui_remote_display_control(bool enabled)
{
    if (!display_lock(100)) return false;
    cancel_input();
    s_control = enabled && s_open && !s_suspended && !physical_pressed();
    s_heartbeat = esp_timer_get_time();
    bool result = s_control;
    display_unlock();
    return result;
}

bool ui_remote_display_pointer(unsigned kind, int x, int y)
{
    if (!display_lock(100)) return false;
    bool ok = false;
    if (kind == 3) { cancel_input(); ok = true; }
    else if (s_control && !s_suspended && !physical_pressed() &&
             x >= 0 && y >= 0 && x < s_width && y < s_height &&
             ((kind == 0 && !s_queued_down) || ((kind == 1 || kind == 2) && s_queued_down))) {
        if (s_count && kind == 1 && s_inputs[(s_head + s_count - 1) % INPUT_COUNT].kind == 1) --s_count;
        if (s_count == INPUT_COUNT) cancel_input();
        else {
            s_inputs[(s_head + s_count++) % INPUT_COUNT] = (input_t){kind, x, y};
            s_queued_down = kind != 2;
            s_heartbeat = esp_timer_get_time();
            ok = true;
        }
    }
    display_unlock();
    return ok;
}

bool ui_remote_display_heartbeat(void)
{
    if (!display_lock(100)) return false;
    s_heartbeat = esp_timer_get_time();
    bool control = s_control;
    display_unlock();
    return control;
}

void ui_remote_display_suspend(bool suspended)
{
    if (!display_lock(0)) return;
    s_suspended = suspended;
    s_dirty = true;
    s_control = false;
    cancel_input();
    display_unlock();
}

bool ui_remote_display_available(void)
{
    if (!display_lock(100)) return false;
    bool available = s_display && !s_suspended && (!s_open || !s_geometry_changed);
    display_unlock();
    return available;
}
bool ui_remote_display_geometry(uint16_t *width, uint16_t *height)
{
    if (!display_lock(100)) return false;
    bool ok = s_display && !s_suspended;
    if (ok) {
        *width = lv_display_get_horizontal_resolution(s_display);
        *height = lv_display_get_vertical_resolution(s_display);
        ok = *width > 0 && *height > 0 && *width <= 1280 && *height <= 1280;
    }
    display_unlock();
    return ok;
}

#else
bool ui_remote_display_geometry(uint16_t *w, uint16_t *h) { return false; }
esp_err_t ui_remote_display_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t ui_remote_display_open(void) { return ESP_ERR_NOT_SUPPORTED; }
void ui_remote_display_close(void) {}
bool ui_remote_display_capture(const uint8_t **p, uint16_t *w, uint16_t *h, unsigned *retry_ms) { *retry_ms = 200; return false; }
void ui_remote_display_ack(void) {}
bool ui_remote_display_control(bool enabled) { return false; }
bool ui_remote_display_pointer(unsigned k, int x, int y) { return false; }
bool ui_remote_display_heartbeat(void) { return false; }
void ui_remote_display_suspend(bool suspended) {}
bool ui_remote_display_available(void) { return false; }
#endif
