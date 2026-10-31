/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <time.h>
#include "lvgl.h"

/* Empty-dashboard view. UI task/display lock only; never modifies the layout. */
lv_obj_t *ui_flip_clock_create(lv_obj_t *parent, bool format_24h);
void ui_flip_clock_set_format(lv_obj_t *clock, bool format_24h);
/* Also used by deterministic simulator checks. NULL time means not synchronized. */
void ui_flip_clock_update(lv_obj_t *clock, const struct tm *time, bool animate);
