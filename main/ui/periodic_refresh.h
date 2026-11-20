/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once

#include <stdatomic.h>
#include "lvgl.h"

#define UI_PERIODIC_REFRESH_MS 1000

typedef struct {
    atomic_bool enabled;
    void (*run)(void);
} ui_periodic_refresh_t;

static inline void ui_periodic_refresh_tick(lv_timer_t *timer)
{
    ui_periodic_refresh_t *refresh = lv_timer_get_user_data(timer);
    if (atomic_load(&refresh->enabled)) refresh->run();
}

/* The adapter already owns the display lock while it dispatches LVGL timers. */
static inline lv_timer_t *ui_periodic_refresh_create(ui_periodic_refresh_t *refresh)
{
    return lv_timer_create(ui_periodic_refresh_tick, UI_PERIODIC_REFRESH_MS, refresh);
}
