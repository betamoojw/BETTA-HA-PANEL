/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Width-dependent layout editing budgets within static storage capacity.
 */
#pragma once
#include "app_config.h"
#include "ui/nav_layout.h"

/** @brief Compute the allowed page count at the active logical width in pixels.
 *
 * Both the navigation bar and static capacity constrain the editing limit.
 * Rotating to a narrower view does not discard stored pages.
 */
static inline int layout_max_pages_for_width(int width)
{
    int limit = width >= 1000 ? 12 : width >= 700 ? 8 : 5;
    const int fit = nav_layout_max_pages(width);
    if (limit > fit)
    {
        limit = fit;
    }
    return limit < APP_MAX_PAGES ? limit : APP_MAX_PAGES;
}

/** @return Total widget budget at this width, capped by static widget capacity. */
static inline int layout_max_widgets_for_width(int width)
{
    const int limit = layout_max_pages_for_width(width) * APP_MAX_WIDGETS_PER_PAGE;
    return limit < APP_MAX_WIDGETS_TOTAL ? limit : APP_MAX_WIDGETS_TOTAL;
}
