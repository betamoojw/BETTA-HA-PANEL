/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define AMBIENT_GRID 16
#define AMBIENT_TEST_MAX_MS 30000

int ambient_presence_threshold(int sensitivity);
int ambient_test_remaining_s(int64_t until_ms, int64_t now_ms);
bool ambient_motion_needed(bool test, bool enabled, bool dimmed, bool night, bool night_wake);
int ambient_light_percent(float light_index);
/* Reject whole-scene lighting changes; callers own the previous grid. */
int ambient_grid_motion(const uint8_t *grid, const uint8_t *previous, int *level);
