/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
/** @file
 * @brief Structured diagnostics for validation of canonical layout JSON.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

/** @brief Caller-owned bounded list of validation errors. */
typedef struct
{
    uint16_t count;                           /**< Number of messages retained, not a count beyond the capacity. */
    char messages[APP_LAYOUT_MAX_ERRORS][96]; /**< NUL-terminated diagnostic texts. */
} layout_validation_result_t;

/** @brief Reset the result and clear its message storage. */
void layout_validation_clear(layout_validation_result_t *result);
/** @brief Copy a bounded diagnostic if there is room; additional messages are dropped. */
void layout_validation_add(layout_validation_result_t *result, const char *msg);
/** @brief Validate a canonical layout without mutating the caller's text or saving it.
 * @param json Borrowed NUL-terminated layout JSON.
 * @param[out] result Required output, cleared before validation begins.
 * @return True only when the layout passes validation.
 */
bool layout_validate_json(const char *json, layout_validation_result_t *result);
