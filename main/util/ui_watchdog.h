/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Pure recovery policy for a stalled LVGL timer or UI runtime loop.
 *
 * A task blocked on a lock can stop the UI without starving the idle task.
 * Independent heartbeats detect that case. The ESP adapter handles recovery;
 * the policy applies a boot grace period and a consecutive restart limit.
 */
#pragma once

#include <stdint.h>

#define UI_WD_BOOT_GRACE_S  120
#define UI_WD_STALL_S       90
#define UI_WD_CLEAR_AFTER_S 600
#define UI_WD_MAX_RESTARTS  3

/** @brief Uptime-based state for detecting a sustained healthy period. */
typedef struct
{
    int64_t alive_since; /**< Uptime seconds when the healthy period began, or -1 when inactive. */
} ui_wd_state_t;

/** @brief Latest heartbeat ages and retained recovery count. */
typedef struct
{
    int64_t lvgl_age_s;    /**< Seconds since the LVGL timer last ran, or -1 before the first beat. */
    int64_t runtime_age_s; /**< Seconds since the UI runtime last ran, or -1 before the first beat. */
    uint32_t restarts_in_a_row;
} ui_wd_input_t;

/** @brief Outcome of one heartbeat evaluation. */
typedef enum
{
    UI_WD_OK = 0,          /**< No restart is due. */
    UI_WD_RESTART_LVGL,    /**< The LVGL timer stopped reporting. */
    UI_WD_RESTART_RUNTIME, /**< The UI runtime loop stopped reporting. */
    UI_WD_SUPPRESSED,      /**< Restart due, but the consecutive restart limit was reached. */
} ui_wd_decision_t;

/** @brief Initialize state before receiving heartbeats. */
void ui_wd_init(ui_wd_state_t *state);
/** @brief Evaluate one heartbeat snapshot at now_s seconds since boot. */
ui_wd_decision_t ui_wd_step(ui_wd_state_t *state, const ui_wd_input_t *in, int64_t now_s);
/** @return Uptime converted to 32-bit milliseconds plus one, remapping zero at wrap to one.
 *
 * Zero is reserved for a heartbeat that has not been recorded.
 */
uint32_t ui_wd_stamp_ms(int64_t uptime_us);
/** @return Nonzero once sustained UI health lets the caller clear the restart count. */
int ui_wd_may_clear_restarts(const ui_wd_state_t *state, int64_t now_s);
