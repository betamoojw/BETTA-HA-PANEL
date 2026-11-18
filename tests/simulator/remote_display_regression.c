/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Exercise the production capture/input module against real LVGL, without a board.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_clock.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "ui/ui_remote_display.h"
#include "drivers/display_init.h"

/* Native Windows assert() can open a modal dialog and block unattended tests. */
#undef assert
#define assert(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

static unsigned clicks, wakes, flushes, lock_attempts;
static size_t flushed_pixels;
static bool local_down, lock_busy;
static size_t free_psram = 32U * 1024 * 1024, largest_block = 32U * 1024 * 1024;
static unsigned allocation_calls, fail_on_allocation, live_allocations;
typedef union { size_t bytes; max_align_t alignment; } allocation_header_t;

size_t heap_caps_get_free_size(unsigned caps)
{
    assert(caps & MALLOC_CAP_SPIRAM);
    return free_psram;
}
size_t heap_caps_get_largest_free_block(unsigned caps)
{
    assert(caps & MALLOC_CAP_SPIRAM);
    return largest_block < free_psram ? largest_block : free_psram;
}
void *heap_caps_malloc(size_t bytes, unsigned caps)
{
    assert(caps & MALLOC_CAP_SPIRAM);
    if (++allocation_calls == fail_on_allocation || bytes > heap_caps_get_largest_free_block(caps))
        return NULL;
    allocation_header_t *p = malloc(sizeof(*p) + bytes);
    assert(p);
    p->bytes = bytes;
    free_psram -= bytes;
    ++live_allocations;
    return p + 1;
}
void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    assert(!count || size <= SIZE_MAX / count);
    void *p = heap_caps_malloc(count * size, caps);
    if (p) memset(p, 0, count * size);
    return p;
}
void heap_caps_free(void *ptr)
{
    if (!ptr) return;
    allocation_header_t *p = (allocation_header_t *)ptr - 1;
    free_psram += p->bytes;
    assert(live_allocations);
    --live_allocations;
    free(p);
}
static void local_read(lv_indev_t *i, lv_indev_data_t *data)
{ (void)i; data->point=(lv_point_t){5,5}; data->state=local_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
bool display_lock(uint32_t timeout) { (void)timeout; ++lock_attempts; return !lock_busy; }
void display_unlock(void) {}
void display_note_activity(void) { wakes++; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)p; flushes++; flushed_pixels += lv_area_get_size(a); lv_display_flush_ready(d); }
static void clicked(lv_event_t *e) { (void)e; clicks++; }
static void tick(void) { sim_virtual_delay(20); lv_tick_inc(20); lv_timer_handler(); }
static uint16_t pixel(const ui_remote_frame_t *image, unsigned x, unsigned y)
{
    uint16_t value;
    memcpy(&value, ui_remote_frame_row(image, y) + 2 * x, sizeof(value));
    return value;
}
static void frame(uint16_t w, uint16_t h, uint16_t expected)
{
    const ui_remote_frame_t *pixels = NULL; uint16_t width=0,height=0; unsigned retry=0;
    lock_busy = true;
    assert(!ui_remote_display_capture(&pixels,&width,&height,&retry) && retry==200 && !pixels);
    lock_busy = false;
    assert(!ui_remote_display_capture(&pixels,&width,&height,&retry));
    lv_refr_now(NULL);
    lock_busy = true;
    assert(!ui_remote_display_capture(&pixels,&width,&height,&retry) && retry==200 && !pixels);
    lock_busy = false;
    assert(ui_remote_display_capture(&pixels,&width,&height,&retry));
    assert(width==w && height==h);
    for (unsigned y=0; y<h; ++y) {
        assert(pixel(pixels, 0, y) == expected);
        assert(pixel(pixels, w-1, y) == expected);
    }
    for (unsigned tile = 0; tile < ((w+31U)/32U)*((h+31U)/32U); ++tile)
        assert(ui_remote_frame_tile_changed(pixels, tile));
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x0000ff),0);
    lv_refr_now(NULL);
    assert(pixel(pixels,0,0)==expected && pixel(pixels,w-1,h-1)==expected); /* Frozen until ACK. */
    ui_remote_display_ack();
}

