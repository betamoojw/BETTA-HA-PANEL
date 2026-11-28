/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared notification payload parsing for the REST endpoint and MQTT command topic.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app_events.h"

/** @brief Parse and validate a notification payload with a required nonempty message.
 * @param json Borrowed NUL-terminated JSON; len does not bound the parser's read.
 * @param len Body length supplied by the transport, currently unused.
 * @param[out] out Required destination for the new notification, cleared on entry.
 * @param[out] err_msg Optional destination for a borrowed payload error message.
 * @return True with a caller-owned notification. Free it if not handed off
 *         successfully to app_events_publish(). False with a message denotes
 *         a payload error; false without one denotes an internal failure
 *         when valid output pointers are supplied.
 */
bool notification_parse_json(const char *json, size_t len, app_notification_t **out, const char **err_msg);

/** @brief Clear unsupported action or camera IDs before a notification is dispatched.
 *
 * An invalid action also loses its label, and the lock-open flag is
 * retained only for an action on a valid lock.
 */
void notification_sanitize_entities(app_notification_t *notification);
