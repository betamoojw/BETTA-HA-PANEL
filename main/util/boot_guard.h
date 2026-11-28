/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Select the built-in layout when consecutive early crashes prevent normal boot.
 *
 * The one-shot fallback leaves the stored layout intact. The ESP adapter
 * keeps the crash count in RTC memory across supported resets, not power loss.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOOT_GUARD_MAGIC       0xB007C0DEu
#define BOOT_GUARD_MAX_CRASHES 3
#define BOOT_GUARD_STABLE_S    60

/** @brief Crash counter and stability flag retained between boots. */
typedef struct
{
    uint32_t magic;
    uint32_t crashes; /**< Consecutive crashes before the stability timer elapsed. */
    uint32_t stable;  /**< The run that wrote this state reached the stability threshold. */
} boot_guard_state_t;

/** @brief Advance the guard for the boot that just started.
 * @param[in,out] state Previous state to update.
 * @param power_on True when retained RTC state cannot be trusted (power-on or brownout).
 * @param crash_reset True when the previous run ended in a panic or a watchdog reset.
 */
void boot_guard_next(boot_guard_state_t *state, bool power_on, bool crash_reset);
/** @return True when a valid crash count has reached the built-in layout threshold. */
bool boot_guard_safe(const boot_guard_state_t *state);

/** @brief Initialize the ESP guard and start the stability timer once, early in app_main. */
void boot_guard_init(void);
/** @return True only for the first call on a safe-mode boot, during initial layout loading. */
bool boot_guard_take_safe_layout(void);
/** @return Whether this boot started in safe mode, even after the initial load consumed it. */
bool boot_guard_safe_mode(void);
