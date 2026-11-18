/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 betamoojw
 * Adapted by andre68723 from betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3,
 * commit 57df6a3, for the shared LVGL 9 adapter and all panel profiles.
 */
#include "ui/ui_remote_display.h"
#if CONFIG_APP_REMOTE_DISPLAY
#include <stdatomic.h>
#include <string.h>
#include "drivers/display_init.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl.h"

#define INPUT_COUNT 24
#define CONTROL_LEASE_MS 1000U
#define FRAME_US 200000LL
#define BAND_ROWS 8U
#define MAX_TILE_COLS ((UI_REMOTE_MAX_DIMENSION + UI_REMOTE_TILE_SIZE - 1) / UI_REMOTE_TILE_SIZE)
#define FRAME_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#if CONFIG_APP_PANEL_VARIANT_S3_480
#define PSRAM_RESERVE (384U * 1024U)
#else
#define PSRAM_RESERVE (512U * 1024U)
#endif

struct ui_remote_frame {
    uint16_t width, tile_cols;
    uint8_t changed[(MAX_TILE_COLS * MAX_TILE_COLS + 7) / 8];
    uint8_t *bands[(UI_REMOTE_MAX_DIMENSION + BAND_ROWS - 1) / BAND_ROWS];
};

enum mirror_status { NOT_STARTED, DASHBOARD_STARTING, RETRY_INIT, AVAILABLE, UNAVAILABLE, UNSUPPORTED };
static atomic_int s_status = NOT_STARTED;
typedef struct { unsigned kind; int16_t x, y; } input_t;
static lv_display_t *s_display;
static lv_display_rotation_t s_rotation;
static bool s_geometry_changed, s_capturing;
static int s_next_capture_row;
static lv_area_t s_pending_area, s_requested_area;
static bool s_pending;
static lv_indev_t *s_remote;
static ui_remote_frame_t *s_frame;
static uint16_t s_width, s_height;
static bool s_open, s_ready, s_want, s_dirty, s_down, s_queued_down, s_suspended;
static _Atomic uint32_t s_last_control_ms;
static int64_t s_last_capture;
static lv_point_t s_point;
static input_t s_inputs[INPUT_COUNT];
static unsigned s_head, s_count;
static bool s_cancel_reset;

/* Publish readiness without taking the display lock for every WebSocket
 * message. A busy renderer must not look like a disconnected display. */
static void publish_status(void)
{
    if (s_display)
        atomic_store(&s_status, !s_suspended && (!s_open || !s_geometry_changed) ? AVAILABLE : UNAVAILABLE);
}

bool ui_remote_display_memory_fits(uint16_t width, uint16_t height, uint16_t packet_bytes, uint16_t tile_bytes)
{
    if (!width || !height || width > UI_REMOTE_MAX_DIMENSION || height > UI_REMOTE_MAX_DIMENSION) return false;
    size_t bytes = (size_t)width * height * 2U + sizeof(ui_remote_frame_t) + PSRAM_RESERVE;
    size_t free_bytes = heap_caps_get_free_size(FRAME_CAPS);
    size_t largest = heap_caps_get_largest_free_block(FRAME_CAPS);
    size_t band = (size_t)width * LV_MIN(height, BAND_ROWS) * 2U;
    return free_bytes >= bytes + packet_bytes + tile_bytes &&
           largest >= LV_MAX(LV_MAX(band, sizeof(ui_remote_frame_t)), LV_MAX(packet_bytes, tile_bytes));
}

static void free_frame(ui_remote_frame_t *frame)
{
    if (!frame) return;
    for (unsigned i = 0; i < sizeof(frame->bands) / sizeof(frame->bands[0]); ++i)
        heap_caps_free(frame->bands[i]);
    heap_caps_free(frame);
}

static ui_remote_frame_t *allocate_frame(uint16_t width, uint16_t height)
{
    if (!ui_remote_display_memory_fits(width, height, 0, 0)) return NULL;
    ui_remote_frame_t *frame = heap_caps_calloc(1, sizeof(*frame), FRAME_CAPS);
    if (!frame) return NULL;
    frame->width = width;
    frame->tile_cols = (width + UI_REMOTE_TILE_SIZE - 1) / UI_REMOTE_TILE_SIZE;
    memset(frame->changed, 0xff, sizeof(frame->changed)); /* First frame always includes every tile. */
    for (unsigned y = 0; y < height; y += BAND_ROWS) {
        size_t bytes = (size_t)width * LV_MIN(height - y, BAND_ROWS) * 2U;
        frame->bands[y / BAND_ROWS] = heap_caps_calloc(1, bytes, FRAME_CAPS);
        if (!frame->bands[y / BAND_ROWS]) {
            free_frame(frame);
            return NULL;
        }
    }
    if (heap_caps_get_free_size(FRAME_CAPS) < PSRAM_RESERVE) {
        free_frame(frame);
        return NULL;
    }
    return frame;
}

