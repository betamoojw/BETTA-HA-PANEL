/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED 1
#endif
#include <SDL.h>
#include "ha/ha_client.h"
#include "ui/alert_policy.h"
#include "ui/periodic_refresh.h"
#include "ui/ui_alert_center.h"
#include "ui/ui_display_protection.h"
#include "ui/ui_i18n.h"
#include "ui/ui_pages.h"
#include "util/coredump_archive.h"

/* SDL supplies host waits. The fixture supplies FreeRTOS task names and owner snapshots. */
typedef SDL_sem *SemaphoreHandle_t;
typedef const char *TaskHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef SDL_SpinLock portMUX_TYPE;
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY UINT32_MAX
#define configMAX_TASK_NAME_LEN 16
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) SDL_AtomicLock(mux)
#define portEXIT_CRITICAL(mux) SDL_AtomicUnlock(mux)

static struct { SemaphoreHandle_t lvgl_mutex; } s_ctx;
static struct {
    SemaphoreHandle_t mutex;
    bool authenticated;
    bool initial_layout_sync_done;
} s_client;

static _Thread_local TaskHandle_t s_task;
static _Thread_local TickType_t s_wait_ticks;
static portMUX_TYPE s_mutex_state_lock;
static TaskHandle_t s_holder;
static unsigned s_depth;
static bool s_report_holder = true;
static bool s_ws_connected;

static BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, TickType_t ticks)
{
    assert(mutex == s_ctx.lvgl_mutex && s_task);
    SDL_AtomicLock(&s_mutex_state_lock);
    if (s_holder == s_task) {
        ++s_depth;
        SDL_AtomicUnlock(&s_mutex_state_lock);
        return pdTRUE;
    }
    SDL_AtomicUnlock(&s_mutex_state_lock);

    s_wait_ticks = ticks;
    int result = SDL_SemWaitTimeout(mutex, ticks);
    if (result == SDL_MUTEX_TIMEDOUT) return 0;
    assert(result == 0);
    SDL_AtomicLock(&s_mutex_state_lock);
    assert(s_holder == NULL && s_depth == 0);
    s_holder = s_task;
    s_depth = 1;
    SDL_AtomicUnlock(&s_mutex_state_lock);
    return pdTRUE;
}

static void xSemaphoreGiveRecursive(SemaphoreHandle_t mutex)
{
    assert(mutex == s_ctx.lvgl_mutex);
    SDL_AtomicLock(&s_mutex_state_lock);
    assert(s_holder == s_task && s_depth != 0);
    if (--s_depth == 0) {
        s_holder = NULL;
        assert(SDL_SemPost(mutex) == 0);
    }
    SDL_AtomicUnlock(&s_mutex_state_lock);
}

static TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t mutex)
{
    assert(mutex == s_ctx.lvgl_mutex);
    SDL_AtomicLock(&s_mutex_state_lock);
    TaskHandle_t holder = s_report_holder ? s_holder : NULL;
    SDL_AtomicUnlock(&s_mutex_state_lock);
    return holder;
}

static BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
    assert(mutex == s_client.mutex && ticks == 0);
    int result = SDL_SemTryWait(mutex);
    assert(result == 0 || result == SDL_MUTEX_TIMEDOUT);
    return result == 0 ? pdTRUE : 0;
}

static void xSemaphoreGive(SemaphoreHandle_t mutex) { assert(SDL_SemPost(mutex) == 0); }
static TaskHandle_t xTaskGetCurrentTaskHandle(void) { return s_task; }
static const char *pcTaskGetName(TaskHandle_t task) { return task ? task : s_task; }
static bool ha_ws_is_connected(void) { return s_ws_connected; }
#ifndef strlcpy
static void copy_task_name(char *dst, const char *src, size_t size) { snprintf(dst, size, "%s", src); }
#define strlcpy copy_task_name
#endif

