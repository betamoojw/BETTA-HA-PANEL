/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Luma-grid motion policy state and diagnostics.
 */
#pragma once
#include "camera/camera_config.h"
/** @brief Number of luma cells along each axis of a motion sample. */
#define CAMERA_MOTION_GRID 16

/** @brief Per-sample motion measurements and trigger history. */
typedef struct
{
    bool valid,                             /**< A recent baseline exists; startup grace can still suppress evaluation. */
        active,                             /**< Motion thresholds passed, even if timing prevents a new trigger. */
        ignored_lighting;                   /**< Rejected by the whole-grid lighting test. */
    int level,                              /**< Average absolute luma change across the union of selected cells. */
        changed_pct,                        /**< Percentage of selected cells meeting the change cutoff. */
        zone_level[CAMERA_MAX_ZONES],       /**< Average absolute luma change for each configured zone. */
        zone_changed_pct[CAMERA_MAX_ZONES]; /**< Percentage of cells meeting the cutoff in each zone. */
    uint32_t trigger_count;                 /**< Emitted triggers, retained across policy resets. */
    int64_t last_trigger_ms;                /**< Monotonic time of the last trigger; meaningful when `trigger_count` is nonzero. */
} camera_motion_status_t;

/** @brief Caller-owned history for one luma-grid stream. */
typedef struct
{
    uint8_t previous[CAMERA_MOTION_GRID * CAMERA_MOTION_GRID]; /**< Copy of the previous row-major sample. */
    bool have_previous;                                        /**< The previous sample is initialized; its age is checked separately. */
    int64_t start_ms,                                          /**< Monotonic time of the last reset, which anchors startup grace. */
        active_since_ms,           /**< Monotonic time of the first active sample in this run, or -1 when inactive. */
        previous_ms;               /**< Monotonic time of the stored previous sample. */
    camera_motion_status_t status; /**< Last sample's measurements and accumulated trigger history. */
} camera_motion_state_t;

/** @brief Start the grace period and clear sample history, preserving the trigger count and last trigger time.
 * @pre Zero-initialize the state before its first reset.
 * @param[in,out] state Tracking state for this stream.
 * @param now_ms Monotonic milliseconds at reset.
 */
void camera_motion_reset(camera_motion_state_t *state, int64_t now_ms);

/** @brief Compare the new luma grid with the previous sample and update diagnostics.
 * @param[in,out] state Tracking state initialized before the first sample.
 * @param config Borrowed motion policy with validated zone bounds.
 * @param grid Borrowed row-major CAMERA_MOTION_GRID by CAMERA_MOTION_GRID luma cells.
 * @param now_ms Sample time in monotonic milliseconds, on the same clock used for reset.
 * @return True only when this sample emits a trigger; motion can be active during cooldown without a new trigger.
 */
bool camera_motion_sample(camera_motion_state_t *state, const camera_motion_config_t *config, const uint8_t *grid, int64_t now_ms);