static void overlay_delta(lv_display_t *display, unsigned width, unsigned height)
{
    /* The four by four square crosses both tile boundaries. This also exercises
     * an overlay on the top layer, as used by the clock screensaver. */
    lv_obj_t *overlay = lv_obj_create(lv_display_get_layer_top(display));
    lv_obj_remove_style_all(overlay);
    lv_obj_set_pos(overlay, 31, 31);
    lv_obj_set_size(overlay, 4, 4);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x00ffff), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_refr_now(display);
    sim_virtual_delay(210);
    const ui_remote_frame_t *image;
    uint16_t w, h;
    unsigned retry;
    assert(!ui_remote_display_capture(&image, &w, &h, &retry));
    lv_refr_now(display);
    assert(ui_remote_display_capture(&image, &w, &h, &retry));
    assert(pixel(image, 31, 31) == 0x07ff && pixel(image, 34, 34) == 0x07ff);
    unsigned columns = (width + 31) / 32, rows = (height + 31) / 32;
    for (unsigned tile = 0; tile < rows * columns; ++tile)
        assert(ui_remote_frame_tile_changed(image, tile) == (tile / columns < 2 && tile % columns < 2));

    lv_obj_delete(overlay); /* The receiver hasn't acknowledged the cyan pixels yet. */
    lv_refr_now(display);
    assert(pixel(image, 31, 31) == 0x07ff);
    ui_remote_display_ack();
    sim_virtual_delay(210);
    assert(!ui_remote_display_capture(&image, &w, &h, &retry));
    lv_refr_now(display);
    assert(ui_remote_display_capture(&image, &w, &h, &retry));
    assert(pixel(image, 31, 31) == 0x001f && pixel(image, 34, 34) == 0x001f);
    for (unsigned tile = 0; tile < rows * columns; ++tile)
        assert(ui_remote_frame_tile_changed(image, tile) == (tile / columns < 2 && tile % columns < 2));
    ui_remote_display_ack();
}

static void incomplete_keyframe(lv_display_t *display)
{
    const ui_remote_frame_t *image;
    uint16_t width, height;
    unsigned retry;
    lv_area_t unrelated = {0, 0, 0, 0};
    assert(!ui_remote_display_capture(&image, &width, &height, &retry));
    lv_display_send_event(display, LV_EVENT_REFR_START, NULL);
    lv_display_send_event(display, LV_EVENT_REFR_READY, NULL); /* No pixels arrived. */
    lv_display_send_event(display, LV_EVENT_FLUSH_START, &unrelated);
    sim_virtual_delay(210);
    assert(!ui_remote_display_capture(&image, &width, &height, &retry));
    lv_display_send_event(display, LV_EVENT_REFR_START, NULL);
    lv_display_send_event(display, LV_EVENT_FLUSH_START, &unrelated);
    lv_display_send_event(display, LV_EVENT_REFR_READY, NULL);
    assert(!ui_remote_display_capture(&image, &width, &height, &retry)); /* A single pixel can't complete the keyframe. */
    sim_virtual_delay(210);
    assert(!ui_remote_display_capture(&image, &width, &height, &retry));
    lv_display_send_event(display, LV_EVENT_REFR_START, NULL);
    lv_area_t stripe = {0, 0, lv_display_get_horizontal_resolution(display) - 1, 0};
    for (int y = 0; y < lv_display_get_vertical_resolution(display); ++y)
        lv_display_send_event(display, LV_EVENT_FLUSH_START, &stripe);
    lv_display_send_event(display, LV_EVENT_REFR_READY, NULL);
    assert(!ui_remote_display_capture(&image, &width, &height, &retry)); /* All rows except the first are missing. */
    sim_virtual_delay(210);
}

