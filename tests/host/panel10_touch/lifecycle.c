/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * The production touch_init_panel10.c is inserted below; only dependencies are stubbed.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_TIMEOUT 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NOT_FINISHED 4
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
#define LV_VERSION_MAJOR 9
#define LV_INDEV_MODE_TIMER 1
#define LV_EVENT_PRESSED 2
typedef int lv_event_t;
typedef int lv_display_t;
typedef int lv_indev_t;
typedef int lv_timer_t;
typedef void *esp_lcd_panel_io_handle_t;
typedef struct { esp_lcd_panel_io_handle_t io; } touch_t;
typedef touch_t *esp_lcd_touch_handle_t;
typedef struct { int x,y; } esp_lcd_touch_point_data_t;
typedef struct { int ignored; } bsp_display_cfg_t;
typedef esp_err_t (*read_cb_t)(esp_lcd_touch_handle_t,esp_lcd_touch_point_data_t *,uint8_t *,uint8_t,void *);
typedef struct {
    lv_display_t *disp; esp_lcd_touch_handle_t handle;
    struct { float x,y; } scale;
    struct { read_cb_t custom_touch_read; } callbacks;
} esp_lv_adapter_touch_config_t;
static bool ready=true,locked,lock_fails,missing_display,register_fails,missing_timer;
static int bsp_failures,bsp_calls,delays,handles,ios,handle_deletes,io_deletes;
static int display_gets,registrations,modes,timers,periods,timer_ready,events,activities;
static int scan_error,data_error,scans,data_reads;
static uint8_t sample_count=1;
static lv_display_t display;
static lv_indev_t indev;
static lv_timer_t timer;
static read_cb_t registered_read;
static void (*registered_event)(lv_event_t *);
static bool display_is_ready(void) { return ready; }
static bool display_lock(unsigned timeout) { assert(timeout == 1000 && !locked); if(lock_fails) return false;locked=true;return true; }
static void display_unlock(void) { assert(locked);locked=false; }
static void display_note_activity(void) { ++activities; }
static lv_display_t *lv_display_get_default(void) { assert(locked);++display_gets;return missing_display ? NULL : &display; }
static esp_err_t bsp_touch_new(const bsp_display_cfg_t *cfg,esp_lcd_touch_handle_t *tp) {
    assert(cfg && tp && !locked);++bsp_calls;
    if(bsp_failures) { --bsp_failures;*tp=NULL;return ESP_FAIL; }
    *tp=calloc(1,sizeof(touch_t));assert(*tp);(*tp)->io=calloc(1,1);assert((*tp)->io);
    ++handles;++ios;return ESP_OK;
}
static void vTaskDelay(unsigned ticks) { assert(ticks == 250 && !locked);++delays; }
static esp_err_t esp_lcd_touch_del(touch_t *tp) { assert(!locked && handles && tp);--handles;++handle_deletes;free(tp);return ESP_OK; }
static esp_err_t esp_lcd_panel_io_del(void *io) { assert(!locked && ios && io);--ios;++io_deletes;free(io);return ESP_OK; }
static esp_err_t esp_lcd_touch_set_swap_xy(touch_t *tp,bool value) { assert(tp && !value);return ESP_OK; }
static esp_err_t esp_lcd_touch_set_mirror_x(touch_t *tp,bool value) { assert(tp && !value);return ESP_OK; }
static esp_err_t esp_lcd_touch_set_mirror_y(touch_t *tp,bool value) { assert(tp && !value);return ESP_OK; }
static lv_indev_t *esp_lv_adapter_register_touch(const esp_lv_adapter_touch_config_t *cfg) {
    assert(locked && cfg->disp == &display && cfg->handle && cfg->callbacks.custom_touch_read);
    assert(cfg->scale.x == 1 && cfg->scale.y == 1);++registrations;registered_read=cfg->callbacks.custom_touch_read;
    return register_fails ? NULL : &indev;
}
static void lv_indev_add_event_cb(lv_indev_t *i,void (*cb)(lv_event_t *),int event,void *ctx) { assert(locked && i == &indev && event == LV_EVENT_PRESSED && !ctx);++events;registered_event=cb; }
static void lv_indev_set_mode(lv_indev_t *i,int mode) { assert(locked && i == &indev && mode == LV_INDEV_MODE_TIMER);++modes; }
static lv_timer_t *lv_indev_get_read_timer(lv_indev_t *i) { assert(locked && i == &indev);++timers;return missing_timer ? NULL : &timer; }
static void lv_timer_set_period(lv_timer_t *t,uint32_t period) { assert(locked && t == &timer && period == 10);++periods; }
static void lv_timer_ready(lv_timer_t *t) { assert(locked && t == &timer);++timer_ready; }
static esp_err_t esp_lcd_touch_read_data(touch_t *tp) { assert(tp);++scans;return scan_error; }
static esp_err_t esp_lcd_touch_get_data(touch_t *tp,esp_lcd_touch_point_data_t *p,uint8_t *count,uint8_t maximum) { assert(tp && p && maximum);++data_reads;*count=data_error ? 7 : sample_count;return data_error; }

/* TOUCH_INIT */

static void reset_success(void) {
    touch_release_handle();s_touch_ready=false;s_touch_indev=NULL;
    assert(!handles && !ios && !locked);
}
int main(void) {
    ready=false;assert(touch_init() == ESP_ERR_INVALID_STATE && !bsp_calls);ready=true;
    bsp_failures=8;assert(touch_init() == ESP_FAIL && bsp_calls == 8 && delays == 7);
    assert(!handles && !ios && !locked);
    lock_fails=true;assert(touch_init() == ESP_ERR_TIMEOUT);
    assert(!handles && !ios && handle_deletes == 1 && io_deletes == 1);lock_fails=false;
    missing_display=true;assert(touch_init() == ESP_ERR_INVALID_STATE);
    assert(!handles && !ios && !locked);missing_display=false;
    register_fails=true;assert(touch_init() == ESP_FAIL);
    assert(!handles && !ios && !locked);register_fails=false;
    bsp_failures=2;int previous_delays=delays;
    assert(touch_init() == ESP_OK && touch_is_ready());
    assert(delays == previous_delays + 2 && handles == 1 && ios == 1 && !locked);
    assert(modes == 1 && periods == 1 && timer_ready == 1 && events == 1);
    int previous_calls=bsp_calls;
    assert(touch_init() == ESP_OK && bsp_calls == previous_calls && periods == 1);
    registered_event(NULL);assert(activities == 1);
    esp_lcd_touch_point_data_t points[1];uint8_t count=9;
    scan_error=ESP_ERR_TIMEOUT;
    assert(registered_read(s_touch_handle,points,&count,1,NULL) == ESP_ERR_NOT_FINISHED);
    assert(count == 0 && scans == 1 && data_reads == 0);
    scan_error=ESP_OK;data_error=ESP_FAIL;count=9;
    assert(registered_read(s_touch_handle,points,&count,1,NULL) == ESP_ERR_NOT_FINISHED && count == 0);
    data_error=ESP_OK;
    assert(registered_read(s_touch_handle,points,&count,1,NULL) == ESP_OK && count == 1);
    sample_count=0;
    assert(registered_read(s_touch_handle,points,&count,1,NULL) == ESP_OK && count == 0);
    reset_success();
    missing_timer=true;previous_calls=periods;
    assert(touch_init() == ESP_OK && periods == previous_calls);
    reset_success();
    assert(handle_deletes == io_deletes);
    puts("panel10 touch lifecycle: LVGL lock, retry, cleanup and unknown-input handling passed");
    return 0;
}
