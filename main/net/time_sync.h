/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
/** @file
 * @brief SNTP lifecycle and paired timezone/local-clock queries.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include "esp_err.h"

/** @brief Apply a catalog timezone without restarting SNTP or persisting settings.
 * @retval ESP_ERR_INVALID_ARG The name is empty, unknown, or too large.
 */
esp_err_t time_sync_set_tz(const char *name);
/** @brief Copy the active zone and derive wall-clock time under the same timezone lock.
 * @param now Input Unix time to convert.
 * @param[out] local Caller-owned broken-down wall-clock time.
 * @param[out] zone Caller-owned buffer for the active zone name or legacy rules.
 * @param zone_size Available bytes in zone, including the NUL terminator.
 * @return True when the conversion succeeded; this does not establish SNTP synchronization.
 */
bool time_sync_local_clock(time_t now, struct tm *local, char *zone, size_t zone_size);
/** @return Borrowed catalog POSIX rules for the exact name, or NULL if unknown. */
const char *time_sync_tz_rules(const char *name);
/** @return Borrowed catalog name at the index, or NULL after the final entry. */
const char *time_sync_tz_name(size_t index);
/** @brief Apply a named zone or legacy POSIX rules at boot; NULL or empty uses the configured default. */
esp_err_t time_sync_set_timezone(const char *tz);
/** @brief Start or restart SNTP polling without waiting for a valid timestamp. */
esp_err_t time_sync_start(const char *ntp_server);
/** @brief Poll the clock within the boot wait budget; timeout does not stop SNTP. */
bool time_sync_wait_for_sync(uint32_t timeout_ms);
