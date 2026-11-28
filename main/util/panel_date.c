/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Render local calendar fields using the atomically selected date pattern.
 */
#include "util/panel_date.h"
#include <stdatomic.h>
#include <stdio.h>

static _Atomic(const char *) s_pattern = PANEL_DATE_ISO;

void panel_date_set_pattern(const char *pattern)
{
    atomic_store(&s_pattern, pattern ? pattern : PANEL_DATE_ISO);
}

const char *panel_date_pattern(void)
{
    return atomic_load(&s_pattern);
}

bool panel_date_format(char *out, size_t capacity, int year, int month, int day, panel_date_width_t width)
{
    if (!out || !capacity)
    {
        return false;
    }
    out[0] = '\0';
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        return false;
    }
    size_t length = width == PANEL_DATE_FULL ? 10 : width == PANEL_DATE_SHORT_YEAR ? 8 : 5;
    if (capacity <= length)
    {
        return false;
    }
    const char *pattern = panel_date_pattern();
    size_t used = 0;
    for (size_t i = 0; i < 5; i += 2)
    {
        char field = pattern[i];
        if (field == 'Y' && width == PANEL_DATE_MONTH_DAY)
        {
            continue;
        }
        if (field == 'D' && width == PANEL_DATE_MONTH_YEAR)
        {
            continue;
        }
        int number = field == 'D' ? day : field == 'M' ? month : year;
        if (field == 'Y' && width != PANEL_DATE_FULL)
        {
            number %= 100;
        }
        int digits = field == 'Y' && width == PANEL_DATE_FULL ? 4 : 2;
        if (used)
        {
            out[used++] = pattern[1];
        }
        int n = snprintf(out + used, capacity - used, "%0*d", digits, number);
        if (n < 0 || (size_t) n >= capacity - used)
        {
            out[0] = '\0';
            return false;
        }
        used += (size_t) n;
    }
    return true;
}

bool panel_date_from_tm(char *out, size_t capacity, const struct tm *local, panel_date_width_t width)
{
    return panel_date_format(out, capacity, local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, width);
}
