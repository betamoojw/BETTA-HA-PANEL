/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "api/api_routes.h"

static bool locked, lock_available = true;
static unsigned live_buffers, peak_buffers;
static unsigned take_calls, fail_take, bad_size, bad_stride, send_calls, fail_send, cases;
static unsigned bad_format, short_buffer;
static size_t live_bytes, peak_bytes, buffer_budget = SIZE_MAX;
static lv_draw_buf_t *snapshot_take(lv_obj_t *obj, lv_color_format_t format)
{
    assert(locked);
    if (++take_calls == fail_take) return NULL;
    lv_draw_buf_t *buf = lv_snapshot_take(obj, format);
    /* Model a snapshot allocation refusal; host LVGL itself is unrestricted. */
    if (buf && buf->data_size > buffer_budget - live_bytes) {
        lv_draw_buf_destroy(buf); return NULL;
    }
    if (buf) {
        if (take_calls == bad_size) buf->header.w--;
        if (take_calls == bad_stride) buf->header.stride = 1;
        if (take_calls == bad_format) buf->header.cf = LV_COLOR_FORMAT_RGB565;
        if (take_calls == short_buffer) buf->data_size = buf->header.stride * buf->header.h - 1;
        if (++live_buffers > peak_buffers) peak_buffers = live_buffers;
        live_bytes += buf->data_size;
        if (live_bytes > peak_bytes) peak_bytes = live_bytes;
    }
    return buf;
}
static void snapshot_destroy(lv_draw_buf_t *buf)
{
    assert(buf && live_buffers); --live_buffers; live_bytes -= buf->data_size;
    lv_draw_buf_destroy(buf);
}
#define lv_snapshot_take snapshot_take
#define lv_draw_buf_destroy snapshot_destroy
#include "../../main/api/api_screenshot.c"
#undef lv_snapshot_take
#undef lv_draw_buf_destroy

typedef struct {
    char status[64], type[32];
    uint8_t *bytes;
    size_t size, capacity;
    bool completed;
} response_t;

bool display_lock(uint32_t timeout_ms)
{
    assert(!locked && timeout_ms == 1500); locked = lock_available; return locked;
}
void display_unlock(void) { assert(locked); locked = false; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *field, const char *value)
{
    (void)req; assert(!locked && !strcmp(field, "Cache-Control") && !strcmp(value, "no-store")); return ESP_OK;
}
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status)
{
    assert(!locked); response_t *r = req->user_ctx;
    snprintf(r->status, sizeof(r->status), "%s", status); return ESP_OK;
}
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type)
{
    assert(!locked); response_t *r = req->user_ctx;
    snprintf(r->type, sizeof(r->type), "%s", type); return ESP_OK;
}
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *text)
{
    (void)req; (void)text; assert(!locked); return ESP_OK;
}
esp_err_t httpd_resp_send_chunk(httpd_req_t *req, const char *data, size_t size)
{
    assert(!locked); response_t *r = req->user_ctx;
    if (++send_calls == fail_send) return ESP_FAIL;
    if (!size) { r->completed = true; return ESP_OK; }
    assert(data && r->size + size <= r->capacity);
    memcpy(r->bytes + r->size, data, size); r->size += size; return ESP_OK;
}

