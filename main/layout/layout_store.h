/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief LittleFS layout lifetime, ownership and fallback contracts.
 */
#pragma once

#include "cJSON.h"
#include "esp_err.h"

/** @brief Initialize storage after mounting LittleFS.
 *
 * Create the default only if the file is absent. An invalid or unreadable
 * existing file is retained rather than overwritten.
 */
esp_err_t layout_store_init(void);

/** @brief Load bounded layout text without parsing or substituting a default.
 * @param[out] json_out NUL-terminated buffer to release with free() on success.
 * @retval ESP_ERR_NOT_FOUND No layout file exists.
 * @retval ESP_ERR_INVALID_SIZE The file is empty or over the stored JSON limit.
 */
esp_err_t layout_store_load(char **json_out);

/** @brief Atomically save caller-validated, canonical layout text.
 * @pre Call outside the display lock because persistence can wait for flash.
 */
esp_err_t layout_store_save(const char *json);

/** @return Borrowed built-in layout JSON; do not modify or free it. */
const char *layout_store_default_json(void);

/** @brief Parse and normalize stored layout text, using the default if the read fails.
 * @return Caller-owned tree to release with cJSON_Delete(), or NULL if parsing or normalization fails.
 */
cJSON *layout_store_load_parsed(void);
