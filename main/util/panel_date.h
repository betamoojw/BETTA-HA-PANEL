/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Compact calendar formatting in the order and separator of the panel's date pattern.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define PANEL_DATE_ISO "Y-M-D"

/** @brief Date fields and year width to include in a label. */
typedef enum
{
    PANEL_DATE_FULL,       /**< Year with four digits, month, and day. */
    PANEL_DATE_MONTH_DAY,  /**< Month and day without a year. */
    PANEL_DATE_SHORT_YEAR, /**< Year with two digits, month, and day. */
    PANEL_DATE_MONTH_YEAR  /**< Month and two-digit year without a day. */
} panel_date_width_t;

/** @brief Publish a borrowed pattern from the immutable timezone catalog, such as `D.M.Y`.
 * @pre A non-NULL pattern must remain valid while any reader could use it.
 * @param pattern Immutable catalog pattern, or NULL for the ISO order.
 */
void panel_date_set_pattern(const char *pattern);
/** @return The current immutable pattern without copying it. */
const char *panel_date_pattern(void);
/** @brief Format calendar fields with a one-based month and a full input year.
 * @return True for a formatted value; false for out-of-range fields or insufficient space.
 */
bool panel_date_format(char *out, size_t capacity, int year, int month, int day, panel_date_width_t width);
/** @brief Format a caller-supplied local calendar time without performing timezone conversion. */
bool panel_date_from_tm(char *out, size_t capacity, const struct tm *local, panel_date_width_t width);
