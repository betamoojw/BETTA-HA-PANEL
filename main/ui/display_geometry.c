/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
#include "ui/display_geometry.h"
#include "app_config.h"
#include <stdatomic.h>

#if defined(CONFIG_APP_PANEL_VARIANT_10INCH_1280)
#define DEFAULT_ROTATION 270
#elif defined(CONFIG_APP_PANEL_VARIANT_TAB5_1280) || defined(CONFIG_APP_PANEL_VARIANT_10INCH_JC)
#define DEFAULT_ROTATION 90
#else
#define DEFAULT_ROTATION 0
#endif
static atomic_int s_rotation = DEFAULT_ROTATION;
int display_geometry_default_rotation(void) { return DEFAULT_ROTATION; }
int display_geometry_rotation(void) { return atomic_load(&s_rotation); }
bool display_geometry_set_rotation(int degrees)
{
    if (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270) return false;
    atomic_store(&s_rotation, degrees);
    return true;
}
bool display_geometry_portrait(void)
{
    return APP_SCREEN_WIDTH != APP_SCREEN_HEIGHT &&
        (display_geometry_rotation() + 360 - DEFAULT_ROTATION) % 180 != 0;
}
int display_geometry_width(void) { return display_geometry_portrait() ? APP_SCREEN_HEIGHT : APP_SCREEN_WIDTH; }
int display_geometry_height(void) { return display_geometry_portrait() ? APP_SCREEN_WIDTH : APP_SCREEN_HEIGHT; }
