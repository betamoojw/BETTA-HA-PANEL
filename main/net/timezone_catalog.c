/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "net/timezone_catalog.h"
#include <string.h>
static const struct { const char *name; const char *rules; } s_zones[] = {
#include "tzdb/zones.inc"
};
const char *timezone_catalog_name(size_t index)
{
    return index < sizeof(s_zones) / sizeof(s_zones[0]) ? s_zones[index].name : NULL;
}
const char *timezone_catalog_rules(const char *name)
{
    if (name == NULL) return NULL;
    size_t lo = 0, hi = sizeof(s_zones) / sizeof(s_zones[0]);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(name, s_zones[mid].name);
        if (cmp == 0) return s_zones[mid].rules;
        if (cmp < 0) hi = mid; else lo = mid + 1;
    }
    return NULL;
}
