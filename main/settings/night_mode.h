/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Night mode: between a start and an end time of day the backlight drops to
 * the night brightness (0 = off). A touch wakes the display for the normal
 * dim timeout, then it returns to the night level. Windows may cross midnight (22:00 - 06:00).
 */
/** @file
 * @brief Live night schedule and its backlight level.
 */
#pragma once

#include <stdbool.h>

/** @brief Reconfigure the nightly window; the end is exclusive and may cross midnight.
 * @param enabled Whether the schedule is enabled.
 * @param start_minutes Local minutes since midnight at the start.
 * @param end_minutes Local minutes since midnight at the end; equal times mean inactive.
 * @param brightness_percent Night backlight percentage, clamped to 0-100.
 */
void night_mode_configure(bool enabled, int start_minutes, int end_minutes, int brightness_percent);

/** @return Configured night backlight percentage; zero means off. */
int night_mode_brightness_percent(void);

/** @return True when the local clock is usable and falls inside the enabled night window. */
bool night_mode_active(void);
