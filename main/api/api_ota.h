/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Starting an OTA install from inside the firmware (the automatic update, net/auto_update_task.c); the editor uses the HTTP handlers.
 */
/** @file
 * @brief Shared firmware update phase and URL install interface.
 */
#pragma once

#include "esp_err.h"

/** @brief Coarse installation phase shared with the automatic-update worker. */
typedef enum
{
    API_OTA_PHASE_IDLE = 0,  /**< No installation is in progress or awaiting reboot. */
    API_OTA_PHASE_RUNNING,   /**< Download or flash write is in progress. */
    API_OTA_PHASE_REBOOTING, /**< The validated image was selected and a reboot is pending. */
    API_OTA_PHASE_FAILED     /**< The most recent installation ended with an error. */
} api_ota_phase_t;

/** @brief Start a URL install using the same validation as the HTTP endpoint.
 * @param url Image URL; project identity and any matching release digest are checked.
 * @retval ESP_OK A worker was started; installation has not completed yet.
 * @retval ESP_ERR_INVALID_ARG The URL was not accepted.
 * @retval ESP_ERR_NOT_FOUND No OTA partition is available.
 * @retval ESP_ERR_INVALID_STATE Another install is running or a restart is pending.
 * @return Other errors indicate failure to start the worker.
 */
esp_err_t api_ota_start_url(const char *url);
/** @brief Read the current installation phase without starting a download or update check. */
api_ota_phase_t api_ota_phase(void);