static uint8_t *frame_row(const ui_remote_frame_t *frame, unsigned y)
{
    return frame->bands[y / BAND_ROWS] + (y % BAND_ROWS) * frame->width * 2U;
}

const uint8_t *ui_remote_frame_row(const ui_remote_frame_t *frame, unsigned y)
{
    return frame_row(frame, y);
}

bool ui_remote_frame_tile_changed(const ui_remote_frame_t *frame, unsigned tile)
{
    return (frame->changed[tile / 8] & (1U << (tile % 8))) != 0;
}

/* The mirror itself holds the last acknowledged pixels. Mark tiles before
 * overwriting their rows so the transport needs no second RGB565 image. */
static void copy_row(unsigned y, unsigned x, unsigned end, const uint8_t *src)
{
    uint8_t *dst = frame_row(s_frame, y) + x * 2U;
    while (x < end) {
        unsigned pixels = LV_MIN(end - x, UI_REMOTE_TILE_SIZE - x % UI_REMOTE_TILE_SIZE);
        unsigned tile = (y / UI_REMOTE_TILE_SIZE) * s_frame->tile_cols + x / UI_REMOTE_TILE_SIZE;
        size_t bytes = pixels * 2U;
        if (ui_remote_frame_tile_changed(s_frame, tile) || memcmp(dst, src, bytes) != 0) {
            s_frame->changed[tile / 8] |= 1U << (tile % 8);
            memcpy(dst, src, bytes);
        }
        dst += bytes;
        src += bytes;
        x += pixels;
    }
}

/* Zero revokes control. Odd millisecond ticks reserve zero and let unsigned
 * subtraction measure the one-second lease across a 32-bit clock wrap. */
static uint32_t control_clock_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000) | 1U;
}

static bool control_lease_active(bool renew)
{
    uint32_t last = atomic_load(&s_last_control_ms);
    while (last) {
        uint32_t now = control_clock_ms();
        bool active = now - last <= CONTROL_LEASE_MS;
        if (active && !renew) return true;
        /* A heartbeat must never undo a simultaneous physical touch or revoke. */
        if (atomic_compare_exchange_strong(&s_last_control_ms, &last, active ? now : 0))
            return active;
    }
    return false;
}

/* LVGL operations and the pointer queue are serialized by display_lock. */
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
    atomic_store(&s_last_control_ms, 0);
    cancel_input();
}

static void remote_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    bool allowed = control_lease_active(false);
    if (allowed && physical_pressed()) {
        atomic_store(&s_last_control_ms, 0);
        allowed = false;
    }
    if (!allowed && (s_down || s_count || s_queued_down)) cancel_input();
    if (allowed && s_count) {
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
        publish_status();
        atomic_store(&s_last_control_ms, 0);
        cancel_input();
        return;
    }
    if (!s_open || s_suspended || s_geometry_changed) return;
    if (code == LV_EVENT_REFR_START) {
        s_capturing = s_want && !s_ready;
        s_next_capture_row = s_requested_area.y1;
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
            copy_row(y, x1, x2 + 1, src);
            src += buf->header.stride;
        }
        /* LVGL flushes an invalidated rectangle whole or in top-to-bottom,
         * full-width strips. Duplicate flushes cannot stand in for missing rows. */
        if (x1 <= s_requested_area.x1 && x2 >= s_requested_area.x2 &&
            y1 <= s_next_capture_row && y2 >= s_next_capture_row)
            s_next_capture_row = y2 + 1;
    } else if (code == LV_EVENT_REFR_READY && s_capturing) {
        s_capturing = false;
        s_ready = s_next_capture_row > s_requested_area.y2;
        if (!s_ready) {
            /* An incomplete refresh may already have overwritten a few rows.
             * Reconstruct the whole image before publishing those changes. */
            s_pending = true;
            s_pending_area = (lv_area_t){0, 0, s_width - 1, s_height - 1};
        }
        s_want = false;
        s_dirty = !s_ready;
        s_last_capture = esp_timer_get_time();
    }
}

