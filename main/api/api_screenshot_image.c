/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief PNG and board-dependent hardware JPEG encoding for captured frames.
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
#  include "driver/jpeg_encode.h"
#endif

#include "drivers/display_init.h"
#include "lvgl.h"
#include "util/log_tags.h"

#define SCREENSHOT_JPEG_QUALITY    90U
#define SCREENSHOT_JPEG_TIMEOUT_MS 4000
/* Upper bound of the compressed image; a larger result fails the request. */
#define SCREENSHOT_JPEG_MAX_BYTES    (1024U * 1024U)
#define SCREENSHOT_PNG_MAX_BYTES     (1024U * 1024U)
#define SCREENSHOT_PNG_LEVEL         1
#define SCREENSHOT_PNG_BLOCK_BYTES   8192U
#define SCREENSHOT_PNG_DEFLATE_BYTES 4096U

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
static uint32_t encode_jpeg(const screenshot_frame_t *frame, uint8_t *out, uint8_t *scratch, size_t scratch_capacity)
{
    jpeg_encoder_handle_t engine = NULL;
    const jpeg_encode_engine_cfg_t engine_cfg = {.intr_priority = 0, .timeout_ms = SCREENSHOT_JPEG_TIMEOUT_MS};
    if (jpeg_new_encoder_engine(&engine_cfg, &engine) != ESP_OK)
        return 0;
    screenshot_jpeg_join_t join;
    bool ok = screenshot_jpeg_join_init(&join, out, SCREENSHOT_JPEG_MAX_BYTES, frame->width, frame->height, SCREENSHOT_BAND_ROWS);
    for (uint32_t i = 0; i < frame->count && ok; i++)
    {
        uint32_t rows = frame->height - i * SCREENSHOT_BAND_ROWS;
        if (rows > SCREENSHOT_BAND_ROWS)
            rows = SCREENSHOT_BAND_ROWS;
        const jpeg_encode_cfg_t cfg = {
            .height = rows,
            .width = frame->width,
            .src_type = JPEG_ENCODE_IN_FORMAT_RGB888,
            .sub_sample = JPEG_DOWN_SAMPLING_YUV444,
            .image_quality = SCREENSHOT_JPEG_QUALITY,
            .pixel_reverse = false,
        };
        uint32_t size = 0;
        ok = jpeg_encoder_process(engine, &cfg, frame->bands[i], frame->width * rows * 3U, scratch, (uint32_t) scratch_capacity, &size) ==
                 ESP_OK &&
             size >= 4 && size <= scratch_capacity && screenshot_jpeg_join_append(&join, scratch, size);
    }
    jpeg_del_encoder_engine(engine);
    return ok && join.rows == frame->height ? (uint32_t) join.used : 0;
}
#endif

#if LV_USE_SNAPSHOT
typedef struct png_block
{
    struct png_block *next;
    size_t used;
    uint8_t bytes[SCREENSHOT_PNG_BLOCK_BYTES];
} png_block_t;

typedef struct
{
    png_block_t *head;
    png_block_t *tail;
    size_t size;
} png_output_t;

static void png_output_free(png_output_t *out)
{
    while (out->head)
    {
        png_block_t *next = out->head->next;
        heap_caps_free(out->head);
        out->head = next;
    }
    memset(out, 0, sizeof(*out));
}

/* Keep the encoded image bounded without requiring a contiguous 1 MiB hole.
 * Finish encoding before sending headers so allocation failures remain errors. */
static bool png_output_write(png_output_t *out, const void *data, size_t len)
{
    if (len > SCREENSHOT_PNG_MAX_BYTES - out->size)
        return false;
    const uint8_t *src = data;
    while (len)
    {
        if (!out->tail || out->tail->used == SCREENSHOT_PNG_BLOCK_BYTES)
        {
            png_block_t *block = heap_caps_malloc(sizeof(*block), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!block)
                return false;
            block->next = NULL;
            block->used = 0;
            if (out->tail)
                out->tail->next = block;
            else
                out->head = block;
            out->tail = block;
        }
        size_t n = SCREENSHOT_PNG_BLOCK_BYTES - out->tail->used;
        if (n > len)
            n = len;
        memcpy(out->tail->bytes + out->tail->used, src, n);
        out->tail->used += n;
        out->size += n;
        src += n;
        len -= n;
    }
    return true;
}

static void *png_zalloc(void *opaque, unsigned items, unsigned size)
{
    (void) opaque;
    if (size && items > SIZE_MAX / size)
        return NULL;
    return heap_caps_malloc((size_t) items * size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void png_zfree(void *opaque, void *ptr)
{
    (void) opaque;
    heap_caps_free(ptr);
}

static void png_put_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t) (value >> 24);
    dst[1] = (uint8_t) (value >> 16);
    dst[2] = (uint8_t) (value >> 8);
    dst[3] = (uint8_t) value;
}

