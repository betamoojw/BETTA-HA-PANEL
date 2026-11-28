/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief cJSON helpers for parsing, serialization, and typed field access.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"

/** @return A caller-owned tree to release with cJSON_Delete(), or NULL on failure. */
cJSON *json_util_parse(const char *json);
/** @return Caller-owned compact JSON text from the cJSON allocator, or NULL on failure. */
char *json_util_print_unformatted(const cJSON *json);
/** @brief Borrow a case-sensitive string field until the tree is modified or freed.
 * @return False without changing the output when the field is absent or not a string.
 */
bool json_util_get_string(const cJSON *obj, const char *key, const char **out_value);
/** @brief Read the cJSON integer representation of a case-sensitive numeric field.
 * @return False without changing the output if no numeric field matches.
 */
bool json_util_get_int(const cJSON *obj, const char *key, int *out_value);
/** @brief Free a free()-compatible string buffer and clear the caller's pointer. */
void json_util_safe_free(char **ptr);

/** @brief Truncate a JSON number to an unsigned 32-bit integer without an out-of-range cast.
 * @return Zero for non-numeric, NaN, or values below one; UINT32_MAX at and above the upper limit.
 */
uint32_t json_util_number_u32(const cJSON *item);
