/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief MQTT broker configuration, asynchronous lifecycle and editor-facing state.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

/** @brief Broker settings. Keep all pointed-to strings valid until mqtt_mgr_start() returns. */
typedef struct
{
    const char *broker_host;  /**< Nonempty host name or address. */
    int broker_port;          /**< Explicit port, or a nonpositive value for 1883. */
    bool tls_enabled;         /**< Use the TLS transport instead of plain TCP. */
    const char *username;     /**< Optional username; NULL or empty omits it. */
    const char *password;     /**< Optional password; NULL or empty omits it. */
    const char *topic_prefix; /**< Base of panel topics; HA discovery still uses its fixed root. */
    const char *device_name;  /**< HA-visible name; NULL or empty uses the device identity. */
} mqtt_mgr_config_t;

/** @brief Start the MQTT client without waiting for the broker connection.
 *
 * Set up a retained "offline" last-will on
 * "<topic_prefix>/<device_id>/status" (qos=1, retain=true) and, once
 * MQTT_EVENT_CONNECTED fires: publishes a retained "online" availability
 * message, publishes retained HA discovery configs for a `notify` entity
 * and an "Online" connectivity `binary_sensor` under
 * "homeassistant/{notify,binary_sensor}/<device_id>/config" plus an
 * "IP address" diagnostic `sensor` under
 * "homeassistant/sensor/<device_id>/ip/config" (state retained on
 * "<topic_prefix>/<device_id>/ip"; the device block carries the MAC and a
 * configuration_url so HA shows a "Visit device" link), and
 * subscribes (qos=1) to "<topic_prefix>/<device_id>/notify/set" -- JSON
 * payloads on that topic (same shape as POST /api/notifications) are
 * parsed via notification_parse_json() and surfaced as EV_NOTIFICATION_SHOW.
 *
 * @return ESP_OK means the client started or was already running. Changing
 *         the configuration of a running client requires stopping it first.
 */
esp_err_t mqtt_mgr_start(const mqtt_mgr_config_t *cfg);

/** @brief Stop and destroy the client and clear any partial command.
 * Also safe before a successful start.
 */
void mqtt_mgr_stop(void);

/** @return Connection state from MQTT events, not whether the client task has started. */
bool mqtt_mgr_is_connected(void);

/** @brief Return the topic this panel uses for notifications.
 *
 * The command topic ("<topic_prefix>/<device_id>/notify/set") this device
 * subscribes to for notifications, for display in the web UI so users can
 * target it directly with mqtt.publish (HA's MQTT notify platform only
 * forwards the plain message text through notify.send_message -- title,
 * message_id and timeout_sec need a raw JSON payload published straight to
 * this topic instead). Returns an empty string if mqtt_mgr_start() has not
 * been called yet (e.g. MQTT disabled). The returned pointer is valid for
 * the process lifetime. The contents can change on a later start.
 */
const char *mqtt_mgr_get_command_topic(void);

/** @return True when a client exists, even if it is waiting to connect. */
bool mqtt_mgr_is_started(void);

/** @return Borrowed topic and discovery ID, derived from the room or panel identity.
 * Empty until initialized. Storage has process lifetime and can be updated by a later start.
 */
const char *mqtt_mgr_device_id(void);
