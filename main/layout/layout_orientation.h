/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Orientation-specific editing views and canonical saved rectangles.
 */
#pragma once
#include <stdbool.h>
#include "cJSON.h"
#include "layout/widget_types.h"

#include "app_config.h"
#define LAYOUT_PORTRAIT_WIDTH  APP_SCREEN_HEIGHT        /**< Logical portrait content width in pixels. */
#define LAYOUT_PORTRAIT_HEIGHT (APP_SCREEN_WIDTH - 120) /**< Visible portrait height below the reserved bars. */
#define LAYOUT_PORTRAIT_MAX_Y  30000                    /**< Maximum portrait bottom edge, including scrolling content. */

/** @brief Project the canonical layout for editing in the selected orientation.
 * @param[in,out] root Mutable layout JSON; portrait projection preserves the original
 *                    landscape rectangles in temporary aliases.
 * @param portrait Select the portrait view rather than canonical geometry.
 * @return True on success. Failure can leave a partially changed tree; discard it.
 */
bool layout_orientation_project(cJSON *root, bool portrait);
/** @brief Restore canonical rectangles and keep portrait edits separately before saving.
 *
 * Strip the transient editor metadata. Discard the tree on failure because
 * earlier rectangles may already have been replaced.
 */
bool layout_orientation_canonicalize(cJSON *root);
/** @return True for integral coordinates within the orientation and widget size bounds. */
bool layout_orientation_rect_valid(const cJSON *widget, const cJSON *rect, bool portrait);
/** @return Size limits for the widget's look, adjusted for the portrait viewport when selected. */
widget_size_limits_t layout_orientation_widget_limits(const char *type, const char *style, bool portrait);
