/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define PANEL_DATE_ISO "Y-M-D"
typedef enum { PANEL_DATE_FULL, PANEL_DATE_MONTH_DAY, PANEL_DATE_SHORT_YEAR, PANEL_DATE_MONTH_YEAR } panel_date_width_t;

/* Patterns come from the immutable timezone catalog (e.g. "D.M.Y"). */
void panel_date_set_pattern(const char *pattern);
const char *panel_date_pattern(void);
bool panel_date_format(char *out, size_t capacity, int year, int month, int day, panel_date_width_t width);
bool panel_date_from_tm(char *out, size_t capacity, const struct tm *local, panel_date_width_t width);
