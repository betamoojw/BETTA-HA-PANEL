/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Pure timing predicate for the informational license overlay.
 */
#pragma once
#include "settings/license_manager.h"

/** @brief Uptime deadline in microseconds, independent of the trial display. */
#define LICENSE_LCD_DELAY_US INT64_C(1800000000)

/** @return True after the deadline if the device has no valid license. */
static inline bool license_warning_due(license_key_state_t state, int64_t uptime_us)
{
    return uptime_us >= LICENSE_LCD_DELAY_US && state != LICENSE_VALID;
}
