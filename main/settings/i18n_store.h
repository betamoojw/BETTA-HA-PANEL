/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Translation catalog enumeration, parsing and per-language persistence.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "cJSON.h"

#include "app_config.h"

/** @brief Lowercase the first non-space token and check the language code and buffer limits.
 * @return True for an accepted code; discard the destination on failure.
 */
bool i18n_store_normalize_language_code(const char *input, char *out_code, size_t out_len);
/** @return True for a supported built-in code, with exact spelling. */
bool i18n_store_is_builtin_language(const char *language_code);
/** @return Caller-owned built-in tree, or NULL for unsupported language or parse/allocation failure. */
cJSON *i18n_store_parse_builtin_translation(const char *language_code);

/** @brief Read a bounded custom catalog as raw JSON text.
 * @return ESP_OK with a NUL-terminated buffer in out_json that the caller frees.
 */
esp_err_t i18n_store_load_custom_translation(const char *language_code, char **out_json);
/** @brief Atomically save caller-validated custom JSON without selecting the language. */
esp_err_t i18n_store_save_custom_translation(const char *language_code, const char *json_payload, size_t payload_len);
/** @return True when the normalized code has a regular override file, without parsing it. */
bool i18n_store_custom_translation_exists(const char *language_code);
/** @brief Copy built-in codes first, then unique custom codes until the array is full. */
esp_err_t i18n_store_list_languages(char (*out_codes)[APP_UI_LANGUAGE_MAX_LEN], size_t max_codes, size_t *out_count);
