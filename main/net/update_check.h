/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Daily firmware update check: a few minutes after boot and then once a day, the panel asks the GitHub
 * release API of the fork whether a newer build for its variant exists. It only reports; installing
 * is a user action in the editor (OTA by URL) or, when ota.auto_install is on, auto_update_task.c.
 */
/** @file
 * @brief Release-check scheduling and cached update metadata.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "esp_err.h"

/** @brief Start the release-check task after network setup.
 *
 * The worker waits for an operational connection and usable wall clock before
 * a TLS check. A successful start does not imply that a check has run.
 */
esp_err_t update_check_start(bool enabled, bool include_prereleases);
/** @brief Apply the selected release channel; disabling clears the cached result. */
void update_check_configure(bool enabled, bool include_prereleases);
/** @brief Wake the checker to service the editor's manual request.
 * @retval ESP_ERR_INVALID_STATE Worker absent, a check is running, or the one-minute gap has not elapsed.
 */
esp_err_t update_check_request_now(void);
/** @brief Append the cached check status to a caller-owned OTA status object. */
void update_check_add_status_json(cJSON *root);
/** @brief Copy the cached release digest only for an exact match to the selected asset URL.
 * @param url Candidate asset URL.
 * @param[out] out Buffer for 64 lowercase hex digits and a NUL; unchanged on failure.
 * @return True only when the matching asset includes a digest.
 */
bool update_check_expected_sha256(const char *url, char out[65]);
/** @brief Copy the tag, URL and digest-presence flag of the cached newer release.
 * @return False when checks are disabled or no newer release was found.
 */
bool update_check_get_available(char *tag, size_t tag_len, char *url, size_t url_len, bool *has_sha256);
/** @brief Copy only the cached newer release's tag for the status popup. */
bool update_check_available_version(char *out, size_t out_len);
