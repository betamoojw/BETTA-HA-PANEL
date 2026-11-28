/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * The brightness rules as pure functions (no hardware, no FreeRTOS), so the display drivers, the
 * settings and the host tests (tests/host) share one implementation.
 *
 * One scale, three levels: night <= dimmed <= active, each 1..100 % (night also 0 % = display off).
 * Room light adaption (camera) scales active and dimmed with a factor of 10..100 %, never below
 * BRIGHTNESS_ROOM_FLOOR_PERCENT (or the level itself when that is lower); night is not scaled.
 */
/** @file
 * @brief Pure brightness limits and display-mode policy.
 */
#pragma once

#include <stdbool.h>

#define BRIGHTNESS_MIN_PERCENT        1
#define BRIGHTNESS_MAX_PERCENT        100
#define BRIGHTNESS_ROOM_FLOOR_PERCENT 5

/** @brief Backlight levels in percent; only night may be zero. */
typedef struct
{
    int active;
    int dimmed;
    int night;
} brightness_levels_t;

/** @brief Select the configured level before applying any room-light adjustment. */
typedef enum
{
    BRIGHTNESS_MODE_ACTIVE,
    BRIGHTNESS_MODE_DIMMED,
    BRIGHTNESS_MODE_NIGHT,
} brightness_mode_t;

/** @return Active or dimmed level clamped to 1-100 percent. */
int brightness_clamp_level(int percent);
/** @return Night level clamped to 0-100 percent; zero means backlight off. */
int brightness_clamp_night(int percent);
/** @brief Clamp each level and lower dimmed or night as needed to preserve their order. */
void brightness_levels_normalize(brightness_levels_t *levels);
/** @brief Scale a configured level by room light, rounding to whole percent.
 *
 * Clamp the factor to 0-100. The floor is the smaller of the configured level
 * and BRIGHTNESS_ROOM_FLOOR_PERCENT, so scaling never raises a low level.
 */
int brightness_scale_for_room_light(int level_percent, int room_light_percent);
/** @return Backlight percentage for the selected mode. Night bypasses ambient scaling. */
int brightness_resolve(const brightness_levels_t *levels, brightness_mode_t mode, int room_light_percent);
