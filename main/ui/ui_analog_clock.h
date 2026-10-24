/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "lvgl.h"
#include <time.h>

/* A scalable, allocation-free-per-frame dial. The parent owns its lifetime. */
lv_obj_t *ui_analog_clock_create(lv_obj_t *parent);
void ui_analog_clock_update(lv_obj_t *dial, const struct tm *local, bool seconds);
