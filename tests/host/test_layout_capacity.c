/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "layout/layout_limits.h"
#include "layout/layout_validate.h"
#include "ui/display_geometry.h"

static cJSON *make_layout(int count)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON *pages = cJSON_AddArrayToObject(root, "pages");
    for (int p = 0, remaining = count; remaining > 0; p++) {
        char id[32];
        cJSON *page = cJSON_CreateObject();
        snprintf(id, sizeof(id), "p%d", p);
        cJSON_AddStringToObject(page, "id", id);
        cJSON_AddStringToObject(page, "title", id);
        cJSON *widgets = cJSON_AddArrayToObject(page, "widgets");
        cJSON_AddItemToArray(pages, page);
        for (int w = 0; w < APP_MAX_WIDGETS_PER_PAGE && remaining > 0; w++, remaining--) {
            cJSON *widget = cJSON_CreateObject();
            snprintf(id, sizeof(id), "w%d_%d", p, w);
            cJSON_AddStringToObject(widget, "id", id);
            cJSON_AddStringToObject(widget, "type", "empty_tile");
            cJSON *rect = cJSON_AddObjectToObject(widget, "rect");
            cJSON_AddNumberToObject(rect, "x", 0);
            cJSON_AddNumberToObject(rect, "y", 0);
            cJSON_AddNumberToObject(rect, "w", 120);
            cJSON_AddNumberToObject(rect, "h", 80);
            cJSON_AddItemToArray(widgets, widget);
        }
    }
    return root;
}

int main(void)
{
    const int widths[] = {480, 720, 800, 1280};
    const int policies[] = {5, 8, 8, 12};
    int titles[NAV_MAX_TABS];
    for (int i = 0; i < NAV_MAX_TABS; i++) titles[i] = 400;
    for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); i++) {
        const int limit = layout_max_pages_for_width(widths[i]);
        assert(limit == (policies[i] < APP_MAX_PAGES ? policies[i] : APP_MAX_PAGES));
        for (int selected = 0; selected < limit; selected++) {
            nav_layout_t nav;
            nav_layout_compute(&nav, widths[i], limit, selected, titles);
            for (int tab = 0; tab < limit; tab++) {
                assert(nav.tab[tab].w >= NAV_COMPACT_MIN_W);
                assert(nav.tab[tab].x >= NAV_MARGIN);
                assert(nav.tab[tab].x + nav.tab[tab].w <= widths[i] - NAV_MARGIN);
            }
        }
    }
    for (int angle = 0; angle < 360; angle += 90) {
        assert(display_geometry_set_rotation(angle));
        const int width = APP_VIEW_WIDTH;
        assert(layout_max_pages_for_width(width) <= APP_MAX_PAGES);
        assert(layout_max_widgets_for_width(width) <= APP_MAX_WIDGETS_TOTAL);
        assert(width == APP_SCREEN_WIDTH || width == APP_SCREEN_HEIGHT);
    }
    assert(display_geometry_set_rotation(display_geometry_default_rotation()));
    for (int extra = 0; extra <= 1; extra++) {
        cJSON *root = make_layout(APP_MAX_WIDGETS_TOTAL + extra);
        char *json = cJSON_PrintUnformatted(root);
        layout_validation_result_t result;
        const bool valid = layout_validate_json(json, &result);
        if (valid != (extra == 0)) {
            for (int i = 0; i < result.count; i++) puts(result.messages[i]);
        }
        assert(valid == (extra == 0));
        if (extra) {
            bool found = false;
            for (int i = 0; i < result.count; i++) {
                if (strstr(result.messages[i], "too many widgets in layout")) found = true;
            }
            assert(found);
        }
        free(json);
        cJSON_Delete(root);
    }
    printf("Layout capacity OK: %dx%d, %d stored pages, %d widgets\n",
           APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, APP_MAX_PAGES, APP_MAX_WIDGETS_TOTAL);
    return 0;
}
