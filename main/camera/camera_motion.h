/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "camera/camera_config.h"
#define CAMERA_MOTION_GRID 16
typedef struct
{
        bool valid, active, ignored_lighting;
        int level, changed_pct, zone_level[CAMERA_MAX_ZONES], zone_changed_pct[CAMERA_MAX_ZONES];
        uint32_t trigger_count;
        int64_t last_trigger_ms;
} camera_motion_status_t;
typedef struct
{
        uint8_t previous[CAMERA_MOTION_GRID * CAMERA_MOTION_GRID];
        bool have_previous;
        int64_t start_ms, active_since_ms, previous_ms;
        camera_motion_status_t status;
} camera_motion_state_t;
void camera_motion_reset(camera_motion_state_t *state, int64_t now_ms);
bool camera_motion_sample(camera_motion_state_t *state, const camera_motion_config_t *config, const uint8_t *grid, int64_t now_ms);
