/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/* Production capture/HTTP/encoders, with deterministic regions and fault injection.
 * Real LVGL pixels and real JPEG decoding are separate regression gates. */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "driver/jpeg_encode.h"
static int fail_deflate_at, deflate_calls;
static int test_deflate(z_streamp stream, int flush);
#define deflate test_deflate
#include "api/api_screenshot.c"
#include "api/screenshot_jpeg_join.c"
#include "api/api_screenshot_image.c"
#undef deflate
static int failures, cases;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
static struct lv_obj { int x; } screen_obj, top_obj;
static lv_display_t display;
static int32_t display_w, display_h;
static bool locked, opaque_cover, popup, display_missing, change_geometry, noise;
static int lock_calls, unlock_calls, fail_lock_at, render_calls, fail_render_at;
static int heap_calls, fail_alloc_at, allocations, engines, fail_engine, jpeg_calls, fail_jpeg_at, bad_jpeg;
static int send_calls, fail_send_at, diag_logs, memory_samples;
static size_t heap_bytes, peak_bytes, largest_alloc, largest_limit, total_limit, sampled_bytes;
static char status[64], type[32], capture_log[512];
static uint8_t body[4 << 20];
static size_t body_len;
static int64_t fake_us;
static int test_deflate(z_streamp stream, int flush)
{
    CHECK(!locked);
    if (++deflate_calls == fail_deflate_at) return Z_STREAM_ERROR;
    return deflate(stream, flush);
}
lv_display_t *lv_display_get_default(void) { return display_missing ? NULL : &display; }
int32_t lv_display_get_horizontal_resolution(lv_display_t *d) { (void)d; return display_w + (change_geometry && lock_calls == 2); }
int32_t lv_display_get_vertical_resolution(lv_display_t *d) { (void)d; return display_h; }
void lv_obj_update_layout(lv_obj_t *obj) { (void)obj; CHECK(locked); }
lv_obj_t *lv_refr_get_top_obj(const lv_area_t *area, lv_obj_t *layer)
{
    CHECK(locked && area->x1 == 0 && area->y1 == 0); return opaque_cover ? layer : NULL;
}
lv_obj_t *lv_screen_active(void) { return &screen_obj; }
lv_obj_t *lv_layer_top(void) { return &top_obj; }
int64_t esp_timer_get_time(void) { return fake_us; }
bool display_lock(uint32_t timeout_ms)
{
    CHECK(timeout_ms == 1500 && !locked); fake_us += 2500;
    locked = ++lock_calls != fail_lock_at; return locked;
}
void display_unlock(void) { CHECK(locked); unlock_calls++; locked = false; fake_us += 900; }
void test_screenshot_log(const char *fmt, ...)
{
    CHECK(!locked);
    if (strstr(fmt, "capture=%s")) {
        va_list args; va_start(args, fmt);
        int n = vsnprintf(capture_log, sizeof(capture_log), fmt, args); va_end(args);
        CHECK(n > 0 && n + 48 < 256); diag_logs++;
    }
    fake_us += 5000;
}
size_t heap_caps_get_free_size(unsigned caps)
{
    CHECK(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    sampled_bytes = heap_bytes; memory_samples++; fake_us += 50;
    return total_limit - heap_bytes;
}
size_t heap_caps_get_largest_free_block(unsigned caps)
{
    CHECK(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); fake_us += 50; return largest_limit;
}
typedef union { max_align_t alignment; size_t size; } heap_header_t;
void *heap_caps_malloc(size_t size, unsigned caps)
{
    (void)caps; CHECK(!locked);
    if (++heap_calls == fail_alloc_at || size > largest_limit || size > total_limit - heap_bytes) return NULL;
    heap_header_t *h = malloc(sizeof(*h) + size); if (!h) abort(); h->size = size;
    allocations++; heap_bytes += size;
    if (heap_bytes > peak_bytes) peak_bytes = heap_bytes;
    if (size > largest_alloc) largest_alloc = size;
    return h + 1;
}
void *heap_caps_aligned_alloc(size_t alignment, size_t size, unsigned caps)
{
    CHECK(alignment == 16);
    void *p = heap_caps_malloc(size, caps);
    CHECK(!p || (uintptr_t)p % alignment == 0);
    return p;
}
void heap_caps_free(void *p)
{
    if (!p) return;
    heap_header_t *h = (heap_header_t *)p - 1;
    CHECK(!locked && allocations > 0); allocations--; heap_bytes -= h->size; free(h);
}
static uint8_t base_pixel(unsigned x, unsigned y, unsigned c)
{
    if (!noise) return (uint8_t)((x / 16 * 13 + y / 13 * 7 + c * 63) & 255);
    uint32_t v = (y * (unsigned)display_w + x) * 3 + c + 1;
    v ^= v >> 16; v *= 0x7feb352dU; v ^= v >> 15; v *= 0x846ca68bU; return (uint8_t)(v ^ (v >> 16));
}
static unsigned alpha(unsigned x, unsigned y) { return popup ? (x + y) % 3 * 127 + ((x + y) % 3 == 2) : 0; }
static uint8_t expected(unsigned x, unsigned y, unsigned c)
{
    unsigned a = alpha(x, y); return opaque_cover ? 200 + c : (uint8_t)(((200 + c) * a + base_pixel(x,y,c) * (255-a))/255);
}
bool screenshot_render_region(lv_obj_t *obj, lv_color_format_t cf, const lv_area_t *area, uint8_t *pixels, size_t capacity)
{
    CHECK(locked); fake_us += 300;
    if (++render_calls == fail_render_at) return false;
    unsigned bpp = cf == LV_COLOR_FORMAT_RGB888 ? 3 : 4;
    CHECK(area->x1 == 0 && area->x2 == display_w - 1 && area->y2 - area->y1 < 32);
    CHECK(capacity >= (size_t)display_w * (area->y2-area->y1+1) * bpp);
    for (int y = area->y1; y <= area->y2; y++) for (int x = 0; x < display_w; x++) {
        size_t at = ((size_t)(y-area->y1)*display_w+x)*bpp;
        for (unsigned c = 0; c < 3; c++) pixels[at+c] = obj == &top_obj ? 200+c : base_pixel(x,y,c);
        if (bpp == 4) pixels[at+3] = (uint8_t)alpha(x,y);
    }
    return true;
}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *f, const char *v) { (void)r; CHECK(!locked && !strcmp(f,"Cache-Control") && !strcmp(v,"no-store")); return ESP_OK; }
esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *s) { (void)r; snprintf(status,sizeof(status),"%s",s); return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *s) { (void)r; snprintf(type,sizeof(type),"%s",s); return ESP_OK; }
esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *s) { (void)r; (void)s; CHECK(!locked && !heap_bytes); return ++send_calls == fail_send_at ? ESP_FAIL : ESP_OK; }
esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *p, ssize_t n)
{
    (void)r; CHECK(!locked && !engines);
    if (++send_calls == fail_send_at) return ESP_FAIL;
    CHECK(n >= 0 && body_len + (size_t)n <= sizeof(body));
    if (p) memcpy(body + body_len, p, (size_t)n);
    body_len += (size_t)n; return ESP_OK;
}
esp_err_t httpd_resp_send(httpd_req_t *r, const char *p, ssize_t n)
{
    CHECK(allocations == 1); return httpd_resp_send_chunk(r,p,n);
}
esp_err_t jpeg_new_encoder_engine(const jpeg_encode_engine_cfg_t *cfg, jpeg_encoder_handle_t *ret)
{
    CHECK(!locked && cfg->timeout_ms == 4000);
    if (fail_engine) return ESP_FAIL;
    engines++; *ret = (jpeg_encoder_handle_t)&engines; return ESP_OK;
}
esp_err_t jpeg_del_encoder_engine(jpeg_encoder_handle_t e) { (void)e; engines--; return ESP_OK; }
void *jpeg_alloc_encoder_mem(size_t size, const jpeg_encode_memory_alloc_cfg_t *cfg, size_t *allocated)
{
    CHECK(cfg->buffer_direction == JPEG_ENC_ALLOC_OUTPUT_BUFFER); *allocated = size;
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
esp_err_t jpeg_encoder_process(jpeg_encoder_handle_t e, const jpeg_encode_cfg_t *cfg, const uint8_t *in, uint32_t in_size,
                               uint8_t *out, uint32_t out_size, uint32_t *written)
{
    (void)e; CHECK(!locked && engines == 1 && (uintptr_t)in % 16 == 0); jpeg_calls++;
    CHECK(cfg->src_type == JPEG_ENCODE_IN_FORMAT_RGB888 && cfg->sub_sample == JPEG_DOWN_SAMPLING_YUV444);
    CHECK(cfg->image_quality == 90 && !cfg->pixel_reverse && in_size == cfg->width*cfg->height*3U);
    for (unsigned y=0; y<cfg->height; y++) for (unsigned x=0; x<cfg->width; x++) for (unsigned c=0;c<3;c++)
        CHECK(in[(y*cfg->width+x)*3+c] == expected(x,(jpeg_calls-1)*32+y,c));
    if (jpeg_calls == fail_jpeg_at) return ESP_FAIL;
    if (bad_jpeg == 1) { *written = out_size + 1; return ESP_OK; }
    /* Structural fixture only: real entropy correctness is checked by Pillow. */
    uint8_t h[] = {255,216,255,219,0,3,1,255,196,0,3,1,255,192,0,17,8,0,0,0,0,3,1,17,0,2,17,1,3,17,1,
                   255,218,0,12,3,1,0,2,17,3,17,0,63,0,42,255,217};
    h[17]=(uint8_t)(cfg->height>>8); h[18]=(uint8_t)cfg->height;
    h[19]=(uint8_t)(cfg->width>>8); h[20]=(uint8_t)cfg->width;
    memcpy(out,h,sizeof(h)); *written=sizeof(h);
    if (bad_jpeg == 2) out[1]=0;
    if (bad_jpeg == 3 && jpeg_calls==2) out[6]++;
    return ESP_OK;
}
static void reset(void)
{
    CHECK(!locked && !heap_bytes && !allocations && !engines); cases++;
    display_w=3; display_h=2;
    opaque_cover=popup=display_missing=change_geometry=noise=false;
    lock_calls=unlock_calls=fail_lock_at=render_calls=fail_render_at=0;
    heap_calls=fail_alloc_at=fail_engine=jpeg_calls=fail_jpeg_at=bad_jpeg=0;
    send_calls=fail_send_at=diag_logs=memory_samples=0;
    fail_deflate_at=deflate_calls=0;
    peak_bytes=largest_alloc=sampled_bytes=body_len=0; largest_limit=2162688; total_limit=6850000;
    status[0]=type[0]=capture_log[0]=0; fake_us=0;
}
static void clean(void) { CHECK(!locked && !allocations && !engines && !heap_bytes); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* Validates the PNG in body (signature, IHDR, IDAT and IEND with their CRCs) and returns the unfiltered RGB rows (w*3 each). */
static bool decode_png(uint32_t w, uint32_t h, uint8_t *rgb)
{
    bool ok = body_len > 57 && memcmp(body, "\x89PNG\r\n\x1a\n", 8) == 0 && be32(body + 8) == 13 && memcmp(body + 12, "IHDR", 4) == 0 &&
              be32(body + 16) == w && be32(body + 20) == h && body[24] == 8 && body[25] == 2 &&
              crc32(0, body + 12, 17) == be32(body + 29);
    if (!ok) return false;
    const uint32_t idat = be32(body + 33);
    ok = memcmp(body + 37, "IDAT", 4) == 0 && crc32(0, body + 37, idat + 4) == be32(body + 41 + idat) &&
         body_len == 41 + idat + 4 + 12 && memcmp(body + 45 + idat + 4, "IEND", 4) == 0;
    if (!ok) return false;
    const size_t row = 1U + (size_t)w * 3U;
    uint8_t *raw = malloc(row * h);
    uLongf raw_len = (uLongf)(row * h);
    ok = uncompress(raw, &raw_len, body + 41, idat) == Z_OK && raw_len == row * h;
    for (uint32_t y = 0; ok && y < h; y++) {
        ok = raw[y * row] == 0;
        memcpy(rgb + (size_t)y * w * 3U, raw + y * row + 1U, (size_t)w * 3U);
    }
    free(raw);
    return ok;
}


#include "screenshot_large_cases.h"
#include "screenshot_diagnostic_cases.h"
int main(void)
{
    large_cases(); diagnostic_cases(); clean();
    printf("Screenshot lifecycle: %d cases, %d failures (JPEG=%d)\n",cases,failures,SOC_JPEG_ENCODE_SUPPORTED);
    return failures != 0;
}
