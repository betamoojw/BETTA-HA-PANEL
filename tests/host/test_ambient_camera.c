/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drivers/ambient_camera_policy.h"
#include "ov5647_stats.h"

int main(void)
{
    assert(ambient_presence_threshold(5) == 8);
    assert(ambient_presence_threshold(-1) == 20);
    assert(ambient_presence_threshold(11) == 2);
    for (int i = 2; i <= 10; i++)
        assert(ambient_presence_threshold(i) < ambient_presence_threshold(i - 1));

    /* Normal wake, night gate, disabled feature and test-only sampling. */
    assert(ambient_motion_needed(false, true, true, false, false));
    assert(!ambient_motion_needed(false, true, true, true, false));
    assert(ambient_motion_needed(false, true, true, true, true));
    assert(!ambient_motion_needed(false, true, false, false, true));
    assert(!ambient_motion_needed(false, false, true, false, true));
    assert(ambient_motion_needed(true, false, false, true, false));

    /* Deadline stays on the device, including beyond the 32-bit uptime range. */
    const int64_t start = INT64_C(5000000000);
    assert(ambient_test_remaining_s(start + 30000, start) == 30);
    assert(ambient_test_remaining_s(start + 30000, start + 29999) == 1);
    assert(ambient_test_remaining_s(start + 30000, start + 30000) == 0);
    assert(ambient_test_remaining_s(start + 30000, start + 31000) == 0);
    assert(ambient_test_remaining_s(0, start) == 0);

    uint8_t prev[256], grid[256];
    memset(prev, 80, sizeof(prev));
    memcpy(grid, prev, sizeof(grid));
    int level;
    assert(ambient_grid_motion(grid, prev, &level) == 0 && level == 0);
    /* Subthreshold noise cannot wake; a local change crosses sensitivity 5. */
    memset(grid, 91, sizeof(grid));
    assert(ambient_grid_motion(grid, prev, &level) == 0);
    memcpy(grid, prev, sizeof(grid));
    memset(grid, 120, 26);
    int motion = ambient_grid_motion(grid, prev, &level);
    assert(motion >= ambient_presence_threshold(5));
    assert(motion < ambient_presence_threshold(1));
    /* Switching the room light on/off must not be mistaken for a person. */
    memset(grid, 120, sizeof(grid));
    assert(ambient_grid_motion(grid, prev, &level) == 0);
    memset(grid, 20, sizeof(grid));
    assert(ambient_grid_motion(grid, prev, &level) == 0);
    memset(grid, 120, 128);
    assert(ambient_grid_motion(grid, prev, &level) == 100);

    assert(ambient_light_percent(-10) == 10);
    assert(ambient_light_percent(0) == 100);
    assert(ambient_light_percent(-5.0f) > ambient_light_percent(-6.0f));

    /* Actual AEC/AGC register values, not the OV5647's AE target setting. */
    assert(ov5647_exposure_lines(0, 0x3d, 0x80) == 984.0f);
    assert(ov5647_exposure_lines(0xf0, 0, 8) == 0.5f);
    assert(ov5647_gain_factor(0, 0x10) == 1.0f);
    assert(ov5647_gain_factor(0xfc, 0xf8) == 15.5f);
    assert(ov5647_gain_factor(3, 0xff) == 63.9375f);
    puts("ambient camera: sensitivity, wake/night, expiry, motion, light and OV5647 readback passed");
    return 0;
}
