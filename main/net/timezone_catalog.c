/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "net/timezone_catalog.h"
#include "util/panel_date.h"
#include <string.h>
typedef struct { const char *name; const char *rules; const char *date_pattern; } timezone_entry_t;
static const timezone_entry_t s_zones[] = {
#include "tzdb/zones.inc"
};
const char *timezone_catalog_name(size_t index)
{
    return index < sizeof(s_zones) / sizeof(s_zones[0]) ? s_zones[index].name : NULL;
}
static const timezone_entry_t *lookup(const char *name)
{
    if (name == NULL) return NULL;
    size_t lo = 0, hi = sizeof(s_zones) / sizeof(s_zones[0]);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(name, s_zones[mid].name);
        if (cmp == 0) return &s_zones[mid];
        if (cmp < 0) hi = mid; else lo = mid + 1;
    }
    return NULL;
}
const char *timezone_catalog_rules(const char *name)
{
    const timezone_entry_t *entry = lookup(name);
    return entry ? entry->rules : NULL;
}
const char *timezone_catalog_date_pattern(const char *name)
{
    const timezone_entry_t *entry = lookup(name);
    return entry ? entry->date_pattern : PANEL_DATE_ISO;
}
