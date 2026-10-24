/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "lvgl.h"
/* Called under the display lock. The active screen owns the overlay and its timer. */
void ui_screen_settings_open(void);
lv_obj_t *ui_screen_settings_root(void);