static void reset_faults(void)
{
    assert(!locked && !live_buffers && !live_bytes);
    take_calls = fail_take = bad_size = bad_stride = send_calls = fail_send = 0;
    bad_format = short_buffer = 0;
    peak_buffers = 0; peak_bytes = 0; buffer_budget = SIZE_MAX; lock_available = true;
}
static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static response_t small_capture(const char *error, esp_err_t result)
{
    static uint8_t bytes[512];
    response_t r = {.bytes = bytes, .capacity = sizeof(bytes)};
    httpd_req_t req = {.user_ctx = &r};
    assert(api_screenshot_bmp_get_handler(&req) == result);
    assert(!locked && !live_buffers && !live_bytes);
    if (error) assert(!strcmp(r.status, error) && !r.size && !r.completed);
    else if (result == ESP_OK) {
        assert(!r.status[0] && !strcmp(r.type, "image/bmp") && r.completed && r.size == 250);
        assert(bytes[0] == 'B' && bytes[1] == 'M' && read_u32(bytes+2) == r.size);
        assert(read_u32(bytes+10) == 54 && read_u32(bytes+14) == 40);
        assert(read_u32(bytes+18) == 9 && read_u32(bytes+22) == 7);
        assert(bytes[26] == 1 && bytes[28] == 24 && read_u32(bytes+30) == 0);
        assert(read_u32(bytes+34) == 196);
        for (unsigned y = 0; y < 7; y++) assert(bytes[54+y*28+27] == 0);
    } else assert(!r.completed);
    cases++; return r;
}
static void color(const response_t *r, unsigned x, unsigned y, unsigned rgb, unsigned tolerance)
{
    const uint8_t *p = r->bytes + 54 + (6-y)*28 + x*3;
    for (unsigned c = 0; c < 3; c++) assert(abs((int)p[c] - (int)((rgb>>(8*c))&255)) <= (int)tolerance);
}
static void composition_and_failures(lv_obj_t *overlay, unsigned width, unsigned height)
{
    lv_display_t *display = lv_display_get_default();
    lv_obj_set_hidden(overlay, true);
    lv_display_set_resolution(display, 9, 7);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x123456), 0);
    reset_faults(); buffer_budget = 9*7*4;
    response_t r = small_capture(NULL, ESP_OK);
    color(&r, 0, 0, 0x123456, 0); color(&r, 8, 6, 0x123456, 0);
    assert(take_calls == 2 && peak_buffers == 1);
    lv_obj_t *popup = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(popup);
    lv_obj_set_pos(popup, 2, 1); lv_obj_set_size(popup, 5, 3);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0xc06020), 0);
    lv_obj_set_style_bg_opa(popup, 128, 0);
    reset_faults(); r = small_capture(NULL, ESP_OK);
    color(&r, 0, 0, 0x123456, 0); color(&r, 8, 6, 0x123456, 0);
    color(&r, 3, 2, 0x694a3a, 1); /* Independent half-opacity expectation. */
    assert(peak_buffers == 2);
    for (unsigned call = 1; call <= 2; call++) {
        reset_faults(); fail_take = call; small_capture("500 Internal Server Error", ESP_OK);
        reset_faults(); bad_size = call; small_capture("500 Internal Server Error", ESP_OK);
        reset_faults(); bad_stride = call; small_capture("500 Internal Server Error", ESP_OK);
        reset_faults(); bad_format = call; small_capture("500 Internal Server Error", ESP_OK);
        reset_faults(); short_buffer = call; small_capture("500 Internal Server Error", ESP_OK);
    }
    reset_faults(); buffer_budget = 9*7*4; small_capture("500 Internal Server Error", ESP_OK);
    lv_obj_set_hidden(popup, true);
    reset_faults(); r = small_capture(NULL, ESP_OK);
    color(&r, 3, 2, 0x123456, 0); assert(peak_buffers == 1);
    lv_obj_set_hidden(popup, false);

    /* Offset, rounded corners and layered opacity must retain the base. */
    lv_obj_set_pos(popup, 1, 0); lv_obj_set_size(popup, 9, 7);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    reset_faults(); r = small_capture(NULL, ESP_OK);
    color(&r, 0, 0, 0x123456, 0); color(&r, 4, 3, 0xff0000, 0); assert(peak_buffers == 2);
    lv_obj_set_pos(popup, 0, 0); lv_obj_set_style_radius(popup, 3, 0);
    reset_faults(); r = small_capture(NULL, ESP_OK);
    color(&r, 4, 3, 0xff0000, 0); assert(peak_buffers == 2);
    lv_obj_set_style_radius(popup, 0, 0); lv_obj_set_style_opa_layered(popup, 128, 0);
    reset_faults(); r = small_capture(NULL, ESP_OK);
    color(&r, 4, 3, 0x88192a, 1); assert(peak_buffers == 2);
    lv_obj_set_style_opa_layered(popup, LV_OPA_COVER, 0);
    reset_faults(); buffer_budget = 256; /* One small buffer, including alignment. */
    r = small_capture(NULL, ESP_OK); color(&r, 0, 0, 0xff0000, 0); assert(peak_buffers == 1);
    /* An opaque cover below a newer popup still needs all top-layer children. */
    lv_obj_t *front = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(front);
    lv_obj_set_pos(front, 2, 1); lv_obj_set_size(front, 5, 3);
    lv_obj_set_style_bg_color(front, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_bg_opa(front, LV_OPA_COVER, 0);
    reset_faults(); buffer_budget = 256;
    r = small_capture(NULL, ESP_OK);
    color(&r, 0, 0, 0xff0000, 0); color(&r, 3, 2, 0x00ff00, 0); assert(peak_buffers == 1);
    lv_obj_delete(front);
    reset_faults(); fail_take = 1; small_capture("500 Internal Server Error", ESP_OK);
    reset_faults(); bad_size = 1; small_capture("500 Internal Server Error", ESP_OK);
    reset_faults(); bad_format = 1; small_capture("500 Internal Server Error", ESP_OK);
    reset_faults(); short_buffer = 1; small_capture("500 Internal Server Error", ESP_OK);
    lv_obj_delete(popup);
    reset_faults(); lock_available = false;
    small_capture("503 Service Unavailable", ESP_OK); assert(!take_calls);
    const unsigned send_failures[] = {1, 2, 3, 16}; /* header, row, padding, terminator */
    for (unsigned i = 0; i < sizeof(send_failures)/sizeof(send_failures[0]); i++) {
        reset_faults(); fail_send = send_failures[i]; small_capture(NULL, ESP_FAIL);
    }
    reset_faults(); small_capture(NULL, ESP_OK);
    lv_display_set_resolution(display, width, height); lv_obj_set_hidden(overlay, false);
}

