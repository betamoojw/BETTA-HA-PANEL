/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
#include "api/api_routes.h"
#include "api/api_screenshot_capture.h"
#include "api/screenshot_jpeg_join.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "zlib.h"
#include "soc/soc_caps.h"
#if SOC_JPEG_ENCODE_SUPPORTED
#include "driver/jpeg_encode.h"
#endif

#include "drivers/display_init.h"
#include "lvgl.h"
#include "util/log_tags.h"

#define SCREENSHOT_JPEG_QUALITY 90U
#define SCREENSHOT_JPEG_TIMEOUT_MS 4000
/* Upper bound of the compressed image; a larger result fails the request. */
#define SCREENSHOT_JPEG_MAX_BYTES (1024U * 1024U)
#define SCREENSHOT_PNG_MAX_BYTES (1024U * 1024U)
#define SCREENSHOT_PNG_LEVEL 1

static void image_set_common_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static esp_err_t image_send_text_error(httpd_req_t *req, const char *status, const char *message)
{
    image_set_common_headers(req);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, message);
}

#if LV_USE_SNAPSHOT && SOC_JPEG_ENCODE_SUPPORTED
/* Each band is a complete MCU-row-aligned image. Joining is validated before
 * publishing any bytes; codec waits and all entropy work occur outside LVGL. */
static uint32_t encode_jpeg(const screenshot_frame_t *frame, uint8_t *out,
                            uint8_t *scratch, size_t scratch_capacity)
{
    jpeg_encoder_handle_t engine = NULL;
    const jpeg_encode_engine_cfg_t engine_cfg = {.intr_priority = 0, .timeout_ms = SCREENSHOT_JPEG_TIMEOUT_MS};
    if (jpeg_new_encoder_engine(&engine_cfg, &engine) != ESP_OK) return 0;
    screenshot_jpeg_join_t join;
    bool ok = screenshot_jpeg_join_init(&join, out, SCREENSHOT_JPEG_MAX_BYTES,
                                        frame->width, frame->height, SCREENSHOT_BAND_ROWS);
    for (uint32_t i = 0; i < frame->count && ok; i++) {
        uint32_t rows = frame->height - i * SCREENSHOT_BAND_ROWS;
        if (rows > SCREENSHOT_BAND_ROWS) rows = SCREENSHOT_BAND_ROWS;
        const jpeg_encode_cfg_t cfg = {
            .height = rows, .width = frame->width, .src_type = JPEG_ENCODE_IN_FORMAT_RGB888,
            .sub_sample = JPEG_DOWN_SAMPLING_YUV444, .image_quality = SCREENSHOT_JPEG_QUALITY,
            .pixel_reverse = false,
        };
        uint32_t size = 0;
        ok = jpeg_encoder_process(engine, &cfg, frame->bands[i], frame->width * rows * 3U,
                                  scratch, (uint32_t)scratch_capacity, &size) == ESP_OK &&
             size >= 4 && size <= scratch_capacity && screenshot_jpeg_join_append(&join, scratch, size);
    }
    jpeg_del_encoder_engine(engine);
    return ok && join.rows == frame->height ? (uint32_t)join.used : 0;
}
#endif

