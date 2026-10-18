/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "net/update_check_parse.h"
#include "layout/widget_types.h"
#include "ui/nav_layout.h"
#include "ui/text_scale.h"

int main(void)
{
    _Static_assert(APP_SCREEN_WIDTH == 480 && APP_SCREEN_HEIGHT == 480, "S3 display");
    _Static_assert(APP_CONTENT_BOX_HEIGHT == 360, "S3 dashboard");
    _Static_assert(APP_MAX_PAGES == 5, "S3 page policy");
    _Static_assert(APP_HA_MAX_ENTITIES == 512 && APP_HA_MAX_STATES == 512, "S3 memory policy");
    assert(strcmp(APP_NAME, "betta-ha-panel-s3") == 0);
    assert(strcmp(update_check_variant_for_project(APP_NAME), "panels3") == 0);
    assert(text_scale_build_board() == TEXT_BOARD_S3);
    assert(text_scale_floor_px(TEXT_BOARD_S3) == 16);
    assert(nav_layout_max_pages(480) == APP_MAX_PAGES);
    const char *complex[] = {"wallbox_evcc", "bambu_printer", "weather_3day", "solar_forecast",
        "media_player", "car_widget", "energy_timeline", "vacuum_tile", "todo_list", "sonos_favorites"};
    for (unsigned i = 0; i < sizeof(complex) / sizeof(complex[0]); ++i) {
        widget_size_limits_t limits = widget_type_size_limits(complex[i]);
        assert(limits.min_w <= 456 && limits.min_h <= 336);
        assert(limits.max_w <= 480 && limits.max_h <= 360);
    }
    assert(widget_type_size_limits("wallbox_evcc").min_h == 320);
    int titles[NAV_MAX_TABS];
    for (int i = 0; i < NAV_MAX_TABS; i++) titles[i] = 400;
    for (int count = 1; count <= APP_MAX_PAGES; count++) {
        for (int selected = -1; selected < count; selected++) {
            nav_layout_t layout;
            nav_layout_compute(&layout, 480, count, selected, titles);
            for (int i = 0; i < count; i++) {
                assert(layout.tab[i].x >= NAV_MARGIN);
                assert(layout.tab[i].x + layout.tab[i].w <= 480 - NAV_MARGIN);
                if (i) assert(layout.tab[i].x >= layout.tab[i-1].x + layout.tab[i-1].w + NAV_GAP);
            }
        }
    }
    puts("PASS: panels3 geometry, memory limits, OTA identity, fonts and navigation");
    return 0;
}
