/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

/* Numeric owners, never widget pointers: late replies after deletion are discarded.
 * UI calls are nonblocking. Network transmission runs on a worker. */
uint32_t ha_alarm_panel_open(void);
void ha_alarm_panel_close(uint32_t owner);
/* Invalidates queued work and late replies; an already transmitted action cannot be undone. */
void ha_alarm_panel_cancel(uint32_t owner);
/* Takes ownership of command on every path. Never retries a service action. */
esp_err_t ha_alarm_panel_submit(uint32_t owner, cJSON *command, bool history);
/* Transfers a bounded reply to the UI; caller deletes it. */
bool ha_alarm_panel_poll(uint32_t owner, bool *success, bool *history, cJSON **reply);