static const char *const TAG = "adapter";
static char s_warning[192];
static unsigned s_warnings;
#define ESP_LOGE(tag, ...) do { \
    (void)(tag); \
    ++s_warnings; \
    snprintf(s_warning, sizeof(s_warning), __VA_ARGS__); \
} while (0)
#define ESP_RETURN_ON_FALSE(ok, err, tag, msg) do { \
    if (!(ok)) { ESP_LOGE(tag, msg); return err; } \
} while (0)

/* UI sinks observe the actual runtime decisions with a stable Wi-Fi connection. */
static const bool s_alert_wifi_expected = true;
static const bool s_alert_ha_expected = true;
static const bool s_alert_mqtt_expected = false;
static bool s_topbar_connected, s_topbar_synced, s_ha_alert;
static int64_t s_now_ms = 30000;
static lv_obj_t *s_refresh_label;
static unsigned s_refreshes, s_frames;

static bool net_mgr_is_connected(void) { return true; }
static bool net_mgr_uses_ethernet(void) { return false; }
static bool mqtt_mgr_is_connected(void) { return false; }
static bool wifi_mgr_is_setup_ap_active(void) { return false; }
static int64_t esp_timer_get_time(void) { return s_now_ms * 1000; }
const char *ui_i18n_get(const char *key, const char *fallback) { (void)key; return fallback; }

bool coredump_archive_snapshot(coredump_record_t out[COREDUMP_STORE_LIMIT], size_t *count)
{
    (void)out;
    (void)count;
    return false;
}
void ui_alert_center_crash(unsigned index, const coredump_record_t *record) { (void)index; (void)record; }
static void ui_runtime_check_alerts(const char *entity_id) { assert(entity_id == NULL); }
void ui_alert_center_refresh(uint64_t now_ms) { assert(now_ms == (uint64_t)s_now_ms); }
void ui_pages_set_topbar_datetime(const struct tm *info) { assert(info != NULL); }

void ui_pages_set_topbar_status(bool wifi, bool setup_ap, bool connected, bool synced)
{
    assert(wifi && !setup_ap);
    s_topbar_connected = connected;
    s_topbar_synced = synced;
}

void ui_alert_center_set(unsigned slot, const char *title, const char *detail, uint64_t expiry)
{
    assert(slot == UI_ALERT_HA && strcmp(title, "Home Assistant disconnected") == 0 && detail && expiry == 0);
    s_ha_alert = true;
}

void ui_alert_center_clear(unsigned slot)
{
    if (slot == UI_ALERT_HA) s_ha_alert = false;
}

void ui_display_protection_poll(void)
{
    lv_label_set_text_fmt(s_refresh_label, "%u", ++s_refreshes);
}

#include "panel10_lock_fixture.h"

static void make_refresh_due(void)
{
    s_now_ms += UI_PERIODIC_REFRESH_MS;
    lv_tick_inc(UI_PERIODIC_REFRESH_MS);
}

static void run_refresh(lv_display_t *display)
{
    const unsigned before = s_refreshes;
    assert(esp_lv_adapter_lock(-1) == ESP_OK);
    lv_timer_handler();
    lv_refr_now(display);
    esp_lv_adapter_unlock();
    assert(s_refreshes == before + 1);
}

static void begin_ha_update(bool connected, bool synced)
{
    assert(SDL_SemTryWait(s_client.mutex) == 0);
    s_ws_connected = connected;
    s_client.authenticated = connected;
    s_client.initial_layout_sync_done = synced;
}

static void expect_ha_tick(lv_display_t *display, bool connected, bool synced, bool alert)
{
    make_refresh_due();
    run_refresh(display);
    assert(s_topbar_connected == connected && s_topbar_synced == synced && s_ha_alert == alert);
}

