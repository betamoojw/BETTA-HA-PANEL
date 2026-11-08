/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_alarm_panel_client.h"
#include "ha/ha_client.h"
#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

#define MAILBOXES (2 * APP_MAX_WIDGETS_TOTAL)
#define QUEUED_REQUESTS 16
#define REPLY_TIMEOUT_US 20000000LL
typedef struct {
    uint32_t owner, request;
    bool ready, success, history;
    int64_t deadline;
    cJSON *reply;
} mailbox_t;
typedef struct { uint32_t request; cJSON *command; } job_t;
static mailbox_t *s_boxes;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_sequence;
static QueueHandle_t s_jobs;

static uint32_t next_id(void)
{
    if (++s_sequence == 0) ++s_sequence;
    return s_sequence;
}
static void reply_cb(bool success, cJSON *result, void *user)
{
    uint32_t request = (uint32_t)(uintptr_t)user;
    const int64_t now = esp_timer_get_time();
    /* Copy only the newest 40 events. HA owns result only during this call. */
    cJSON *copy = NULL;
    if (success && cJSON_IsArray(result)) {
        copy = cJSON_CreateArray();
        int count = cJSON_GetArraySize(result);
        for (int i = count > 40 ? count - 40 : 0; copy && i < count; ++i) {
            cJSON *row = cJSON_Duplicate(cJSON_GetArrayItem(result, i), true);
            if (!row || !cJSON_AddItemToArray(copy, row)) {
                cJSON_Delete(row); cJSON_Delete(copy); copy = NULL; success = false; break;
            }
        }
        if (!copy) success = false;
    }
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; i < MAILBOXES; ++i) {
        mailbox_t *box = &s_boxes[i];
        if (box->owner && box->request == request && !box->ready && now < box->deadline) {
            box->reply = copy;
            copy = NULL;
            box->success = success && (!box->history || box->reply != NULL);
            box->ready = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_lock);
    cJSON_Delete(copy);
}
static bool request_live(uint32_t request)
{
    bool live = false;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; i < MAILBOXES; ++i)
        if (s_boxes[i].owner && s_boxes[i].request == request && !s_boxes[i].ready && now < s_boxes[i].deadline) live = true;
    portEXIT_CRITICAL(&s_lock);
    return live;
}
static void worker(void *unused)
{
    (void)unused;
    job_t job;
    for (;;) {
        if (xQueueReceive(s_jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        if (!request_live(job.request)) { cJSON_Delete(job.command); continue; }
        esp_err_t err = ha_client_send_command_with_response(job.command, reply_cb, (void *)(uintptr_t)job.request);
        /* Early rejection has no callback; a synchronous failure callback is harmlessly deduplicated. */
        if (err != ESP_OK) reply_cb(false, NULL, (void *)(uintptr_t)job.request);
    }
}
uint32_t ha_alarm_panel_open(void)
{
    /* Called only by the serialized UI factory. Allocate the worker only when used. */
    if (!s_jobs) {
        s_boxes = heap_caps_calloc(MAILBOXES, sizeof(*s_boxes), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_boxes) return 0;
        s_jobs = xQueueCreate(QUEUED_REQUESTS, sizeof(job_t));
        if (!s_jobs) { free(s_boxes); s_boxes = NULL; return 0; }
        if (xTaskCreate(worker, "alarm_requests", 8192, NULL, 3, NULL) != pdPASS) {
            vQueueDelete(s_jobs); s_jobs = NULL; free(s_boxes); s_boxes = NULL; return 0;
        }
    }
    uint32_t owner = 0;
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; i < MAILBOXES; ++i) if (!s_boxes[i].owner) {
        owner = s_boxes[i].owner = next_id(); break;
    }
    portEXIT_CRITICAL(&s_lock);
    return owner;
}
void ha_alarm_panel_close(uint32_t owner)
{
    cJSON *reply = NULL;
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; owner && i < MAILBOXES; ++i) if (s_boxes[i].owner == owner) {
        reply = s_boxes[i].reply;
        memset(&s_boxes[i], 0, sizeof(s_boxes[i])); break;
    }
    portEXIT_CRITICAL(&s_lock);
    cJSON_Delete(reply);
}
void ha_alarm_panel_cancel(uint32_t owner)
{
    cJSON *reply = NULL;
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; owner && i < MAILBOXES; ++i) if (s_boxes[i].owner == owner) {
        reply = s_boxes[i].reply;
        memset(&s_boxes[i], 0, sizeof(s_boxes[i]));
        s_boxes[i].owner = owner;
        break;
    }
    portEXIT_CRITICAL(&s_lock);
    cJSON_Delete(reply);
}
esp_err_t ha_alarm_panel_submit(uint32_t owner, cJSON *command, bool history)
{
    uint32_t request = 0;
    if (command && owner && s_jobs) {
        int64_t deadline = esp_timer_get_time() + REPLY_TIMEOUT_US;
        portENTER_CRITICAL(&s_lock);
        for (unsigned i = 0; i < MAILBOXES; ++i) if (s_boxes[i].owner == owner && !s_boxes[i].request) {
            request = s_boxes[i].request = next_id();
            s_boxes[i].deadline = deadline;
            s_boxes[i].history = history;
            break;
        }
        portEXIT_CRITICAL(&s_lock);
    }
    if (!request) { cJSON_Delete(command); return ESP_ERR_INVALID_STATE; }
    job_t job = {.request = request, .command = command};
    if (xQueueSend(s_jobs, &job, 0) != pdTRUE) {
        cJSON_Delete(command);
        reply_cb(false, NULL, (void *)(uintptr_t)request);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
bool ha_alarm_panel_poll(uint32_t owner, bool *success, bool *history, cJSON **reply)
{
    bool ready = false;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; owner && i < MAILBOXES; ++i) {
        mailbox_t *box = &s_boxes[i];
        if (box->owner != owner || !box->request) continue;
        if (!box->ready && now >= box->deadline) { box->ready = true; box->success = false; }
        if (box->ready) {
            *success = box->success; *history = box->history; *reply = box->reply;
            box->request = 0; box->ready = false; box->reply = NULL;
            ready = true;
        }
        break;
    }
    portEXIT_CRITICAL(&s_lock);
    return ready;
}
