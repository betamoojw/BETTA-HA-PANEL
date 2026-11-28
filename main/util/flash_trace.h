/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Time flash writes so device logs can be correlated with display flashes.
 *
 * A flash write can disable the cache while the DSI refresh interrupt is
 * not cache-safe. The host macros emit no log.
 */
#pragma once

#ifdef ESP_PLATFORM
#  include <stddef.h>
#  include <stdint.h>

#  include "esp_log.h"
#  include "esp_timer.h"

/** @brief Declare a local start timestamp in microseconds for FLASH_TRACE_END. */
#  define FLASH_TRACE_BEGIN(t) const int64_t t = esp_timer_get_time()
/** @brief Log the operation name, byte count, and elapsed time in milliseconds. */
#  define FLASH_TRACE_END(t, what, bytes) \
      ESP_LOGI("flash_trace", "flash write: %s %u B, %d ms", (what), (unsigned) (bytes), (int) ((esp_timer_get_time() - (t)) / 1000))
#else
#  define FLASH_TRACE_BEGIN(t)            ((void) 0)
#  define FLASH_TRACE_END(t, what, bytes) ((void) (bytes))
#endif