static void check_ha_busy_refresh(lv_display_t *display)
{
    begin_ha_update(true, true);
    expect_ha_tick(display, false, false, false); /* No sample yet and no disconnect alert. */
    xSemaphoreGive(s_client.mutex);
    expect_ha_tick(display, true, true, false);

    begin_ha_update(false, false);
    expect_ha_tick(display, true, true, false);
    expect_ha_tick(display, true, true, false);
    xSemaphoreGive(s_client.mutex);
    expect_ha_tick(display, false, false, true); /* A fresh disconnect raises the alert. */

    begin_ha_update(true, false);
    expect_ha_tick(display, false, false, true); /* Keep the existing alert until the status is readable. */
    xSemaphoreGive(s_client.mutex);
    expect_ha_tick(display, true, false, false);
    begin_ha_update(true, true);
    xSemaphoreGive(s_client.mutex);
    expect_ha_tick(display, true, true, false);
    puts("HA busy status and existing alert preserved. Fresh disconnect, reconnect and sync applied.");
}

enum { WAIT_MS = 20 };
static const uintptr_t s_waiter_pc = 0x2000;
static const uintptr_t s_holder_pc = 0x1000;

static int timed_waiter(void *arg)
{
    s_task = arg;
    int result = betta_lv_adapter_lock_at(WAIT_MS, s_waiter_pc);
    assert(s_wait_ticks == WAIT_MS);
    return result;
}

static void expect_timeout(const char *holder, uintptr_t holder_pc)
{
    char waiter[] = "ui_runtime";
    char expected[sizeof(s_warning)];
    snprintf(expected, sizeof(expected),
        "Failed to acquire LVGL lock (timeout_ms=%d waiter=%s caller=%p holder=%s holder_pc=%p)",
        WAIT_MS, waiter, (void *)s_waiter_pc, holder, (void *)holder_pc);
    const unsigned before = s_warnings;
    SDL_Thread *thread = SDL_CreateThread(timed_waiter, waiter, waiter);
    assert(thread);
    int result = ESP_OK;
    SDL_WaitThread(thread, &result);
    assert(result == ESP_ERR_TIMEOUT && s_warnings == before + 1);
    assert(strcmp(s_warning, expected) == 0);
    puts(s_warning);
}

static void check_lvgl_lock_recovery(lv_display_t *display)
{
    const unsigned refreshes = s_refreshes, frames = s_frames;
    assert(betta_lv_adapter_lock_at(-1, s_holder_pc) == ESP_OK);
    assert(betta_lv_adapter_lock_at(-1, 0x1500) == ESP_OK);
    make_refresh_due();
    expect_timeout("lvgl_worker", s_holder_pc);
    esp_lv_adapter_unlock();
    s_report_holder = false;
    expect_timeout("unavailable", 0);
    esp_lv_adapter_unlock();
    s_report_holder = true;

    assert(s_refreshes == refreshes && s_frames == frames);
    const unsigned warnings = s_warnings;
    run_refresh(display);
    assert(s_frames > frames && s_warnings == warnings);
    puts("Periodic LVGL timer and frame flush resumed after the lock was released.");
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    if (lv_display_flush_is_last(display)) ++s_frames;
    lv_display_flush_ready(display);
}

int main(void)
{
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    s_ctx.lvgl_mutex = SDL_CreateSemaphore(1);
    s_client.mutex = SDL_CreateSemaphore(1);
    assert(s_ctx.lvgl_mutex && s_client.mutex);
    s_task = "lvgl_worker";

    lv_init();
    lv_display_t *display = lv_display_create(64, 32);
    static uint16_t pixels[64 * 32];
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    s_refresh_label = lv_label_create(lv_screen_active());
    lv_timer_t *timer = ui_periodic_refresh_create(&s_periodic_refresh);
    assert(timer);
    atomic_store(&s_periodic_refresh.enabled, true);

    check_ha_busy_refresh(display);
    check_lvgl_lock_recovery(display);

    lv_timer_delete(timer);
    lv_deinit();
    SDL_DestroySemaphore(s_client.mutex);
    SDL_DestroySemaphore(s_ctx.lvgl_mutex);
    SDL_Quit();
    return 0;
}
