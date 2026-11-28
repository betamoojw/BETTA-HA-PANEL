/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Synchronized camera settings snapshots and persistence.
 */
#pragma once
#include "camera/camera_config.h"
#include "esp_err.h"
/** @brief Load camera settings at startup without creating or replacing the file.
 *
 * Repeated calls are no-ops once the mutex exists, including after a file-load error.
 *
 * @pre Mount LittleFS and initialize the store before starting camera readers.
 * @return ESP_OK for valid saved settings or a missing file. An invalid or unreadable file reports failure and is retained.
 */
esp_err_t camera_store_init(void);

/** @brief Copy settings, including credentials, under a bounded mutex wait.
 * @param[out] out Caller-owned snapshot; use redacted serialization before exporting it.
 * @return True when a snapshot was copied; false leaves out unchanged.
 */
bool camera_store_get(camera_settings_t *out);

/** @brief Copy only built-in settings under a shorter mutex wait, without reading flash.
 * @param[out] out Caller-owned snapshot, unchanged if the settings cannot be read.
 * @return True when the local settings were copied.
 */
bool camera_store_local(camera_config_t *out);

/** @brief Look up a saved source by exact ID. A match does not imply that it is enabled.
 * @param id Saved source ID; the built-in camera is not in this list.
 * @param[out] out Caller-owned source snapshot, which can include credentials.
 * @return True on a match. If no match is copied, out is unchanged.
 */
bool camera_store_source(const char *id, camera_source_t *out);

/** @brief Persist a validated update before publishing the new settings.
 * @pre Call outside the display lock; the operation can wait for storage.
 * @param root Borrowed JSON update.
 * @return ESP_OK on persistence and publication. On failure, live settings and generation are unchanged.
 */
esp_err_t camera_store_update(const cJSON *root);

/** @return Generation of the live settings, advanced on every successful write, even if values are unchanged. */
uint32_t camera_store_generation(void);

/** @brief Check the saved enable flag without waiting, independently of camera availability.
 * @return False for an unknown source or when the store is busy.
 */
bool camera_store_output_allowed(const char *id);
