/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "cJSON.h"

typedef struct {
    int start_seconds;
    int brightness;
    bool off_enabled;
    int off_seconds; /* time on the clock before turning off */
    bool format_24h;
    bool show_seconds;
    bool show_date;
} display_clock_t;

void display_clock_defaults(display_clock_t *clock);
/* A missing object preserves the caller's defaults/current values. Invalid input fails atomically. */
bool display_clock_parse(const cJSON *object, display_clock_t *clock);
bool display_clock_write(cJSON *hardware, const display_clock_t *clock);
bool display_clock_equal(const display_clock_t *a, const display_clock_t *b);
bool display_clock_is_off(const display_clock_t *clock, uint32_t idle_ms);
void display_clock_format(const display_clock_t *clock, const struct tm *time,
    char *text, size_t text_size, char *date, size_t date_size);
