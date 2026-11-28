/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Settings and timing helpers for the display-protection clock.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "cJSON.h"

/** @brief Clock renderer selection; serialization uses stable names rather than these indices. */
typedef enum
{
    DISPLAY_CLOCK_DIGITAL,
    DISPLAY_CLOCK_ANALOG,
    DISPLAY_CLOCK_FLIP
} display_clock_style_t;

/** @brief Clock policy and display preferences retained even when the clock is disabled. */
typedef struct
{
    int start_seconds; /**< Inactivity in seconds before showing the clock. */
    int brightness;    /**< Backlight level while showing the clock, in percent. */
    bool off_enabled;  /**< Allow the clock to proceed to display-off. */
    int off_seconds;   /**< Additional seconds on the clock before turning off. */
    bool format_24h;
    bool show_seconds;
    bool show_date;
    display_clock_style_t style;
} display_clock_t;

/** @brief Initialize all fields to the firmware defaults. */
void display_clock_defaults(display_clock_t *clock);
/** @return Static persisted style name, falling back to digital for an invalid index. */
const char *display_clock_style_name(display_clock_style_t style);
/** @brief Merge a partial clock object, preferring an explicit style over the legacy analog flag.
 * @param object Borrowed JSON object, or NULL to keep current values.
 * @param[in,out] clock Existing settings; unchanged when validation fails.
 * @return True for an accepted object or an absent one.
 */
bool display_clock_parse(const cJSON *object, display_clock_t *clock);
/** @brief Add a clock object with both style and the legacy analog field.
 * @return False on allocation failure; a partially written object may remain.
 */
bool display_clock_write(cJSON *hardware, const display_clock_t *clock);
/** @return True when all persisted clock preferences match. */
bool display_clock_equal(const display_clock_t *a, const display_clock_t *b);
/** @return True if display-off is enabled and idle_ms has reached both configured delays. */
bool display_clock_is_off(const display_clock_t *clock, uint32_t idle_ms);
/** @brief Format wall-clock text and the optional regional date into caller-owned buffers.
 *
 * A missing or too-early clock produces "--:--" and no date.
 */
void display_clock_format(const display_clock_t *clock, const struct tm *time, char *text, size_t text_size, char *date, size_t date_size);
