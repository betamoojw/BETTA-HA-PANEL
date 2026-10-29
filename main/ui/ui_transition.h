/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "lvgl.h"
#include <stdbool.h>

/* UI task / display lock only. No snapshots or second display buffer. */
void ui_transition_configure(bool enabled);
void ui_transition_page_enter(lv_obj_t *page);
void ui_transition_finish(void);
void ui_transition_fade_in(lv_obj_t *view);
/* Closing retains the object/owner until deletion and blocks pointer input.
 * Owner teardown must still delete synchronously before freeing its context. */
void ui_transition_close(lv_obj_t *view);
/* Event handlers that continue using their context need deferred deletion even
 * with animations disabled. */
void ui_transition_close_async(lv_obj_t *view);
/* Reusable views (the voice dialog) hide instead of deleting. Fade-in reopens. */
void ui_transition_hide(lv_obj_t *view);
bool ui_transition_is_closing(const lv_obj_t *view);