static bool png_write_chunk(png_output_t *out, const char type[4], const uint8_t *data, size_t len)
{
    uint8_t header[8], checksum[4];
    png_put_u32(header, (uint32_t) len);
    memcpy(header + 4, type, 4);
    uLong crc = crc32(0, header + 4, 4);
    if (len)
        crc = crc32(crc, data, (uInt) len);
    png_put_u32(checksum, (uint32_t) crc);
    return png_output_write(out, header, sizeof(header)) && png_output_write(out, data, len) &&
           png_output_write(out, checksum, sizeof(checksum));
}

/* IDAT chunks share one zlib stream, independent of the output block boundaries. */
static bool png_deflate(z_stream *z, png_output_t *out, uint8_t *buffer, int flush)
{
    int result;
    do
    {
        const uInt before = z->avail_in;
        z->next_out = buffer;
        z->avail_out = SCREENSHOT_PNG_DEFLATE_BYTES;
        result = deflate(z, flush);
        const size_t len = SCREENSHOT_PNG_DEFLATE_BYTES - z->avail_out;
        if (result != Z_OK && result != Z_STREAM_END)
            return false;
        if (len && !png_write_chunk(out, "IDAT", buffer, len))
            return false;
        if (result == Z_STREAM_END)
            return flush == Z_FINISH && z->avail_in == 0;
        if (!len && before == z->avail_in)
            return false;
    } while (z->avail_in || flush == Z_FINISH);
    return true;
}

/* Lossless RGB888, filter 0. Only small PSRAM allocations are needed in addition
 * to the immutable capture bands; neither pixels nor resolution are reduced. */
