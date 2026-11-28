/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared idle and total deadlines for bounded transfers.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/** @brief Transfer start and last progress with limits in monotonic milliseconds. */
typedef struct
{
    int64_t started_ms, progress_ms;
    uint32_t idle_ms, total_ms;
} transfer_deadline_t;

/** @brief Start both timers at now_ms with durations in milliseconds. */
void transfer_deadline_init(transfer_deadline_t *d, int64_t now_ms, uint32_t idle_ms, uint32_t total_ms);
/** @return True when the idle or total elapsed time has reached its limit. */
bool transfer_deadline_expired(const transfer_deadline_t *d, int64_t now_ms);
/** @brief Refresh the idle timer without extending the total deadline. */
void transfer_deadline_progress(transfer_deadline_t *d, int64_t now_ms);
