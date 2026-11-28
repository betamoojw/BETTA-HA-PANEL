/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Fixed-width diagnostic evidence passed from a deliberate abort to archive import.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/** @brief Archived cause IDs; append new causes without changing existing values. */
enum
{
    COREDUMP_CAUSE_NONE,         /**< No extra diagnostic evidence. */
    COREDUMP_CAUSE_PPA_TIMEOUT,  /**< The display's PPA operation did not complete in time. */
    COREDUMP_CAUSE_LVGL_STALL,   /**< The LVGL timer heartbeat stalled. */
    COREDUMP_CAUSE_RUNTIME_STALL /**< The UI runtime heartbeat stalled. */
};

/** @brief Captured operation parameters, not a snapshot of peripheral registers. */
typedef struct
{
    uint32_t cause;
    uint32_t width, height;
    uint32_t input_x, input_y;
    uint32_t rotation; /**< Rotation angle in degrees. */
    uint32_t input_buffer, output_buffer, output_capacity;
    uint32_t wait_ms;
} coredump_diagnostic_t;

/** @brief RTC marker with magic and checksum for validating the saved evidence. */
typedef struct
{
    uint32_t magic;
    coredump_diagnostic_t data;
    uint32_t checksum;
} coredump_diagnostic_marker_t;

/** @brief Copy evidence into the marker and seal it with a checksum. */
void coredump_diagnostic_seal(coredump_diagnostic_marker_t *marker, const coredump_diagnostic_t *data);
/** @brief Clear the marker and copy its evidence only after a panic reset.
 * @param[in,out] marker Retained marker, cleared even when invalid.
 * @param panic_reset Whether this boot followed a panic.
 * @param[out] out Evidence cleared on entry and populated on success.
 * @return True for an intact marker with a recognized cause.
 */
bool coredump_diagnostic_take(coredump_diagnostic_marker_t *marker, bool panic_reset, coredump_diagnostic_t *out);
/** @brief Save evidence to RTC memory without allocating or writing to flash. */
void coredump_diagnostics_mark(const coredump_diagnostic_t *data);
/** @brief Consume the retained device marker using the actual reset reason. */
bool coredump_diagnostics_take(coredump_diagnostic_t *out);
