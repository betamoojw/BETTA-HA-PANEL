/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define UI_PERIODIC_REFRESH_MS 1000

/* Advance the deadline before trying the display lock, even if a frame is rendering. */
static inline bool ui_periodic_refresh_begin(int64_t now_ms, int64_t *next_attempt_ms)
{
    if (now_ms < *next_attempt_ms) return false;
    *next_attempt_ms = now_ms + UI_PERIODIC_REFRESH_MS;
    return true;
}
