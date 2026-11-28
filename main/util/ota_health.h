/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Pure health predicate for marking a freshly installed OTA image valid.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Minimum stable uptime in seconds before canceling rollback. */
#define OTA_HEALTH_MIN_UPTIME_S 60

/** @brief Startup, uptime, and reachability observations supplied by the device validator. */
typedef struct
{
    int64_t uptime_s;
    bool boot_completed; /**< The startup sequence reached the OTA validation step. */
    bool http_answers;   /**< The device's own HTTP server answered a health request. */
    bool network_ok;     /**< A usable network connection or active setup access point. */
} ota_health_t;

/** @return True only when the snapshot meets the uptime and all readiness requirements. */
bool ota_health_ok(const ota_health_t *health);

/** @brief First bootloader project version with rollback support, independent of the app version.
 *
 * Keep this threshold aligned with CONFIG_BOOTLOADER_PROJECT_VER when rollback support changes.
 */
#define OTA_BOOTLOADER_ROLLBACK_VERSION 2

/** @return True when the bootloader project version meets the rollback threshold. */
bool ota_bootloader_has_rollback(uint32_t bootloader_version);
