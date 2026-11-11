/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
#include "api/api_routes.h"
#include "api/api_screenshot_capture.h"

#include <limits.h>
#include <stdint.h>

#include "esp_log.h"

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

#if LV_USE_SNAPSHOT
static bool snapshot_valid(const lv_draw_buf_t *buf, uint32_t width, uint32_t height,
                           lv_color_format_t format, uint32_t bytes_per_pixel)
{
    return buf && buf->data && buf->header.cf == format &&
        buf->header.w == width && buf->header.h == height &&
        buf->header.stride >= (uint64_t)width * bytes_per_pixel &&
        buf->data_size >= (uint64_t)buf->header.stride * height;
}

/* Caller holds the display lock. Never publish a base-only image if capturing
 * an overlay fails: allocation/render failures must remain visible to clients. */
lv_draw_buf_t *api_screenshot_capture_locked(void)
{
    lv_display_t *display = lv_display_get_default();
    if (!display) return NULL;
    const int32_t width = lv_display_get_horizontal_resolution(display);
    const int32_t height = lv_display_get_vertical_resolution(display);
    if (width <= 0 || height <= 0) return NULL;
    lv_draw_buf_t *screen = NULL, *top = NULL;
    lv_obj_t *top_layer = lv_layer_top();
    lv_obj_update_layout(top_layer);
    const lv_area_t area = {.x1 = 0, .y1 = 0, .x2 = width - 1, .y2 = height - 1};
    /* Full-screen protection already hides the dashboard. Capture it directly
     * with one RGB buffer. Do not infer opacity from rendered alpha: rounded
     * alpha blends can leave values just below 255 even on an opaque surface. */
    if (lv_refr_get_top_obj(&area, top_layer) != NULL) {
        screen = lv_snapshot_take(top_layer, LV_COLOR_FORMAT_RGB888);
        if (!snapshot_valid(screen, width, height, LV_COLOR_FORMAT_RGB888, 3)) goto failed;
        return screen;
    }
    top = lv_snapshot_take(top_layer, LV_COLOR_FORMAT_ARGB8888);
    if (!snapshot_valid(top, width, height, LV_COLOR_FORMAT_ARGB8888, 4)) goto failed;

    bool transparent = true;
    for (int32_t y = 0; y < height && transparent; y++) {
        const uint8_t *pixel = top->data + (size_t)y * top->header.stride;
        for (int32_t x = 0; x < width; x++, pixel += 4) {
            transparent = transparent && pixel[3] == 0;
        }
    }
    if (transparent) { /* Empty/hidden overlays need no concurrent second buffer. */
        lv_draw_buf_destroy(top);
        top = NULL;
    }
    screen = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    if (!snapshot_valid(screen, width, height, LV_COLOR_FORMAT_RGB888, 3)) goto failed;
    if (top) {
        for (int32_t y = 0; y < height; y++) {
            uint8_t *dst = screen->data + (size_t)y * screen->header.stride;
            const uint8_t *src = top->data + (size_t)y * top->header.stride;
            for (int32_t x = 0; x < width; x++, dst += 3, src += 4) {
                const uint32_t a = src[3]; /* LVGL snapshot bytes are straight B,G,R,A. */
                for (unsigned channel = 0; channel < 3; channel++)
                    dst[channel] = (uint8_t)((src[channel] * a + dst[channel] * (255U - a)) / 255U);
            }
        }
        lv_draw_buf_destroy(top);
    }
    return screen;

failed:
    if (top) lv_draw_buf_destroy(top);
    if (screen) lv_draw_buf_destroy(screen);
    return NULL;
}
#endif

esp_err_t api_screenshot_bmp_get_handler(httpd_req_t *req)
{
#if !LV_USE_SNAPSHOT
    return send_text_error(req, "501 Not Implemented", "LVGL snapshot support is disabled");
#else
    lv_draw_buf_t *snapshot = NULL;
    if (!display_lock(SCREENSHOT_LOCK_TIMEOUT_MS)) {
        ESP_LOGW(TAG_HTTP, "Screenshot request failed: could not lock display");
        return send_text_error(req, "503 Service Unavailable", "Display is busy");
    }

    snapshot = api_screenshot_capture_locked();
    display_unlock();

    if (snapshot == NULL) {
        ESP_LOGW(TAG_HTTP, "Screenshot request failed: incomplete or invalid layer capture");
        return send_text_error(req, "500 Internal Server Error", "Failed to capture screenshot");
    }

    const uint32_t width = snapshot->header.w;
    const uint32_t height = snapshot->header.h;
    const uint32_t src_row_bytes = width * 3U;
    const uint32_t src_stride = snapshot->header.stride;

    if (width == 0 || height == 0 || snapshot->data == NULL || src_stride < src_row_bytes) {
        lv_draw_buf_destroy(snapshot);
        return send_text_error(req, "500 Internal Server Error", "Invalid screenshot buffer");
    }

    const uint32_t bmp_row_stride = (src_row_bytes + 3U) & ~3U;
    const uint32_t pad_len = bmp_row_stride - src_row_bytes;
    const uint64_t pixel_data_size_64 = (uint64_t)bmp_row_stride * (uint64_t)height;
    const uint64_t file_size_64 = 54ULL + pixel_data_size_64;

    if (pixel_data_size_64 > UINT32_MAX || file_size_64 > UINT32_MAX || width > INT32_MAX || height > INT32_MAX) {
        lv_draw_buf_destroy(snapshot);
        return send_text_error(req, "500 Internal Server Error", "Screenshot is too large");
    }

    uint8_t bmp_header[54] = {0};
    bmp_header[0] = 'B';
    bmp_header[1] = 'M';
    write_u32_le(&bmp_header[2], (uint32_t)file_size_64);
    write_u32_le(&bmp_header[10], 54U);
    write_u32_le(&bmp_header[14], 40U);
    write_u32_le(&bmp_header[18], width);
    write_u32_le(&bmp_header[22], height);
    write_u16_le(&bmp_header[26], 1U);
    write_u16_le(&bmp_header[28], 24U);
    write_u32_le(&bmp_header[34], (uint32_t)pixel_data_size_64);
    write_u32_le(&bmp_header[38], 2835U);
    write_u32_le(&bmp_header[42], 2835U);

    set_common_headers(req);
    httpd_resp_set_type(req, "image/bmp");

    esp_err_t err = httpd_resp_send_chunk(req, (const char *)bmp_header, sizeof(bmp_header));
    if (err != ESP_OK) {
        lv_draw_buf_destroy(snapshot);
        return err;
    }

    static const uint8_t pad_bytes[3] = {0, 0, 0};
    for (int32_t y = (int32_t)height - 1; y >= 0; y--) {
        const uint8_t *row = snapshot->data + ((uint32_t)y * src_stride);
        err = httpd_resp_send_chunk(req, (const char *)row, src_row_bytes);
        if (err != ESP_OK) {
            lv_draw_buf_destroy(snapshot);
            return err;
        }

        if (pad_len > 0U) {
            err = httpd_resp_send_chunk(req, (const char *)pad_bytes, pad_len);
            if (err != ESP_OK) {
                lv_draw_buf_destroy(snapshot);
                return err;
            }
        }
    }

    err = httpd_resp_send_chunk(req, NULL, 0);
    lv_draw_buf_destroy(snapshot);
    return err;
#endif
}
