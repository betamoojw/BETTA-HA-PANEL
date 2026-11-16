/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "api/api_routes.h"

#include "esp_heap_caps.h"
#include "api/screenshot_region.h"
static bool locked, lock_available = true, stall_dispatch;
static unsigned live_buffers, peak_buffers, take_calls, fail_take, send_calls, fail_send, cases;
static size_t live_bytes, peak_bytes, buffer_budget = SIZE_MAX, allocation_limit = 2162688;
static void test_dispatch(void);
#define screenshot_render_region production_region
#define lv_draw_dispatch test_dispatch
#include "../../main/api/screenshot_region.c"
#undef lv_draw_dispatch
#undef screenshot_render_region
static void test_dispatch(void) { if (!stall_dispatch) lv_draw_dispatch(); }
bool screenshot_render_region(lv_obj_t *obj, lv_color_format_t cf, const lv_area_t *area, uint8_t *pixels, size_t capacity)
{
    assert(locked);
    if (++take_calls == fail_take) return false;
    lv_display_t *d = lv_obj_get_display(obj);
    lv_layer_t *head = d->layer_head;
    lv_display_t *refreshing = lv_refr_get_disp_refreshing();
    bool result = production_region(obj, cf, area, pixels, capacity);
    assert(d->layer_head == head && lv_refr_get_disp_refreshing() == refreshing);
    return result;
}
typedef union { max_align_t alignment; size_t size; } allocation_t;
static void *test_malloc(size_t size, unsigned caps)
{
    (void)caps; assert(!locked);
    if (size > allocation_limit || size > buffer_budget - live_bytes) return NULL;
    allocation_t *p=malloc(sizeof(*p)+size); assert(p); p->size=size;
    live_buffers++;live_bytes+=size;
    if(live_buffers>peak_buffers)peak_buffers=live_buffers;
    if(live_bytes>peak_bytes)peak_bytes=live_bytes;
    return p+1;
}
static void test_free(void *data)
{
    if (!data) return;
    allocation_t *p=(allocation_t *)data-1;
    assert(!locked && live_buffers);live_buffers--;live_bytes-=p->size;free(p);
}
static void *test_aligned_malloc(size_t alignment, size_t size, unsigned caps)
{
    assert(alignment == 16); void *p = test_malloc(size, caps);
    assert(!p || (uintptr_t)p % alignment == 0); return p;
}
#define heap_caps_aligned_alloc test_aligned_malloc
#define heap_caps_malloc test_malloc
#define heap_caps_free test_free
#include "../../main/api/api_screenshot.c"
#undef heap_caps_aligned_alloc
#undef heap_caps_malloc
#undef heap_caps_free

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
    take_calls = fail_take = send_calls = fail_send = 0; stall_dispatch = false;
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
    reset_faults();
    response_t r = small_capture(NULL, ESP_OK);
    color(&r, 0, 0, 0x123456, 0); color(&r, 8, 6, 0x123456, 0);
    assert(take_calls == 2 && peak_buffers == 2);
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
    }
    reset_faults(); buffer_budget = 9*7*4; small_capture("500 Internal Server Error", ESP_OK);
    lv_obj_set_hidden(popup, true);
    reset_faults(); r = small_capture(NULL, ESP_OK);
    color(&r, 3, 2, 0x123456, 0); assert(peak_buffers == 2);
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
    reset_faults(); stall_dispatch = true; small_capture("500 Internal Server Error", ESP_OK);
    reset_faults(); r = small_capture(NULL, ESP_OK); color(&r, 4, 3, 0x88192a, 1);
    lv_obj_set_style_opa_layered(popup, LV_OPA_COVER, 0);
    reset_faults(); buffer_budget = 512; /* RGB plus ARGB scratch, including alignment. */
    r = small_capture(NULL, ESP_OK); color(&r, 0, 0, 0xff0000, 0); assert(peak_buffers == 2);
    /* An opaque cover below a newer popup still needs all top-layer children. */
    lv_obj_t *front = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(front);
    lv_obj_set_pos(front, 2, 1); lv_obj_set_size(front, 5, 3);
    lv_obj_set_style_bg_color(front, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_bg_opa(front, LV_OPA_COVER, 0);
    reset_faults(); buffer_budget = 512;
    r = small_capture(NULL, ESP_OK);
    color(&r, 0, 0, 0xff0000, 0); color(&r, 3, 2, 0x00ff00, 0); assert(peak_buffers == 2);
    lv_obj_delete(front);
    reset_faults(); fail_take = 1; small_capture("500 Internal Server Error", ESP_OK);
    lv_obj_delete(popup);
    reset_faults(); stall_dispatch = true; small_capture("500 Internal Server Error", ESP_OK);
    reset_faults(); lock_available = false;
    small_capture("503 Service Unavailable", ESP_OK); assert(!take_calls);
    const unsigned send_failures[] = {1, 2, 3, 16}; /* header, row, padding, terminator */
    for (unsigned i = 0; i < sizeof(send_failures)/sizeof(send_failures[0]); i++) {
        reset_faults(); fail_send = send_failures[i]; small_capture(NULL, ESP_FAIL);
    }
    reset_faults(); small_capture(NULL, ESP_OK);
    lv_display_set_resolution(display, width, height); lv_obj_set_hidden(overlay, false);
}