static esp_err_t attach_locked(void)
{
    if (s_display) return ESP_OK;
    lv_display_t *d = lv_display_get_default();
    if (!d) return ESP_ERR_INVALID_STATE;
    if (lv_display_get_color_format(d) != LV_COLOR_FORMAT_RGB565) {
        atomic_store(&s_status, UNSUPPORTED);
        return ESP_ERR_NOT_SUPPORTED;
    }
    s_remote = lv_indev_create();
    if (!s_remote) return ESP_ERR_NO_MEM;
    lv_indev_set_type(s_remote, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(s_remote, d);
    lv_indev_set_read_cb(s_remote, remote_read);
    if (!lv_indev_get_read_timer(s_remote) ||
        !lv_indev_add_event_cb(s_remote, remote_activity, LV_EVENT_ALL, NULL)) goto allocation_failed;
    lv_timer_set_period(lv_indev_get_read_timer(s_remote), 10);
    lv_indev_enable(s_remote, false);
    lv_timer_pause(lv_indev_get_read_timer(s_remote));
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i)) {
        if (i != s_remote && lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER &&
            !lv_indev_add_event_cb(i, physical_event, LV_EVENT_PRESSED, NULL)) goto allocation_failed;
    }
    if (!lv_display_add_event_cb(d, mirror_event, LV_EVENT_ALL, NULL)) goto allocation_failed;
    s_display = d;
    publish_status();
    return ESP_OK;

allocation_failed:
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i))
        lv_indev_remove_event_cb_with_user_data(i, physical_event, NULL);
    lv_indev_delete(s_remote);
    s_remote = NULL;
    return ESP_ERR_NO_MEM;
}

void ui_remote_display_prepare_dashboard(void)
{
    /* Publish eligibility before HTTP starts. Touch registration finishes
     * before init arms actual attachment, even if the browser arrives early. */
    atomic_store(&s_status, DASHBOARD_STARTING);
}

bool ui_remote_display_supported(void)
{
    int status = atomic_load(&s_status);
    return status != NOT_STARTED && status != UNSUPPORTED;
}

esp_err_t ui_remote_display_init(void)
{
    /* Only dashboard startup arms retries. Setup/recovery modes stay closed. */
    int status = atomic_load(&s_status);
    if (status == NOT_STARTED || status == DASHBOARD_STARTING) atomic_store(&s_status, RETRY_INIT);
    if (!display_lock(1000)) return ESP_ERR_TIMEOUT;
    esp_err_t err = attach_locked();
    display_unlock();
    return err;
}

