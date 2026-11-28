/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Alarm panel layout validation independent of Home Assistant state.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"
/** @brief Check alarm-panel-specific settings without mutating the widget.
 *
 * A saved layout may contain entity references, but never an alarm PIN or code.
 *
 * @param widget Borrowed widget JSON.
 * @param[out] error Optional buffer for the first failure diagnostic.
 * @param size Capacity of error in bytes, including the NUL terminator.
 * @return True when the alarm-specific fields are accepted.
 */
bool alarm_panel_validate(const cJSON *widget, char *error, size_t size);
