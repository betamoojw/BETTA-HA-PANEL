/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "settings/license_manager.h"

/* Independent of the informational trial. Only a reboot resets this deadline. */
#define LICENSE_LCD_DELAY_US INT64_C(300000000)
static inline bool license_warning_due(license_key_state_t state, int64_t uptime_us)
{
    return uptime_us >= LICENSE_LCD_DELAY_US && state != LICENSE_VALID;
}