#include "ui/ui_license_warning.h"
const char *ui_i18n_get_language(void) { return "en"; }
static ui_license_warning_status_t warning_sample;
static ui_license_warning_status_t warning_status(void) { return warning_sample; }
static void full_composition(lv_obj_t *overlay, unsigned width, unsigned height)
{
    lv_obj_set_hidden(overlay, true);
    ui_license_warning_set_source(warning_status);
    for (unsigned mode=0;mode<4;mode++) {
        warning_sample=(ui_license_warning_status_t){.state=LICENSE_UNCONFIGURED,
            .uptime_us=INT64_C(300000000)-(mode<2 ? 1 : 0)};
        lv_obj_t *bar=lv_obj_create(lv_screen_active()); lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar,width,60); lv_obj_set_style_bg_opa(bar,LV_OPA_COVER,0);
        lv_obj_set_style_bg_color(bar,lv_color_hex(0x415169),0);
        lv_obj_update_layout(bar); assert(ui_license_warning_attach(bar,NULL,60));
        lv_obj_t *popup=NULL;
        if(mode%2) {
            popup=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(popup);
            lv_obj_set_pos(popup,21,49); lv_obj_set_size(popup,width-42,height-90);
            lv_obj_set_style_bg_color(popup,lv_color_hex(0xc06020),0);
            lv_obj_set_style_bg_opa(popup,200,0); lv_obj_set_style_radius(popup,23,0);
            lv_obj_set_style_border_width(popup,3,0); lv_obj_set_style_border_color(popup,lv_color_hex(0x00ff20),0);
            lv_obj_set_style_shadow_width(popup,17,0); lv_obj_set_style_shadow_opa(popup,128,0);
            lv_obj_set_style_opa_layered(popup,180,0);
        }
        lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_top());
        lv_draw_buf_t *base=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);
        lv_draw_buf_t *top=lv_snapshot_take(lv_layer_top(),LV_COLOR_FORMAT_ARGB8888);
        assert(base && top);
        reset_faults(); buffer_budget=(size_t)width*height*3+(size_t)width*32*4;
        unsigned stride=(width*3+3)&~3U;
        response_t r={.capacity=54+(size_t)stride*height};r.bytes=malloc(r.capacity);assert(r.bytes);
        httpd_req_t req={.user_ctx=&r};assert(api_screenshot_bmp_get_handler(&req)==ESP_OK);
        assert(!r.status[0] && r.completed && !locked && !live_bytes);
        for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++) for(unsigned c=0;c<3;c++) {
            const uint8_t *t=top->data+y*top->header.stride+x*4;
            unsigned b=base->data[y*base->header.stride+x*3+c];
            unsigned expected=(t[c]*t[3]+b*(255-t[3]))/255;
            unsigned actual=r.bytes[54+(height-1-y)*stride+x*3+c];
            if(actual!=expected) {
                fprintf(stderr,"Composite mode=%u (%u,%u,%u) actual=%u expected=%u\n",mode,x,y,c,actual,expected);
                abort();
            }
        }
        cases++;free(r.bytes);lv_draw_buf_destroy(base);lv_draw_buf_destroy(top);
        if(popup)lv_obj_delete(popup);
        lv_obj_delete(bar);
    }
    ui_license_warning_set_source(NULL);lv_obj_set_hidden(overlay,false);
    puts("Full-resolution pixels match legacy composition before/at 5 minutes, with/without popup");
}

void sim_screenshot_regression(lv_obj_t *overlay, const char *capture_path)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0044cc), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_draw_buf_t *expected = lv_snapshot_take(overlay, LV_COLOR_FORMAT_RGB888);
    assert(expected);
    unsigned width = expected->header.w, height = expected->header.h;
    reset_faults();
    buffer_budget = expected->data_size + (size_t)width * 32 * 4;
    unsigned stride = (width * 3 + 3) & ~3U;
    response_t response = {.capacity = 54 + (size_t)stride * height};
    response.bytes = malloc(response.capacity); assert(response.bytes);
    httpd_req_t req = {.user_ctx = &response};
    assert(api_screenshot_bmp_get_handler(&req) == ESP_OK);
    assert(!locked && !live_buffers && response.completed);
    assert(!response.status[0] && !strcmp(response.type, "image/bmp"));
    assert(take_calls == (height+31)/32 && peak_buffers == (height+31)/32+1);
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
    printf("BMP screenshot: opaque flip overlay matches (%u band/scratch buffers, %zu bytes at peak)\n", peak_buffers, peak_bytes);
    full_composition(overlay, width, height);
    composition_and_failures(overlay, width, height);
    printf("BMP screenshot: %u cases passed (composition, padding, lock/capture/send failures and cleanup)\n", cases);
}
