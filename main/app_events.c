/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared FreeRTOS application event queue and coalesced layout revision.
 */
#include "app_events.h"

#include "esp_log.h"
#include <stdatomic.h>

#include "util/log_tags.h"

/** @brief Queue owned by the event module and created during startup. */
static QueueHandle_t s_event_queue = NULL;
/** @brief In-memory layout change stamp, independent of the queue's lifetime or capacity. */
static atomic_uint s_layout_revision;

uint32_t app_events_layout_revision(void)
{
    return atomic_load(&s_layout_revision);
}

esp_err_t app_events_init(void)
{
    if (s_event_queue != NULL)
    {
        return ESP_OK;
    }
    s_event_queue = xQueueCreate(APP_EVENT_QUEUE_LENGTH, sizeof(app_event_t));
    if (s_event_queue == NULL)
    {
        ESP_LOGE(TAG_APP, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

QueueHandle_t app_events_get_queue(void)
{
    return s_event_queue;
}

bool app_events_publish(const app_event_t *event, TickType_t timeout_ticks)
{
    if (event == NULL)
    {
        return false;
    }
    if (event->type == EV_LAYOUT_UPDATED)
    {
        /* The UI retries from the revision even when the queue is full. */
        atomic_fetch_add(&s_layout_revision, 1U);
        return true;
    }
    if (s_event_queue == NULL)
    {
        return false;
    }
    return xQueueSend(s_event_queue, event, timeout_ticks) == pdTRUE;
}

bool app_events_receive(app_event_t *event, TickType_t timeout_ticks)
{
    if (s_event_queue == NULL || event == NULL)
    {
        return false;
    }
    return xQueueReceive(s_event_queue, event, timeout_ticks) == pdTRUE;
}