static bool encode_png(const screenshot_frame_t *frame, png_output_t *out)
{
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const uint32_t w = frame->width;
    const uint32_t h = frame->height;
    uint8_t ihdr[13] = {0};
    png_put_u32(ihdr, w);
    png_put_u32(ihdr + 4, h);
    ihdr[8] = 8; /* bit depth */
    ihdr[9] = 2; /* RGB */
    if (!png_output_write(out, signature, sizeof(signature)) || !png_write_chunk(out, "IHDR", ihdr, sizeof(ihdr)))
        return false;
    uint8_t *row = heap_caps_malloc(1U + (size_t) w * 3U, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *buffer = heap_caps_malloc(SCREENSHOT_PNG_DEFLATE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!row || !buffer)
    {
        heap_caps_free(row);
        heap_caps_free(buffer);
        return false;
    }
    z_stream z;
    memset(&z, 0, sizeof(z));
    z.zalloc = png_zalloc;
    z.zfree = png_zfree;
    if (deflateInit2(&z, SCREENSHOT_PNG_LEVEL, Z_DEFLATED, 12, 5, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        heap_caps_free(row);
        heap_caps_free(buffer);
        return false;
    }
    /* LVGL's RGB888 is stored as B, G, R; PNG wants R, G, B. Each row is swapped into a small buffer behind its filter byte. */
    bool ok = true;
    for (uint32_t y = 0; y < h && ok; y++)
    {
        const uint8_t *src = screenshot_frame_row(frame, y);
        row[0] = 0;
        for (uint32_t x = 0; x < w; x++)
        {
            row[1 + x * 3U] = src[x * 3U + 2U];
            row[2 + x * 3U] = src[x * 3U + 1U];
            row[3 + x * 3U] = src[x * 3U];
        }
        z.next_in = row;
        z.avail_in = 1U + w * 3U;
        ok = png_deflate(&z, out, buffer, Z_NO_FLUSH);
    }
    heap_caps_free(row);
    ok = ok && png_deflate(&z, out, buffer, Z_FINISH);
    deflateEnd(&z);
    heap_caps_free(buffer);
    return ok && png_write_chunk(out, "IEND", NULL, 0);
}

static esp_err_t screenshot_png_handle(httpd_req_t *req)
{
    const int64_t t0 = esp_timer_get_time();
    screenshot_frame_t frame;
    bool busy;
    if (!api_screenshot_capture("PNG", &frame, &busy, NULL, 0))
    {
        return image_send_text_error(req, busy ? "503 Service Unavailable" : "500 Internal Server Error",
                                     busy ? "Display is busy" : "Failed to capture screenshot");
    }
    const int64_t t_snap = esp_timer_get_time();
    png_output_t out = {0};
    const bool ok = encode_png(&frame, &out);
    screenshot_frame_free(&frame);
    const int64_t t_enc = esp_timer_get_time();
    if (!ok)
    {
        png_output_free(&out);
        ESP_LOGW(TAG_HTTP, "Screenshot PNG encoding failed");
        return image_send_text_error(req, "500 Internal Server Error", "Failed to encode screenshot");
    }
    image_set_common_headers(req);
    httpd_resp_set_type(req, "image/png");
    esp_err_t err = ESP_OK;
    for (png_block_t *block = out.head; block && err == ESP_OK; block = block->next)
        err = httpd_resp_send_chunk(req, (const char *) block->bytes, block->used);
    if (err == ESP_OK)
        err = httpd_resp_send_chunk(req, NULL, 0);
    const size_t len = out.size;
    png_output_free(&out);
    ESP_LOGI(TAG_HTTP, "Screenshot PNG %u B: capture %d ms, encode %d ms, send %d ms", (unsigned) len, (int) ((t_snap - t0) / 1000),
             (int) ((t_enc - t_snap) / 1000), (int) ((esp_timer_get_time() - t_enc) / 1000));
    return err;
}
#endif

/** @brief Capture before encoding; never send image data while holding the display lock.
 *
 * PNG does not depend on the hardware JPEG encoder. An unsupported format
 * is reported rather than silently substituted.
 */
static esp_err_t screenshot_handle(httpd_req_t *req, bool png)
{
#if !LV_USE_SNAPSHOT
    return image_send_text_error(req, "501 Not Implemented", "LVGL snapshot support is disabled");
#else
    if (png)
        return screenshot_png_handle(req);
#  if !SOC_JPEG_ENCODE_SUPPORTED
    return image_send_text_error(req, "501 Not Implemented", "JPEG screenshots are unavailable; use PNG");
#  else
    const int64_t t0 = esp_timer_get_time();
    uint8_t *image = heap_caps_malloc(SCREENSHOT_MAX_ENCODED, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *scratch = NULL;
    size_t scratch_capacity = 0;
    if (image)
    {
        const jpeg_encode_memory_alloc_cfg_t mem = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};
        scratch = jpeg_alloc_encoder_mem(SCREENSHOT_MAX_ENCODED, &mem, &scratch_capacity);
    }
    /* Reserve the bounded output/scratch before fragment-sized RGB allocations. */
    if (!image || !scratch || scratch_capacity < SCREENSHOT_MAX_ENCODED)
    {
        heap_caps_free(scratch);
        heap_caps_free(image);
        return image_send_text_error(req, "500 Internal Server Error", "Failed to allocate screenshot output");
    }
    screenshot_frame_t frame;
    bool busy;
    if (!api_screenshot_capture("JPEG", &frame, &busy, scratch, scratch_capacity))
    {
        heap_caps_free(scratch);
        heap_caps_free(image);
        return image_send_text_error(req, busy ? "503 Service Unavailable" : "500 Internal Server Error",
                                     busy ? "Display is busy" : "Failed to capture screenshot");
    }
    const int64_t t_snap = esp_timer_get_time();
    const uint32_t len = encode_jpeg(&frame, image, scratch, scratch_capacity);
    const int64_t t_enc = esp_timer_get_time();
    screenshot_frame_free(&frame);
    heap_caps_free(scratch);
    if (!len)
    {
        heap_caps_free(image);
        ESP_LOGW(TAG_HTTP, "Screenshot JPEG encoding failed");
        return image_send_text_error(req, "500 Internal Server Error", "Failed to encode screenshot");
    }
    image_set_common_headers(req);
    httpd_resp_set_type(req, "image/jpeg");
    const esp_err_t err = httpd_resp_send(req, (const char *) image, (ssize_t) len);
    heap_caps_free(image);
    ESP_LOGI(TAG_HTTP, "Screenshot JPEG %u B: capture %d ms, encode %d ms, send %d ms", (unsigned) len, (int) ((t_snap - t0) / 1000),
             (int) ((t_enc - t_snap) / 1000), (int) ((esp_timer_get_time() - t_enc) / 1000));
    return err;
#  endif
#endif
}

/** @brief Serve a hardware-encoded JPEG, or HTTP 501 if the firmware has no JPEG snapshot path. */
esp_err_t api_screenshot_jpg_get_handler(httpd_req_t *req)
{
    return screenshot_handle(req, false);
}

/** @brief Serve a lossless PNG from the LVGL snapshot path, without requiring the JPEG encoder. */
esp_err_t api_screenshot_png_get_handler(httpd_req_t *req)
{
    return screenshot_handle(req, true);
}
