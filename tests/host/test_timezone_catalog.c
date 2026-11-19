/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "net/timezone_catalog.h"
#include "util/panel_date.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    assert(timezone_catalog_rules(NULL) == NULL);
    assert(timezone_catalog_rules("Asia/Unknown") == NULL);
    assert(timezone_catalog_rules("asia/shanghai") == NULL);
    assert(strcmp(timezone_catalog_rules("Asia/Shanghai"), "CST-8") == 0);
    assert(strcmp(timezone_catalog_rules("Asia/Kolkata"), "IST-5:30") == 0);
    assert(strstr(timezone_catalog_rules("Europe/Berlin"), "M3.5.0") != NULL);
    const struct { const char *zone, *full, *month_day, *month_year; } cases[] = {
        {"America/New_York", "11/06/2026", "11/06", "11/26"},
        {"America/Toronto", "2026-11-06", "11-06", "26-11"},
        {"Europe/London", "06/11/2026", "06/11", "11/26"},
        {"Europe/Berlin", "06.11.2026", "06.11", "11.26"},
        {"Europe/Paris", "06/11/2026", "06/11", "11/26"},
        {"Asia/Tokyo", "2026/11/06", "11/06", "26/11"},
        {"Asia/Shanghai", "2026/11/06", "11/06", "26/11"},
        {"Asia/Hong_Kong", "06/11/2026", "06/11", "11/26"},
        {"Etc/GMT+5", "2026-11-06", "11-06", "26-11"},
        {"EST5EDT,M3.2.0,M11.1.0", "2026-11-06", "11-06", "26-11"},
        {NULL, "2026-11-06", "11-06", "26-11"},
    };
    struct tm local = {.tm_year = 126, .tm_mon = 10, .tm_mday = 6};
    char buf[11];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        panel_date_set_pattern(timezone_catalog_date_pattern(cases[i].zone));
        assert(panel_date_from_tm(buf, sizeof(buf), &local, PANEL_DATE_FULL) && !strcmp(buf, cases[i].full));
        assert(panel_date_from_tm(buf, sizeof(buf), &local, PANEL_DATE_MONTH_DAY) && !strcmp(buf, cases[i].month_day));
        assert(panel_date_from_tm(buf, sizeof(buf), &local, PANEL_DATE_MONTH_YEAR) && !strcmp(buf, cases[i].month_year));
        assert(!panel_date_from_tm(buf, sizeof(buf) - 1, &local, PANEL_DATE_FULL) && !buf[0]);
    }
    size_t count = 0;
    for (const char *name; (name = timezone_catalog_name(count)); ++count) {
        assert(timezone_catalog_rules(name) != NULL);
        panel_date_set_pattern(timezone_catalog_date_pattern(name));
        assert(panel_date_from_tm(buf, sizeof(buf), &local, PANEL_DATE_FULL) && strlen(buf) == 10);
        assert(panel_date_from_tm(buf, sizeof(buf), &local, PANEL_DATE_SHORT_YEAR) && strlen(buf) == 8);
        assert(strlen(name) < 128 && strlen(timezone_catalog_rules(name)) < 128);
        if (count) assert(strcmp(timezone_catalog_name(count - 1), name) < 0);
    }
    assert(count > 400);
    assert(timezone_catalog_name((size_t)-1) == NULL);
    printf("Timezone lookup: %zu locations, bounds and recurring rules passed\n", count);
    return 0;
}