void sim_screenshot_regression(lv_obj_t *overlay, const char *capture_path)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0044cc), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_draw_buf_t *expected = lv_snapshot_take(overlay, LV_COLOR_FORMAT_RGB888);
    assert(expected);
    unsigned width = expected->header.w, height = expected->header.h;
    reset_faults();
    buffer_budget = expected->data_size;
    unsigned stride = (width * 3 + 3) & ~3U;
    response_t response = {.capacity = 54 + (size_t)stride * height};
    response.bytes = malloc(response.capacity); assert(response.bytes);
    httpd_req_t req = {.user_ctx = &response};
    assert(api_screenshot_bmp_get_handler(&req) == ESP_OK);
    assert(!locked && !live_buffers && response.completed);
    assert(!response.status[0] && !strcmp(response.type, "image/bmp"));
    assert(take_calls == 1 && peak_buffers == 1 && peak_bytes == expected->data_size);
    assert(response.size == response.capacity && response.bytes[0] == 'B' && response.bytes[1] == 'M');
    for (unsigned y = 0; y < height; y++) {
        const uint8_t *actual = response.bytes + 54 + (height-1-y)*stride;
        const uint8_t *reference = expected->data + y*expected->header.stride;
        for (unsigned x = 0; x < width; x++) {
            if (memcmp(actual + 3*x, reference + 3*x, 3)) {
                fprintf(stderr, "BMP omitted/changed flip overlay pixel (%u,%u): %u,%u,%u vs %u,%u,%u\n",
                        x, y, actual[3*x], actual[3*x+1], actual[3*x+2], reference[3*x], reference[3*x+1], reference[3*x+2]);
                abort();
            }
        }
    }
    if (capture_path) {
        FILE *file = fopen(capture_path, "wb"); assert(file);
        assert(fwrite(response.bytes, 1, response.size, file) == response.size); assert(!fclose(file));
    }
    lv_draw_buf_destroy(expected); free(response.bytes); cases++;
    printf("BMP screenshot: opaque flip overlay matches (%u snapshot buffers, %zu bytes at peak)\n", peak_buffers, peak_bytes);
    composition_and_failures(overlay, width, height);
    printf("BMP screenshot: %u cases passed (composition, padding, lock/capture/send failures and cleanup)\n", cases);
}
