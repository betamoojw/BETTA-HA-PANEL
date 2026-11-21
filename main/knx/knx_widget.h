/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once
#include "layout/knx_binding.h"
#include "ha/ha_model.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Display-lock callers never wait for the KNX worker. The cursor is local
 * to a widget. A busy read leaves it intact for the next UI turn. */
uint32_t knx_widget_revision(void);
typedef enum { KNX_VIEW_BUSY, KNX_VIEW_UNCHANGED, KNX_VIEW_UPDATED } knx_view_result_t;
knx_view_result_t knx_widget_read(const char *key, knx_tile_kind_t kind, const char *preset,
                                      knx_control_binding_t *controls, uint32_t *cursor, ha_state_t *out);

typedef enum { KNX_SET_POWER, KNX_TOGGLE, KNX_SET_PERCENT, KNX_SET_RGB, KNX_SEND_VALUE } knx_widget_action_t;
esp_err_t knx_widget_control_roles(const knx_control_binding_t *controls, knx_widget_action_t action, int value);
esp_err_t knx_widget_control(const char *key, knx_widget_action_t action, int value, const char *preset);
#ifdef __cplusplus
}
#endif
