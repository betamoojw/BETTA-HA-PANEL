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

typedef enum {
    KNX_SET_POWER, KNX_TOGGLE, KNX_SET_PERCENT, KNX_SET_RGB, KNX_SEND_VALUE,
    KNX_COVER_OPEN, KNX_COVER_CLOSE, KNX_COVER_STOP,
    KNX_SET_TARGET_TEMPERATURE, /* value is tenths of a degree Celsius. */
    KNX_SET_HVAC_MODE, /* value is the DPT 20.105 enumeration, not DPT 20.102. */
    KNX_SET_FAN_PERCENT, KNX_SET_PRESET, KNX_SET_ANGLE
} knx_widget_action_t;

static inline const char *knx_hvac_mode_name(int value)
{
    switch (value) {
    case 0: return "auto";
    case 1: return "heat";
    case 3: return "cool";
    case 6: return "off";
    case 9: return "fan_only";
    case 14: return "dry";
    default: return NULL;
    }
}

static inline int knx_hvac_mode_value(const char *name)
{
    if (name) for (int value = 0; value <= 14; ++value) {
        const char *mode = knx_hvac_mode_name(value);
        if (mode && !strcmp(mode, name)) return value;
    }
    return -1;
}
esp_err_t knx_widget_control_roles(const knx_control_binding_t *controls, knx_widget_action_t action, int value);
esp_err_t knx_widget_control(const char *key, knx_widget_action_t action, int value, const char *preset);
#ifdef __cplusplus
}
#endif
