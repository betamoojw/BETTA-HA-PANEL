/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Exercise the production capture/input module against real LVGL, without a board.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "lvgl.h"
#include "ui/ui_remote_display.h"
#include "drivers/display_init.h"

/* Native Windows assert() can open a modal dialog and block unattended tests. */
#undef assert
#define assert(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

static unsigned clicks, wakes, flushes;
static size_t flushed_pixels;
static bool local_down;
static void local_read(lv_indev_t *i, lv_indev_data_t *data)
{ (void)i; data->point=(lv_point_t){5,5}; data->state=local_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
bool display_lock(uint32_t timeout) { (void)timeout; return true; }
void display_unlock(void) {}
void display_note_activity(void) { wakes++; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)p; flushes++; flushed_pixels += lv_area_get_size(a); lv_display_flush_ready(d); }
static void clicked(lv_event_t *e) { (void)e; clicks++; }
static void tick(void) { lv_tick_inc(20); lv_timer_handler(); }
static void frame(uint16_t w, uint16_t h, uint16_t expected)
{
    const uint8_t *pixels = NULL; uint16_t width=0,height=0; unsigned retry=0;
    assert(!ui_remote_display_capture(&pixels,&width,&height,&retry));
    lv_refr_now(NULL);
    assert(ui_remote_display_capture(&pixels,&width,&height,&retry));
    assert(width==w && height==h);
    /* Corners, including the last partial strip, must all be initialized. */
    const uint16_t *p=(const uint16_t *)pixels;
    if (p[0]!=expected || p[w-1]!=expected || p[(size_t)w*h-1]!=expected)
        fprintf(stderr,"corners=%04x,%04x,%04x expected=%04x\n",p[0],p[w-1],p[(size_t)w*h-1],expected);
    assert(p[0]==expected && p[w-1]==expected && p[(size_t)w*h-1]==expected);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x0000ff),0);
    lv_refr_now(NULL);
    assert(p[0]==expected && p[(size_t)w*h-1]==expected); /* Frozen until ACK. */
    ui_remote_display_ack();
}
int main(int argc, char **argv)
{
    assert(argc==5);
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
    assert(ui_remote_display_init()==ESP_OK);
    assert(ui_remote_display_open()==ESP_OK);
    frame(w,h,0xf800);
    assert(wakes==0);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x00ff00),0);
    lv_refr_now(NULL); /* Change while the viewer is not asking for a frame. */
    /* Use a real delay only for the 200 ms capture throttle. */
    SDL_Delay(210);
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
        SDL_Delay(1100); tick();
        assert(!ui_remote_display_heartbeat() && clicks==1);
    }
    /* A small update must not force an entire RGB frame to be redrawn. */
    const uint8_t *pixels; uint16_t cw, ch; unsigned retry;
    SDL_Delay(210);
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
    SDL_Delay(210);
    assert(!ui_remote_display_capture(&pixels, &cw, &ch, &retry));
    flushed_pixels = 0;
    lv_refr_now(NULL);
    assert(ui_remote_display_capture(&pixels, &cw, &ch, &retry));
    if (mode == 0) assert(flushed_pixels < (size_t)w * h / 4);
    assert(((const uint16_t *)pixels)[0] == 0x001f);
    assert(((const uint16_t *)pixels)[60 * w + 50] == 0xf800);
    ui_remote_display_ack();
    ui_remote_display_suspend(true);
    assert(!ui_remote_display_available());
    assert(!ui_remote_display_pointer(0,50,60));
    ui_remote_display_suspend(false);
    assert(!ui_remote_display_pointer(0,50,60));
    ui_remote_display_close();
    assert(ui_remote_display_open()==ESP_OK);
    assert(!ui_remote_display_pointer(0,50,60));
    lv_display_set_rotation(d,(lv_display_rotation_t)((rot+1)%4));
    assert(!ui_remote_display_available());
    ui_remote_display_close();
    assert(ui_remote_display_available());
    printf("PASS remote capture/input %dx%d mode=%d rotation=%d flushes=%u\n",w,h,mode,rot,flushes);
    lv_display_delete(d); free(buf); lv_deinit();
    return 0;
}
