/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "ha/ha_model.h"

/* Record the LCD factory boundary while running the production drain on the host. */
typedef struct {
    char entity_id[APP_MAX_ENTITY_ID_LEN], type[24];
    const char *knx_write_value;
    uint32_t knx_revision;
    char rendered[APP_MAX_STATE_LEN];
    unsigned applications;
} ui_widget_instance_t;

void ui_widget_factory_apply_state(ui_widget_instance_t *widget, const ha_state_t *state);
void ui_widget_factory_mark_unavailable(ui_widget_instance_t *widget);
