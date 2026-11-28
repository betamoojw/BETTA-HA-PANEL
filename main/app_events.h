/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Event payloads and producer/consumer contracts for the UI task.
 */
#pragma once
#include "util/chime_synth.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "app_config.h"

/** @brief Event kind selecting the active payload, if any. */
typedef enum
{
    EV_NONE = 0,                   /**< No operation; also the zero-initialized kind. */
    EV_HA_CONNECTED,               /**< Refresh dashboard state without clearing entities missing during sync. */
    EV_HA_DISCONNECTED,            /**< Refresh the connectivity indicator. */
    EV_HA_STATE_CHANGED,           /**< Apply the cached state of one entity. */
    EV_HA_ENERGY_CHANGED,          /**< Refresh the Energy views from the cached model. */
    EV_LAYOUT_UPDATED,             /**< Increment the polled revision instead of enqueuing a payload. */
    EV_DISPLAY_ROTATION,           /**< Apply an automatic rotation unless a manual target is pending. */
    EV_DISPLAY_PROTECTION_UPDATED, /**< Reload persisted display-protection settings. */
    EV_UI_NAVIGATE,                /**< Select the page named by the navigation payload. */
    EV_UI_TAP,                     /**< Ask the UI to open or dismiss a test popup. */
    EV_NOTIFICATION_SHOW,          /**< Transfer a heap payload to the UI task for display. */
} app_event_type_t;

/** @brief Heap-allocated notification body passed by pointer to keep event slots small.
 *
 * The queue preallocates fixed-size event slots. Ownership transfers to the
 * UI task on successful publish; on failure the sender must free the body.
 */
typedef struct
{
    char title[APP_NOTIFICATION_MAX_TITLE_LEN];               /**< Heading displayed in the popup. */
    char message[APP_NOTIFICATION_MAX_MESSAGE_LEN];           /**< Body text, separate from the heading. */
    char message_id[APP_NOTIFICATION_MAX_ID_LEN];             /**< Optional identifier carried with the message. */
    uint32_t timeout_sec;                                     /**< Seconds before auto-dismiss; zero keeps the popup until tapped. */
    chime_kind_t sound;                                       /**< Optional chime; a default-initialized payload is silent. */
    char action_entity_id[APP_MAX_ENTITY_ID_LEN];             /**< Optional entity invoked by the popup action. */
    char action_label[APP_NOTIFICATION_MAX_ACTION_LABEL_LEN]; /**< Text of the optional action button. */
    /** @brief Use the lock's open action rather than unlock when the payload requests it. */
    bool action_open;
    /** @brief Optional camera entity whose snapshot refreshes while the card is open. */
    char camera_entity_id[APP_MAX_ENTITY_ID_LEN];
    /** @brief Keep the last HTTP/MQTT message available for reopening from the clock.
     * Alerts and system notices leave this false.
     */
    bool remember;
} app_notification_t;

/** @brief Cache key for an entity update; the state itself remains in the HA model. */
typedef struct
{
    char entity_id[APP_MAX_ENTITY_ID_LEN]; /**< Copied, NUL-terminated entity ID. */
} app_event_state_changed_t;

/** @brief Reference to the requested page in the current layout. */
typedef struct
{
    char page_id[APP_MAX_PAGE_ID_LEN]; /**< Copied page ID, not an array index. */
} app_event_navigate_t;

/** @brief Test-popup request checked by the UI before it acts. */
typedef struct
{
    char widget_id[APP_MAX_WIDGET_ID_LEN]; /**< Widget ID for an open; unused when closing. */
    bool close;                            /**< Dismiss the popup opened by the last test tap. */
} app_event_tap_t;

/** @brief Notification pointer carried in a queue slot. */
typedef struct
{
    app_notification_t *notification; /**< Ownership transfers only on successful publish. */
} app_event_notification_t;

/** @brief Fixed-size queue record; only the union member selected by type is valid. */
typedef struct
{
    app_event_type_t type; /**< Discriminant for the payload in data. */

    union
    {
        app_event_state_changed_t ha_state_changed; /**< Entity key for EV_HA_STATE_CHANGED. */
        app_event_navigate_t navigate;              /**< Page reference for EV_UI_NAVIGATE. */
        app_event_tap_t tap;                        /**< Popup operation for EV_UI_TAP. */
        int rotation_degrees;                       /**< Requested display rotation in degrees for EV_DISPLAY_ROTATION. */
        app_event_notification_t notification;      /**< Owned payload for EV_NOTIFICATION_SHOW after publish. */
    } data;                                         /**< Kind-specific payload; unused for kinds without data. */
} app_event_t;

/** @brief Create the shared queue during startup, before normal producers or consumers run.
 * @retval ESP_OK The queue is ready, including when it was already initialized.
 * @retval ESP_ERR_NO_MEM The queue could not be allocated.
 */
esp_err_t app_events_init(void);

/** @return Borrowed queue handle, or NULL before successful initialization. Do not delete it. */
QueueHandle_t app_events_get_queue(void);

/** @brief Publish an application event from task context.
 * @param event Borrowed event record copied by the queue. Notification payload ownership
 *              passes to the UI on success; on failure it stays with the sender.
 * @param timeout_ticks Maximum queue wait in FreeRTOS ticks; zero does not wait.
 * @return True when accepted. A layout update only changes the revision, even if the
 *         queue is full or not initialized.
 */
bool app_events_publish(const app_event_t *event, TickType_t timeout_ticks);

/** @return Current layout revision. Consumers compare it with their last applied
 *          revision; multiple publishes may be applied in one reload.
 */
uint32_t app_events_layout_revision(void);

/** @brief Dequeue the next event from task context.
 * @param[out] event Caller-owned destination for the event record.
 * @param timeout_ticks Maximum receive wait in FreeRTOS ticks.
 * @return True only when a record was dequeued. Layout revisions are polled separately.
 */
bool app_events_receive(app_event_t *event, TickType_t timeout_ticks);
