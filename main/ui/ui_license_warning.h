/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "lvgl.h"
#include "settings/license_warning.h"

typedef struct {
    license_key_state_t state;
    int64_t uptime_us;
    bool suppressed;
} ui_license_warning_status_t;
typedef ui_license_warning_status_t (*ui_license_warning_source_t)(void);

/* Set before display task startup, or while holding its lock. NULL hides all warnings. */
void ui_license_warning_set_source(ui_license_warning_source_t source);
/* Called in LVGL context. Paints inside the existing header, creates no LVGL object.
 * keep_clear is its first protected control (clock/Cancel), owned by the same view.
 * height caps the strip for a full-screen setup owner. Repeated attach is harmless. */
bool ui_license_warning_attach(lv_obj_t *header, lv_obj_t *keep_clear, int height);
