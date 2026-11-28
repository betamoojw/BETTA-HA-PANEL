/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */

/** @file
 * @brief Non-blocking view and command interface for KNX widgets.
 */
#pragma once
#include "ha/ha_model.h"
#include "layout/knx_binding.h"

#ifdef __cplusplus
extern "C"
{
#endif
/** @brief Return the global view revision so the UI can detect pending refreshes without locking. */
uint32_t knx_widget_revision(void);

/** @brief Outcome of a non-blocking widget view read. */
typedef enum
{
    KNX_VIEW_BUSY,      /**< The registry lock was busy. */
    KNX_VIEW_UNCHANGED, /**< The caller already has this revision. */
    KNX_VIEW_UPDATED    /**< The caller received a new state and revision. */
} knx_view_result_t;

/**
 * @brief Read the latest widget view without waiting for the KNX worker.
 * @param key Widget binding key, including the object ID for a single-object binding.
 * @param kind Widget kind.
 * @param preset Configured value for a single-object action, if applicable.
 * @param[in,out] controls Role bindings and projected capabilities, or NULL for a single-object binding.
 * @param[in,out] cursor Per-widget revision, initially zero.
 * @param[out] out Projected state, including unavailable views.
 * @retval KNX_VIEW_BUSY The registry lock was busy; retry with the same cursor.
 * @retval KNX_VIEW_UNCHANGED The cursor already matches the view.
 * @retval KNX_VIEW_UPDATED A new view was written and the cursor advanced.
 */
knx_view_result_t knx_widget_read(const char *key, knx_tile_kind_t kind, const char *preset, knx_control_binding_t *controls,
                                  uint32_t *cursor, ha_state_t *out);

/** @brief Widget actions translated to DPT-formatted commands. */
typedef enum
{
    KNX_SET_POWER,              /**< Zero means off; nonzero means on. */
    KNX_TOGGLE,                 /**< Invert the applicable feedback or cached command. */
    KNX_SET_PERCENT,            /**< Scaling value from 0 to 100, such as brightness or position. */
    KNX_SET_RGB,                /**< Packed 0xRRGGBB value. */
    KNX_SEND_VALUE,             /**< Send the configured preset text. */
    KNX_COVER_OPEN,             /**< Use the move role or the configured position polarity. */
    KNX_COVER_CLOSE,            /**< Use the move role or the configured position polarity. */
    KNX_COVER_STOP,             /**< Use the stop role's DPT-specific value. */
    KNX_SET_TARGET_TEMPERATURE, /**< Value is tenths of a degree Celsius. */
    KNX_SET_HVAC_MODE,          /**< Value is the DPT 20.105 enumeration, not DPT 20.102. */
    KNX_SET_FAN_PERCENT,        /**< Fan speed from 0 to 100 percent. */
    KNX_SET_PRESET,             /**< DPT 20.102 mode code from 0 through 4. */
    KNX_SET_ANGLE               /**< DPT 5.003 angle from 0 to 360 degrees. */
} knx_widget_action_t;

/** @return Widget mode name for a supported DPT 20.105 code, or NULL. */
static inline const char *knx_hvac_mode_name(int value)
{
    switch (value)
    {
        case 0:
            return "auto";
        case 1:
            return "heat";
        case 3:
            return "cool";
        case 6:
            return "off";
        case 9:
            return "fan_only";
        case 14:
            return "dry";
        default:
            return NULL;
    }
}

/** @return DPT 20.105 code for a supported widget mode name, or -1. */
static inline int knx_hvac_mode_value(const char *name)
{
    if (name)
    {
        for (int value = 0; value <= 14; ++value)
        {
            const char *mode = knx_hvac_mode_name(value);
            if (mode && !strcmp(mode, name))
            {
                return value;
            }
        }
    }
    return -1;
}

/** @brief Resolve a role-based command and queue it without waiting for bus feedback. */
esp_err_t knx_widget_control_roles(const knx_control_binding_t *controls, knx_widget_action_t action, int value);

/** @brief Queue a command for a single-object binding without waiting for bus feedback. */
esp_err_t knx_widget_control(const char *key, knx_widget_action_t action, int value, const char *preset);
#ifdef __cplusplus
}
#endif
