/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "settings/display_clock.h"
#include <stdio.h>

void display_clock_defaults(display_clock_t *clock)
{
    *clock = (display_clock_t){60, 5, true, 300, true, true, true, false};
}

static bool number(const cJSON *object, const char *key, int *out, int low, int high)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (value == NULL) return true;
    if (!cJSON_IsNumber(value) || !(value->valuedouble >= low && value->valuedouble <= high) ||
        value->valuedouble != value->valueint) return false;
    *out = value->valueint;
    return true;
}

static bool boolean(const cJSON *object, const char *key, bool *out)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (value == NULL) return true;
    if (!cJSON_IsBool(value)) return false;
    *out = cJSON_IsTrue(value);
    return true;
}

bool display_clock_parse(const cJSON *object, display_clock_t *clock)
{
    if (object == NULL) return true;
    if (!cJSON_IsObject(object)) return false;
    display_clock_t next = *clock;
    if (!number(object, "start_seconds", &next.start_seconds, 15, 7200) ||
        !number(object, "brightness", &next.brightness, 1, 100) ||
        !number(object, "off_seconds", &next.off_seconds, 30, 7200) ||
        !boolean(object, "off_enabled", &next.off_enabled) ||
        !boolean(object, "format_24h", &next.format_24h) ||
        !boolean(object, "show_seconds", &next.show_seconds) ||
        !boolean(object, "show_date", &next.show_date) ||
        !boolean(object, "analog", &next.analog)) return false;
    *clock = next;
    return true;
}

bool display_clock_write(cJSON *hardware, const display_clock_t *clock)
{
    cJSON *object = cJSON_AddObjectToObject(hardware, "clock");
    return object != NULL &&
        cJSON_AddNumberToObject(object, "start_seconds", clock->start_seconds) &&
        cJSON_AddNumberToObject(object, "brightness", clock->brightness) &&
        cJSON_AddNumberToObject(object, "off_seconds", clock->off_seconds) &&
        cJSON_AddBoolToObject(object, "off_enabled", clock->off_enabled) &&
        cJSON_AddBoolToObject(object, "format_24h", clock->format_24h) &&
        cJSON_AddBoolToObject(object, "show_seconds", clock->show_seconds) &&
        cJSON_AddBoolToObject(object, "show_date", clock->show_date) &&
        cJSON_AddBoolToObject(object, "analog", clock->analog);
}

bool display_clock_equal(const display_clock_t *a, const display_clock_t *b)
{
    return a->start_seconds == b->start_seconds && a->brightness == b->brightness &&
        a->off_enabled == b->off_enabled && a->off_seconds == b->off_seconds &&
        a->format_24h == b->format_24h && a->show_seconds == b->show_seconds && a->show_date == b->show_date && a->analog == b->analog;
}

bool display_clock_is_off(const display_clock_t *clock, uint32_t idle_ms)
{
    return clock->off_enabled && idle_ms >= (uint32_t)(clock->start_seconds + clock->off_seconds) * 1000U;
}

void display_clock_format(const display_clock_t *clock, const struct tm *time,
    char *text, size_t text_size, char *date, size_t date_size)
{
    const char *format = clock->format_24h ? (clock->show_seconds ? "%H:%M:%S" : "%H:%M") :
        (clock->show_seconds ? "%I:%M:%S %p" : "%I:%M %p");
    if (time != NULL && time->tm_year >= 120) {
        strftime(text, text_size, format, time);
        if (clock->show_date) strftime(date, date_size, "%Y-%m-%d", time);
        else if (date_size) date[0] = '\0';
    } else {
        snprintf(text, text_size, "--:--");
        if (date_size) date[0] = '\0';
    }
}