static void control_contention(void)
{
    unsigned before = lock_attempts, prior_clicks = clicks;
    lock_busy = true;
    assert(!ui_remote_display_heartbeat() && lock_attempts == before);
    lock_busy = false;

    /* Renew across the 32-bit millisecond wrap while the renderer is busy. */
    uint32_t now = (uint32_t)(sim_monotonic_us() / 1000);
    sim_virtual_delay(UINT32_MAX - now - 100U);
    assert(ui_remote_display_control(true));
    assert(ui_remote_display_pointer(0,50,60)); tick();
    assert(ui_remote_display_pointer(2,50,60)); /* Release queued, not yet delivered. */
    lock_busy = true;
    before = lock_attempts;
    sim_virtual_delay(200);
    assert(ui_remote_display_heartbeat() && lock_attempts == before);
    sim_virtual_delay(900);
    assert(ui_remote_display_heartbeat() && lock_attempts == before);
    assert(!ui_remote_display_control(false));
    lock_busy = false;
    assert(!ui_remote_display_heartbeat());
    tick();
    assert(clicks == prior_clicks && !ui_remote_display_pointer(0,50,60));

    assert(ui_remote_display_control(true));
    assert(ui_remote_display_pointer(0,50,60)); tick();
    assert(ui_remote_display_pointer(2,50,60));
    lock_busy = true;
    assert(!ui_remote_display_pointer(3,0,0));
    lock_busy = false;
    tick();
    assert(!ui_remote_display_heartbeat() && clicks == prior_clicks);

    /* Neither a late ping nor input can revive a lease before LVGL's next read. */
    assert(ui_remote_display_control(true));
    sim_virtual_delay(1100);
    assert(!ui_remote_display_heartbeat());
    assert(ui_remote_display_control(true));
    sim_virtual_delay(1100);
    assert(!ui_remote_display_pointer(0,50,60));
    assert(!ui_remote_display_heartbeat());
}

