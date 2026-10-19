/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "net/timezone_catalog.h"
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
    size_t count = 0;
    for (const char *name; (name = timezone_catalog_name(count)); ++count) {
        assert(timezone_catalog_rules(name) != NULL);
        assert(strlen(name) < 128 && strlen(timezone_catalog_rules(name)) < 128);
        if (count) assert(strcmp(timezone_catalog_name(count - 1), name) < 0);
    }
    assert(count > 400);
    assert(timezone_catalog_name((size_t)-1) == NULL);
    printf("Timezone lookup: %zu locations, bounds and recurring rules passed\n", count);
    return 0;
}