#if LV_USE_SNAPSHOT
static void *png_zalloc(void *opaque, unsigned items, unsigned size)
{
    (void)opaque;
    return heap_caps_malloc((size_t)items * size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void png_zfree(void *opaque, void *ptr)
{
    (void)opaque;
    heap_caps_free(ptr);
}

static void png_put_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
}

/* Lossless PNG of the RGB888 snapshot: filter 0 and zlib at a fast level (UI screens are flat areas and compress well).
 * Writes into the bounded caller-owned output; returns zero on any failure. */
static uint32_t encode_png(const screenshot_frame_t *frame, uint8_t *out)
{
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    enum { HEAD = 8 + 25 + 8, TAIL = 4 + 12 };
    const uint32_t w = frame->width;
    const uint32_t h = frame->height;
    uint8_t *row = heap_caps_malloc(1U + (size_t)w * 3U, MALLOC_CAP_8BIT);
    if (row == NULL) return 0;
    z_stream z;
    memset(&z, 0, sizeof(z));
    z.zalloc = png_zalloc;
    z.zfree = png_zfree;
    if (deflateInit2(&z, SCREENSHOT_PNG_LEVEL, Z_DEFLATED, 12, 5, Z_DEFAULT_STRATEGY) != Z_OK) {
        heap_caps_free(row);
        return 0;
    }
    z.next_out = out + HEAD;
    z.avail_out = SCREENSHOT_PNG_MAX_BYTES - HEAD - TAIL;
    /* LVGL's RGB888 is stored as B, G, R; PNG wants R, G, B. Each row is swapped into a small buffer behind its filter byte. */
    bool ok = true;
    for (uint32_t y = 0; y < h && ok; y++) {
        const uint8_t *src = screenshot_frame_row(frame, y);
        row[0] = 0;
        for (uint32_t x = 0; x < w; x++) {
            row[1 + x * 3U] = src[x * 3U + 2U];
            row[2 + x * 3U] = src[x * 3U + 1U];
            row[3 + x * 3U] = src[x * 3U];
        }
        z.next_in = row;
        z.avail_in = 1U + w * 3U;
        ok = deflate(&z, Z_NO_FLUSH) == Z_OK && z.avail_in == 0;
    }
    heap_caps_free(row);
    ok = ok && deflate(&z, Z_FINISH) == Z_STREAM_END;
    const uint32_t idat_len = (uint32_t)z.total_out;
    deflateEnd(&z);
    if (!ok) return 0;
    memcpy(out, signature, 8);
    png_put_u32(out + 8, 13);
    memcpy(out + 12, "IHDR", 4);
    png_put_u32(out + 16, w);
    png_put_u32(out + 20, h);
    out[24] = 8; /* bit depth */
    out[25] = 2; /* RGB */
    out[26] = out[27] = out[28] = 0;
    png_put_u32(out + 29, (uint32_t)crc32(0, out + 12, 17));
    png_put_u32(out + 33, idat_len);
    memcpy(out + 37, "IDAT", 4);
    png_put_u32(out + HEAD + idat_len, (uint32_t)crc32(0, out + 37, idat_len + 4U));
    uint8_t *end = out + HEAD + idat_len + 4;
    png_put_u32(end, 0);
    memcpy(end + 4, "IEND", 4);
    png_put_u32(end + 8, (uint32_t)crc32(0, end + 4, 4));
    return HEAD + idat_len + TAIL;
}
#endif

static esp_err_t screenshot_handle(httpd_req_t *req, bool png)
{
#if !LV_USE_SNAPSHOT
    return image_send_text_error(req, "501 Not Implemented", "LVGL snapshot support is disabled");
#else
#if !SOC_JPEG_ENCODE_SUPPORTED
    if (!png) return image_send_text_error(req, "501 Not Implemented", "JPEG screenshots are unavailable; use PNG");
#endif
    const int64_t t0 = esp_timer_get_time();
    uint8_t *image = heap_caps_malloc(SCREENSHOT_MAX_ENCODED, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *scratch = NULL;
    size_t scratch_capacity = 0;
#if SOC_JPEG_ENCODE_SUPPORTED
    if (!png && image) {
        const jpeg_encode_memory_alloc_cfg_t mem = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};
        scratch = jpeg_alloc_encoder_mem(SCREENSHOT_MAX_ENCODED, &mem, &scratch_capacity);
    }
#endif
    /* Reserve the bounded output/scratch before fragment-sized RGB allocations. */
    if (!image || (!png && (!scratch || scratch_capacity < SCREENSHOT_MAX_ENCODED))) {
        heap_caps_free(scratch); heap_caps_free(image);
        return image_send_text_error(req, "500 Internal Server Error", "Failed to allocate screenshot output");
    }
    screenshot_frame_t frame;
    bool busy;
    if (!api_screenshot_capture(png ? "PNG" : "JPEG", &frame, &busy, scratch, scratch_capacity)) {
        heap_caps_free(scratch); heap_caps_free(image);
        return image_send_text_error(req, busy ? "503 Service Unavailable" : "500 Internal Server Error",
                                     busy ? "Display is busy" : "Failed to capture screenshot");
    }
    const int64_t t_snap = esp_timer_get_time();
    uint32_t len = png ? encode_png(&frame, image) : 0;
#if SOC_JPEG_ENCODE_SUPPORTED
    if (!png) len = encode_jpeg(&frame, image, scratch, scratch_capacity);
#endif
    const int64_t t_enc = esp_timer_get_time();
    screenshot_frame_free(&frame); heap_caps_free(scratch);
    if (!len) {
        heap_caps_free(image);
        ESP_LOGW(TAG_HTTP, "Screenshot %s encoding failed", png ? "PNG" : "JPEG");
        return image_send_text_error(req, "500 Internal Server Error", "Failed to encode screenshot");
    }
    image_set_common_headers(req);
    httpd_resp_set_type(req, png ? "image/png" : "image/jpeg");
    const esp_err_t err = httpd_resp_send(req, (const char *)image, (ssize_t)len);
    heap_caps_free(image);
    ESP_LOGI(TAG_HTTP, "Screenshot %s %u B: capture %d ms, encode %d ms, send %d ms", png ? "PNG" : "JPEG", (unsigned)len,
             (int)((t_snap - t0) / 1000), (int)((t_enc - t_snap) / 1000), (int)((esp_timer_get_time() - t_enc) / 1000));
    return err;
#endif
}

esp_err_t api_screenshot_jpg_get_handler(httpd_req_t *req)
{
    return screenshot_handle(req, false);
}

esp_err_t api_screenshot_png_get_handler(httpd_req_t *req)
{
    return screenshot_handle(req, true);
}
