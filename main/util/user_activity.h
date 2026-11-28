/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared idle clock used to defer work such as automatic firmware updates.
 *
 * Callers record touch and editor/API activity, not periodic version polling.
 * Timestamps are uptime in seconds.
 */
#pragma once

#include <stdint.h>

/** @brief Record an interaction at now_s seconds since boot, callable from any task. */
void user_activity_note(uint32_t now_s);

/** @return Idle seconds, or total uptime if no interaction has been recorded. */
int64_t user_activity_idle_s(uint32_t now_s);
