/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Bounded flash gate shared between background writers and OTA verification.
 *
 * With flash-backed PSRAM execution, a write while `esp_ota_end()` remaps
 * flash pages for verification can reset the chip. Hold the gate across
 * either operation; if acquisition fails, do not start the operation.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @brief Acquire the gate with a semaphore wait bounded by timeout_ms.
 * @return True when acquired; on allocation failure or timeout, do not release it.
 */
bool flash_gate_enter(uint32_t timeout_ms);
/** @brief Release the gate after the flash operation and notify the S3 display path if enabled.
 * @pre This task successfully acquired the gate.
 */
void flash_gate_leave(void);
