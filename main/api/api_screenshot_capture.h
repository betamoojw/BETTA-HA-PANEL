/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once

#include "lvgl.h"

#if LV_USE_SNAPSHOT
/* Caller holds the display lock. The returned complete RGB888 image belongs to
 * the caller; destroy it with lv_draw_buf_destroy after releasing the lock. */
lv_draw_buf_t *api_screenshot_capture_locked(void);
#endif
