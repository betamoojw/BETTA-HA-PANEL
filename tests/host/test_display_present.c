/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "drivers/display_present.h"

static uint16_t fb[2][35], old_front[35];
static frame_cb_t callback;
static void *callback_ctx;
static int active, submitted, flips, events, waits;
static int64_t now;
static bool stalled, fail_draw, fail_buffers, race;
void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { (void)caps; return calloc(n,size); }
void heap_caps_free(void *p) { free(p); }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return (void *)1; }
void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
int xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *w) { (void)s; *w=0; return 1; }
int64_t esp_timer_get_time(void) { return now; }
esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t p,uint32_t count,void **a,...)
{
    (void)p; assert(count==2); va_list args; va_start(args,a);
    void **b=va_arg(args,void **); *a=fb[0]; *b=fb[1]; va_end(args);
    return fail_buffers ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t p,const esp_lcd_dpi_panel_event_callbacks_t *c,void *user)
{ (void)p; callback=c->on_frame_buf_complete; callback_ctx=user; return ESP_OK; }
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p,int x,int y,int w,int h,const void *buf)
{
    assert(p && x==0 && y==0 && w==7 && h==5);
    assert(buf==fb[1-active]);
    if(fail_draw) return ESP_FAIL;
    submitted=1-active; ++flips; events=waits=0;
    memcpy(old_front,fb[active],sizeof(old_front));
    if(race) callback(p,NULL,callback_ctx); /* Completion before submit returns. */
    return ESP_OK;
}
int xSemaphoreTake(SemaphoreHandle_t s,unsigned ticks)
{
    (void)s;
    assert(ticks>0 && ticks<=1000);
    assert(memcmp(fb[active],old_front,sizeof(old_front))==0);
    if(stalled) { now+=1000000; return 0; }
    now+=16000; ++waits;
    if(waits==1) return 1; /* Stale token must not release the old framebuffer. */
    callback((void *)1,NULL,callback_ctx);
    if(++events==2) active=submitted;
    return 1;
}
static display_present_t *create(void)
{
    active=flips=events=waits=0; now=0; stalled=fail_draw=fail_buffers=false;
    for(unsigned i=0;i<35;i++) fb[0][i]=(uint16_t)(i+1);
    memset(fb[1],0,sizeof(fb[1]));
    return display_present_create((void *)1,7,5);
}
int main(void)
{
    assert(!display_present_create(NULL,7,5));
    fail_buffers=true;
    assert(!display_present_create((void *)1,7,5));
    for(unsigned trial=0;trial<2;trial++) {
        display_present_t *ctx=create(); assert(ctx); race=trial;
        assert(memcmp(fb[0],fb[1],sizeof(fb[0]))==0);
        uint16_t pixels[]={100,101,102,103,104,105};
        uint16_t expected[35]; memcpy(expected,fb[0],sizeof(expected));
        assert(display_present_write(ctx,(display_rect_t){1,1,4,3},pixels,false)==ESP_OK);
        assert(flips==0 && memcmp(fb[0],expected,sizeof(expected))==0);
        memcpy(expected+8,pixels,6); memcpy(expected+15,pixels+3,6);
        assert(display_present_write(ctx,(display_rect_t){6,4,7,5},pixels,true)==ESP_OK); expected[34]=100;
        assert(flips==1 && events==0 && waits==0); /* Submit releases the UI without waiting. */
        assert(memcmp(fb[1],expected,sizeof(expected))==0);
        assert(display_present_write(ctx,(display_rect_t){0,0,1,1},pixels,false)==ESP_OK);
        assert(events==2 && waits==3 && active==1); /* Next write waits before any reuse. */
        assert(memcmp(fb[1],expected,sizeof(expected))==0); expected[0]=100;
        assert(memcmp(fb[0],expected,sizeof(expected))==0);
        assert(display_present_write(ctx,(display_rect_t){0,0,1,1},pixels,true)==ESP_OK);
        assert(flips==2 && waits==0 && active==1);
        /* Frames complete while LVGL handles input or sleeps. No lock-held wait
         * should occur on the next write, even after a long idle interval. */
        callback((void *)1,NULL,callback_ctx); callback((void *)1,NULL,callback_ctx);
        active=submitted; now+=2000000;
        assert(display_present_write(ctx,(display_rect_t){0,0,1,1},pixels,false)==ESP_OK);
        assert(waits==0 && memcmp(fb[0],expected,sizeof(expected))==0 && memcmp(fb[1],expected,sizeof(expected))==0);
        assert(display_present_write(ctx,(display_rect_t){0,0,1,1},pixels,true)==ESP_OK);
        assert(display_present_write(ctx,(display_rect_t){-1,0,1,1},pixels,true)==ESP_ERR_INVALID_ARG);
        stalled=true;
        assert(display_present_write(ctx,(display_rect_t){0,0,3,2},pixels,true)==ESP_ERR_TIMEOUT);
        assert(memcmp(fb[active],expected,sizeof(expected))==0);
        uint16_t preserved[2][35]; memcpy(preserved,fb,sizeof(fb));
        assert(display_present_write(ctx,(display_rect_t){0,0,1,1},pixels,true)==ESP_ERR_INVALID_STATE);
        assert(memcmp(preserved,fb,sizeof(fb))==0);
        free(ctx);
    }
    display_present_t *ctx=create(); fail_draw=true; uint16_t pixel=7;
    assert(display_present_write(ctx,(display_rect_t){0,0,1,1},&pixel,true)==ESP_FAIL);
    assert(display_present_write(ctx,(display_rect_t){0,0,1,1},&pixel,true)==ESP_ERR_INVALID_STATE);
    free(ctx);
    puts("display presentation: PASS (nonblocking submit, partial repair, delayed reuse, idle completion, submit race, stale signal, timeout and failed-submit quarantine)");
}
