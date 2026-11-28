/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Numeric chart-axis ticks, decimal labels, and reading unit extraction.
 */
#pragma once

#include <stdbool.h>

#define CHART_SCALE_MAX_INTERVALS 8

/** @brief Rounded axis bounds and their recommended label precision. */
typedef struct
{
    float min;     /**< First tick at the lower end of the range. */
    float max;     /**< Last tick at the upper end of the range. */
    float step;    /**< Numeric spacing between ticks. */
    int intervals; /**< Number of gaps; the axis has one more tick. */
    int decimals;  /**< Recommended digits after the decimal point, from 0 through 3. */
} chart_scale_t;

/** @brief Fit an axis using a rounded multiple of a power of ten.
 *
 * The requested interval count is a target, not an exact result. A flat
 * series is expanded around its value rather than getting a zero-height axis.
 *
 * @pre Supply finite data limits with lowest no greater than highest.
 */
void chart_scale_nice(float lowest, float highest, int intervals, chart_scale_t *out);

/** @brief Format with up to three decimals, an optional unit, and no negative zero.
 * @return The supplied buffer.
 */
char *chart_scale_format(char *out, unsigned size, float value, int decimals, const char *unit);

/** @brief Copy the final space-delimited unit token, such as `W` in `506 W`.
 *
 * With a nonempty buffer, writes an empty string if no separate token
 * without digits fits both the eight-byte unit limit and the destination.
 */
void chart_scale_unit_of(const char *reading, char *out, unsigned size);