int main(int argc, char **argv)
{
    assert(argc==5);
    sim_clock_set(1792233000);
    int w=atoi(argv[1]),h=atoi(argv[2]),mode=atoi(argv[3]),rot=atoi(argv[4]);
    lv_init();
    lv_display_t *d=lv_display_create(w,h);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_rotation(d,(lv_display_rotation_t)rot);
    if (rot==1 || rot==3) { int tmp=w;w=h;h=tmp; }
    size_t bytes=(size_t)w*(mode==0?20:h)*2;
    void *buf=calloc(1,bytes); assert(buf);
    lv_display_set_buffers(d,buf,NULL,bytes,(lv_display_render_mode_t)mode);
    lv_display_set_flush_cb(d,flush);
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_hex(0xff0000),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_indev_t *local=lv_indev_create();
    lv_indev_set_type(local,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(local,d);
    lv_indev_set_read_cb(local,local_read);
    lv_timer_set_period(lv_indev_get_read_timer(local),10);
    assert(!ui_remote_display_supported() && !ui_remote_display_available());
    ui_remote_display_prepare_dashboard();
    assert(ui_remote_display_supported() && !ui_remote_display_available()); /* HTTP can start before touch setup. */
    lock_busy = true;
    assert(ui_remote_display_init()==ESP_ERR_TIMEOUT);
    assert(ui_remote_display_supported() && !ui_remote_display_available());
    lock_busy = false;
    /* This is the ordinary capability request after the initial layout finishes. */
    assert(ui_remote_display_available());
    lock_busy = true;
    assert(ui_remote_display_available()); /* A busy render during a session isn't a disconnect. */
    lock_busy = false;
    const size_t packet = 16 * 1024U, tiles = 225 * sizeof(uint16_t);
    free_psram = 954552; /* Idle capacity recorded on COM18. */
#ifdef CONFIG_APP_PANEL_VARIANT_S3_480
    largest_block = packet - 1;
    assert(!ui_remote_display_memory_fits(480,480,packet,tiles));
    largest_block = 32 * 1024;
    assert(ui_remote_display_memory_fits(480,480,packet,tiles));
#else
    assert(!ui_remote_display_memory_fits(480,480,packet,tiles)); /* P4 retains its larger reserve. */
#endif
    free_psram = largest_block = 32U * 1024 * 1024;
#ifdef CONFIG_APP_PANEL_VARIANT_S3_480
    if (w == 480 && h == 480) {
        free_psram = 500000; /* Insufficient headroom even if the mirror itself would fit. */
        assert(ui_remote_display_open(w, h) == ESP_ERR_NO_MEM);
        assert(!live_allocations && free_psram == 500000);
        free_psram = 930000; /* Within the recorded 954,552 B, after transport overhead. */
        largest_block = 32 * 1024;
        fail_on_allocation = allocation_calls + 4;
        assert(ui_remote_display_open(w, h) == ESP_ERR_NO_MEM);
        assert(!live_allocations && free_psram == 930000);
        fail_on_allocation = 0;
    }
#endif
    size_t initial_psram = free_psram;
    unsigned prior_allocations = allocation_calls;
    assert(ui_remote_display_open(w - 1, h) == ESP_ERR_INVALID_STATE);
    lock_busy = true;
    assert(ui_remote_display_open(w, h) == ESP_ERR_TIMEOUT);
    lock_busy = false;
    assert(allocation_calls == prior_allocations);
    assert(ui_remote_display_open(w, h)==ESP_OK);
    if (w == 480 && h == 480 && mode == 0 && rot == 0)
        incomplete_keyframe(d);
    frame(w,h,0xf800);
    assert(wakes==0);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x00ff00),0);
    lv_refr_now(NULL); /* Change while the viewer is not asking for a frame. */
    sim_virtual_delay(210);
    frame(w,h,0x07e0);
    lv_obj_t *button=lv_button_create(screen);
    lv_obj_set_size(button,80,80); lv_obj_set_pos(button,30,40);
    lv_obj_add_event_cb(button,clicked,LV_EVENT_CLICKED,NULL);
    lv_refr_now(NULL);
    assert(!ui_remote_display_pointer(0,50,60)); /* read-only by default */
    assert(ui_remote_display_control(true));
    assert(ui_remote_display_pointer(0,50,60)); tick();
    assert(ui_remote_display_pointer(2,50,60)); tick();
    assert(clicks==1 && wakes==1);
    assert(ui_remote_display_pointer(0,50,60)); tick();
    assert(ui_remote_display_pointer(3,0,0)); tick();
    assert(clicks==1); /* Cancel must never turn into CLICKED. */
    assert(ui_remote_display_pointer(0,50,60)); tick();
    local_down=true; tick();
    assert(!ui_remote_display_heartbeat());
    assert(!ui_remote_display_pointer(2,50,60));
    local_down=false; tick();
    assert(clicks==1); /* Physical touch wins without committing the remote press. */
    if (w==480 && mode==0 && rot==0) {
        assert(ui_remote_display_control(true));
        assert(ui_remote_display_pointer(0,50,60)); tick();
        sim_virtual_delay(1100); tick();
        assert(!ui_remote_display_heartbeat() && clicks==1);
        control_contention();
    }
    /* A small update must not force an entire RGB frame to be redrawn. */
    const ui_remote_frame_t *pixels; uint16_t cw, ch; unsigned retry;
    sim_virtual_delay(210);
    (void)ui_remote_display_capture(&pixels, &cw, &ch, &retry);
    lv_refr_now(NULL);
    assert(ui_remote_display_capture(&pixels, &cw, &ch, &retry));
    ui_remote_display_ack();
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, 30, 40);
    lv_obj_set_size(button, 80, 80);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xff0000), 0);
    lv_refr_now(NULL);
    sim_virtual_delay(210);
    assert(!ui_remote_display_capture(&pixels, &cw, &ch, &retry));
    flushed_pixels = 0;
    lv_refr_now(NULL);
    assert(ui_remote_display_capture(&pixels, &cw, &ch, &retry));
    if (mode == 0) assert(flushed_pixels < (size_t)w * h / 4);
    assert(pixel(pixels,0,0) == 0x001f);
    assert(pixel(pixels,50,60) == 0xf800);
    assert(!ui_remote_frame_tile_changed(pixels, 0));
    assert(ui_remote_frame_tile_changed(pixels, (unsigned)(w+31)/32 + 1));
    ui_remote_display_ack();
    if (w == 480 && h == 480) overlay_delta(d, w, h);
    ui_remote_display_suspend(true);
    assert(ui_remote_display_supported() && !ui_remote_display_available());
    assert(!ui_remote_display_pointer(0,50,60));
    ui_remote_display_suspend(false);
    assert(!ui_remote_display_pointer(0,50,60));
    ui_remote_display_close();
    assert(!live_allocations && free_psram == initial_psram);
    assert(ui_remote_display_open(w, h)==ESP_OK);
    assert(!ui_remote_display_pointer(0,50,60));
    lv_display_set_rotation(d,(lv_display_rotation_t)((rot+1)%4));
    assert(!ui_remote_display_available());
    ui_remote_display_close();
    assert(!live_allocations && free_psram == initial_psram);
    assert(ui_remote_display_available());
    printf("PASS remote capture/input %dx%d mode=%d rotation=%d flushes=%u\n",w,h,mode,rot,flushes);
    lv_display_delete(d); free(buf); lv_deinit();
    return 0;
}
