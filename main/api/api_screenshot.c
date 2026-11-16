/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
#include "api/api_routes.h"
#include "api/api_screenshot_capture.h"
#include "api/screenshot_region.h"

#include <limits.h>
#include <inttypes.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "drivers/display_init.h"
#include "lvgl.h"
#if LV_USE_SNAPSHOT
/* Use LVGL 9.6's own cover test, also used by its snapshot/refresh paths. */
#include "src/core/lv_refr_private.h"
#endif
#include "util/log_tags.h"

#define SCREENSHOT_LOCK_TIMEOUT_MS 1500U

static void set_common_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static esp_err_t send_text_error(httpd_req_t *req, const char *status, const char *message)
{
    set_common_headers(req);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, message);
}

static void write_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xffU);
    dst[1] = (uint8_t)((value >> 8) & 0xffU);
}

static void write_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xffU);
    dst[1] = (uint8_t)((value >> 8) & 0xffU);
    dst[2] = (uint8_t)((value >> 16) & 0xffU);
    dst[3] = (uint8_t)((value >> 24) & 0xffU);
}

const uint8_t *screenshot_frame_row(const screenshot_frame_t *frame, uint32_t y)
{
    return frame->bands[y / SCREENSHOT_BAND_ROWS] +
        (size_t)(y % SCREENSHOT_BAND_ROWS) * frame->width * 3U;
}

void screenshot_frame_free(screenshot_frame_t *frame)
{
    for (unsigned i = 0; i < frame->count; i++) {
        heap_caps_free(frame->bands[i]); frame->bands[i] = NULL;
    }
    frame->count = 0;
}

#if LV_USE_SNAPSHOT
bool api_screenshot_capture(const char *format, screenshot_frame_t *frame, bool *busy,
                            uint8_t *workspace, size_t workspace_capacity)
{
    *frame = (screenshot_frame_t){0}; *busy = false;
    const unsigned caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    const char *stage = "display.lock";
    unsigned band = 0;
    int64_t started = esp_timer_get_time(), acquired = 0, wait_us = 0, hold_us = 0;
    bool locked = false, ok = false, own_workspace = false;
    lv_display_t *display = NULL;
    size_t free_psram, largest;
    if (!display_lock(SCREENSHOT_LOCK_TIMEOUT_MS)) { *busy = true; goto finish; }
    acquired = esp_timer_get_time(); wait_us = acquired - started; locked = true;
    stage = "display.geometry";
    display = lv_display_get_default();
    if (!display) goto finish;
    int32_t width = lv_display_get_horizontal_resolution(display);
    int32_t height = lv_display_get_vertical_resolution(display);
    if (width <= 0 || height <= 0 || (uint32_t)width > SCREENSHOT_MAX_DIMENSION || (uint32_t)height > SCREENSHOT_MAX_DIMENSION)
        goto finish;
    frame->width = (uint32_t)width; frame->height = (uint32_t)height;
    /* Allocate outside the display lock. Geometry is revalidated before any
     * rendering, and all bands are captured under one subsequent lock. */
    hold_us += esp_timer_get_time() - acquired;
    display_unlock(); locked = false;
    frame->count = (frame->height + SCREENSHOT_BAND_ROWS - 1) / SCREENSHOT_BAND_ROWS;
    stage = "frame.allocate";
    unsigned frame_caps = caps;
#if CONFIG_SPIRAM_ENC_EXEMPT
    /* The JPEG DMA codec cannot read encrypted PSRAM. */
    frame_caps = MALLOC_CAP_SPIRAM_NO_ENC | MALLOC_CAP_8BIT;
#endif
    for (band = 0; band < frame->count; band++) {
        uint32_t rows = frame->height - band * SCREENSHOT_BAND_ROWS;
        if (rows > SCREENSHOT_BAND_ROWS) rows = SCREENSHOT_BAND_ROWS;
        /* Hardware JPEG input must be DMA-aligned, including short last bands. */
        frame->bands[band] = heap_caps_aligned_alloc(16, (size_t)frame->width * rows * 3U, frame_caps);
        if (!frame->bands[band]) goto finish;
    }
    uint32_t rows = frame->height < SCREENSHOT_BAND_ROWS ? frame->height : SCREENSHOT_BAND_ROWS;
    size_t needed = (size_t)frame->width * rows * 4U;
    stage = "top.allocate";
    if (!workspace) {
        workspace = heap_caps_malloc(needed, caps);
        workspace_capacity = needed; own_workspace = true;
    }
    if (!workspace || workspace_capacity < needed) goto finish;
    stage = "display.lock";
    started = esp_timer_get_time();
    if (!display_lock(SCREENSHOT_LOCK_TIMEOUT_MS)) { *busy = true; goto finish; }
    acquired = esp_timer_get_time(); wait_us += acquired - started; locked = true;
    stage = "display.geometry";
    if (display != lv_display_get_default() ||
        frame->width != (uint32_t)lv_display_get_horizontal_resolution(display) ||
        frame->height != (uint32_t)lv_display_get_vertical_resolution(display)) goto finish;
    lv_obj_t *top = lv_layer_top(), *screen = lv_screen_active();
    lv_obj_update_layout(top); lv_obj_update_layout(screen);
    const lv_area_t full = {0, 0, (int32_t)frame->width - 1, (int32_t)frame->height - 1};
    const bool cover = lv_refr_get_top_obj(&full, top) != NULL;
    for (band = 0; band < frame->count; band++) {
        uint32_t y = band * SCREENSHOT_BAND_ROWS;
        rows = frame->height - y;
        if (rows > SCREENSHOT_BAND_ROWS) rows = SCREENSHOT_BAND_ROWS;
        lv_area_t area = {0, (int32_t)y, (int32_t)frame->width - 1, (int32_t)(y + rows) - 1};
        size_t pixels = (size_t)frame->width * rows;
        uint8_t *rgb = frame->bands[band];
        if (cover) {
            stage = "cover.render";
            if (!screenshot_render_region(top, LV_COLOR_FORMAT_RGB888, &area, rgb, pixels * 3U)) goto finish;
        } else {
            stage = "top.render";
            if (!screenshot_render_region(top, LV_COLOR_FORMAT_ARGB8888, &area, workspace, pixels * 4U)) goto finish;
            stage = "screen.render";
            if (!screenshot_render_region(screen, LV_COLOR_FORMAT_RGB888, &area, rgb, pixels * 3U)) goto finish;
            for (size_t p = 0; p < pixels; p++) {
                uint32_t a = workspace[p * 4U + 3U];
                if (!a) continue;
                for (unsigned c = 0; c < 3; c++)
                    rgb[p * 3U + c] = (uint8_t)((workspace[p * 4U + c] * a + rgb[p * 3U + c] * (255U - a)) / 255U);
            }
        }
    }
    ok = true; stage = "complete";
finish:
    if (*busy) wait_us += esp_timer_get_time() - started;
    /* Keep failure evidence before releasing owned memory. These are samples,
     * not an atomic reservation or a whole-device peak. */
    free_psram = heap_caps_get_free_size(caps);
    largest = heap_caps_get_largest_free_block(caps);
    if (locked) { hold_us += esp_timer_get_time() - acquired; display_unlock(); }
    ESP_LOGI(TAG_HTTP, "Screenshot %s capture=%s stage=%s band=%u size=%" PRIu32 "x%" PRIu32
             " psram_free=%zu psram_largest=%zu wait_us=%" PRId64 " hold_us=%" PRId64,
             format, ok ? "ok" : "failed", stage, band, frame->width, frame->height,
             free_psram, largest, wait_us, hold_us);
    if (own_workspace) heap_caps_free(workspace);
    if (!ok) screenshot_frame_free(frame);
    return ok;
}
#endif