esp_err_t ui_remote_display_open(uint16_t width, uint16_t height)
{
    if (!display_lock(1000)) return ESP_ERR_TIMEOUT;
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_display && !s_open && !s_suspended &&
        width == lv_display_get_horizontal_resolution(s_display) &&
        height == lv_display_get_vertical_resolution(s_display)) {
        s_width = width;
        s_height = height;
        s_rotation = lv_display_get_rotation(s_display);
        s_geometry_changed = false;
        s_frame = allocate_frame(s_width, s_height);
        err = s_frame ? ESP_OK : ESP_ERR_NO_MEM;
        if (s_frame) {
            lv_indev_enable(s_remote, true);
            lv_timer_resume(lv_indev_get_read_timer(s_remote));
            s_open = s_dirty = true;
            s_ready = s_want = false;
            atomic_store(&s_last_control_ms, 0);
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
    s_open = s_ready = s_want = s_capturing = false;
    atomic_store(&s_last_control_ms, 0);
    cancel_input();
    if (s_remote) {
        lv_indev_enable(s_remote, false);
        lv_timer_pause(lv_indev_get_read_timer(s_remote));
    }
    free_frame(s_frame);
    s_frame = NULL;
    publish_status();
    display_unlock();
}

bool ui_remote_display_capture(const ui_remote_frame_t **frame, uint16_t *width, uint16_t *height, unsigned *retry_ms)
{
    *retry_ms = 200;
    if (!display_lock(100)) return false;
    bool ready = s_open && s_ready && !s_suspended;
    if (ready) { *frame = s_frame; *width = s_width; *height = s_height; }
    else if (s_open && !s_suspended && s_dirty && !s_want &&
             esp_timer_get_time() - s_last_capture >= FRAME_US) {
        s_want = true;
        /* Keep the acknowledged pixels and replay only regions missed while the
         * immutable frame was in flight. A full-screen redraw on every clock or
         * sensor update competes with continuous RGB scanout on the S3. */
        lv_area_t area = s_pending ? s_pending_area :
            (lv_area_t){0, 0, s_width - 1, s_height - 1};
        s_requested_area = area;
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
    if (s_frame) memset(s_frame->changed, 0, sizeof(s_frame->changed));
    s_ready = false;
    display_unlock();
}

bool ui_remote_display_control(bool enabled)
{
    /* If the renderer is busy, its next input read discards the old gesture. */
    atomic_store(&s_last_control_ms, 0);
    if (!display_lock(100)) return false;
    cancel_input();
    bool result = enabled && s_open && !s_suspended && !physical_pressed();
    if (result) atomic_store(&s_last_control_ms, control_clock_ms());
    display_unlock();
    return result;
}

bool ui_remote_display_pointer(unsigned kind, int x, int y)
{
    if (!display_lock(100)) {
        atomic_store(&s_last_control_ms, 0);
        return false;
    }
    bool ok = false;
    if (kind == 3) { cancel_input(); ok = true; }
    else if (!s_suspended && !physical_pressed() &&
             x >= 0 && y >= 0 && x < s_width && y < s_height &&
             ((kind == 0 && !s_queued_down) || ((kind == 1 || kind == 2) && s_queued_down)) &&
             control_lease_active(true)) {
        if (s_count && kind == 1 && s_inputs[(s_head + s_count - 1) % INPUT_COUNT].kind == 1) --s_count;
        if (s_count == INPUT_COUNT) cancel_input();
        else {
            s_inputs[(s_head + s_count++) % INPUT_COUNT] = (input_t){kind, x, y};
            s_queued_down = kind != 2;
            ok = true;
        }
    }
    display_unlock();
    return ok;
}

bool ui_remote_display_heartbeat(void)
{
    return control_lease_active(true);
}

void ui_remote_display_suspend(bool suspended)
{
    if (!display_lock(0)) return;
    s_suspended = suspended;
    publish_status();
    s_dirty = true;
    atomic_store(&s_last_control_ms, 0);
    cancel_input();
    display_unlock();
}

bool ui_remote_display_available(void)
{
    if (atomic_load(&s_status) == RETRY_INIT && display_lock(100)) {
        (void)attach_locked();
        display_unlock();
    }
    return atomic_load(&s_status) == AVAILABLE;
}
bool ui_remote_display_geometry(uint16_t *width, uint16_t *height)
{
    if (!display_lock(100)) return false;
    bool ok = s_display && !s_suspended;
    if (ok) {
        *width = lv_display_get_horizontal_resolution(s_display);
        *height = lv_display_get_vertical_resolution(s_display);
        ok = *width > 0 && *height > 0 && *width <= UI_REMOTE_MAX_DIMENSION && *height <= UI_REMOTE_MAX_DIMENSION;
    }
    display_unlock();
    return ok;
}

#else
void ui_remote_display_prepare_dashboard(void) {}
bool ui_remote_display_supported(void) { return false; }
bool ui_remote_display_geometry(uint16_t *w, uint16_t *h) { return false; }
esp_err_t ui_remote_display_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t ui_remote_display_open(uint16_t width, uint16_t height) { return ESP_ERR_NOT_SUPPORTED; }
void ui_remote_display_close(void) {}
bool ui_remote_display_capture(const ui_remote_frame_t **p, uint16_t *w, uint16_t *h, unsigned *retry_ms) { *retry_ms = 200; return false; }
const uint8_t *ui_remote_frame_row(const ui_remote_frame_t *frame, unsigned y) { return NULL; }
bool ui_remote_frame_tile_changed(const ui_remote_frame_t *frame, unsigned tile) { return false; }
bool ui_remote_display_memory_fits(uint16_t width, uint16_t height, uint16_t packet_bytes, uint16_t tile_bytes) { return false; }
void ui_remote_display_ack(void) {}
bool ui_remote_display_control(bool enabled) { return false; }
bool ui_remote_display_pointer(unsigned k, int x, int y) { return false; }
bool ui_remote_display_heartbeat(void) { return false; }
void ui_remote_display_suspend(bool suspended) {}
bool ui_remote_display_available(void) { return false; }
#endif
