/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "drivers/ambient_camera_policy.h"
#include <math.h>
#include <stdlib.h>

int ambient_presence_threshold(int sensitivity)
{
    static const uint8_t thresholds[] = {20, 16, 13, 10, 8, 6, 5, 4, 3, 2};
    if (sensitivity < 1) sensitivity = 1;
    if (sensitivity > 10) sensitivity = 10;
    return thresholds[sensitivity - 1];
}

int ambient_test_remaining_s(int64_t until_ms, int64_t now_ms)
{
    if (until_ms <= 0 || now_ms >= until_ms) return 0;
    return (int)((until_ms - now_ms + 999) / 1000);
}

bool ambient_motion_needed(bool test, bool enabled, bool dimmed, bool night, bool night_wake)
{
    return test || (enabled && dimmed && (!night || night_wake));
}

int ambient_light_percent(float light_index)
{
    /* JC calibration; other sensors require physical calibration. */
    float t = (light_index - (-6.3f)) / ((-4.6f) - (-6.3f));
    t = fminf(fmaxf(t, 0.0f), 1.0f);
    return 10 + (int)(t * 90.0f + 0.5f);
}

int ambient_grid_motion(const uint8_t *grid, const uint8_t *previous, int *level)
{
    int changed = 0, pos = 0, neg = 0;
    uint32_t sum = 0;
    for (int i = 0; i < AMBIENT_GRID * AMBIENT_GRID; i++) {
        int d = (int)grid[i] - (int)previous[i];
        sum += (uint32_t)abs(d);
        if (abs(d) >= 12) {
            changed++;
            if (d > 0) pos++; else neg++;
        }
    }
    *level = (int)(sum / (AMBIENT_GRID * AMBIENT_GRID));
    int percent = changed * 100 / (AMBIENT_GRID * AMBIENT_GRID);
    int same_sign = pos > neg ? pos : neg;
    if (percent >= 60 && same_sign * 100 >= changed * 90) return 0;
    return percent;
}