esp_err_t api_screenshot_bmp_get_handler(httpd_req_t *req)
{
#if !LV_USE_SNAPSHOT
    return send_text_error(req, "501 Not Implemented", "LVGL snapshot support is disabled");
#else
    screenshot_frame_t frame;
    bool busy;
    if (!api_screenshot_capture("BMP", &frame, &busy, NULL, 0))
        return send_text_error(req, busy ? "503 Service Unavailable" : "500 Internal Server Error",
                               busy ? "Display is busy" : "Failed to capture screenshot");
    const uint32_t row_bytes = frame.width * 3U;
    const uint32_t stride = (row_bytes + 3U) & ~3U;
    uint8_t header[54] = {'B', 'M'};
    write_u32_le(header + 2, 54U + stride * frame.height);
    write_u32_le(header + 10, 54); write_u32_le(header + 14, 40);
    write_u32_le(header + 18, frame.width); write_u32_le(header + 22, frame.height);
    write_u16_le(header + 26, 1); write_u16_le(header + 28, 24);
    write_u32_le(header + 34, stride * frame.height);
    write_u32_le(header + 38, 2835); write_u32_le(header + 42, 2835);
    set_common_headers(req); httpd_resp_set_type(req, "image/bmp");
    esp_err_t err = httpd_resp_send_chunk(req, (const char *)header, sizeof(header));
    static const char padding[3] = {0};
    for (uint32_t y = frame.height; y && err == ESP_OK; y--) {
        err = httpd_resp_send_chunk(req, (const char *)screenshot_frame_row(&frame, y - 1), row_bytes);
        if (err == ESP_OK && stride != row_bytes) err = httpd_resp_send_chunk(req, padding, stride - row_bytes);
    }
    if (err == ESP_OK) err = httpd_resp_send_chunk(req, NULL, 0);
    screenshot_frame_free(&frame);
    return err;
#endif
}
