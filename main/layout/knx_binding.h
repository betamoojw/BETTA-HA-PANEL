/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
/** @file
 * @brief KNX widget roles, runtime capability bindings and model keys.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Runtime key prefix only; saved layouts keep the unmodified object ID. */
#define KNX_BINDING_PREFIX    "knx:"
#define KNX_OBJECT_ID_MAX     48
#define KNX_COMMAND_VALUE_MAX 255

/** @return Borrowed suffix of a prefixed runtime key, or NULL for a non-KNX key. */
static inline const char *knx_binding_id(const char *key)
{
    return key && strncmp(key, KNX_BINDING_PREFIX, sizeof(KNX_BINDING_PREFIX) - 1) == 0 ? key + sizeof(KNX_BINDING_PREFIX) - 1 : NULL;
}

/** @brief Binding category that determines which KNX roles a widget accepts. */
typedef enum
{
    KNX_TILE_NONE,
    KNX_TILE_VALUE,
    KNX_TILE_NUMBER,
    KNX_TILE_BINARY,
    KNX_TILE_SWITCH,
    KNX_TILE_SLIDER,
    KNX_TILE_LIGHT,
    KNX_TILE_ACTION,
    KNX_TILE_COVER,
    KNX_TILE_CLIMATE,
    KNX_TILE_FAN,
    KNX_TILE_SELECT
} knx_tile_kind_t;

/** @return KNX binding category for a widget type, or KNX_TILE_NONE if unsupported. */
static inline knx_tile_kind_t knx_tile_kind(const char *type)
{
    if (!type)
    {
        return KNX_TILE_NONE;
    }
    if (!strcmp(type, "sensor"))
    {
        return KNX_TILE_VALUE;
    }
    if (!strcmp(type, "gauge") || !strcmp(type, "graph"))
    {
        return KNX_TILE_NUMBER;
    }
    if (!strcmp(type, "binary_tile") || !strcmp(type, "contact_tile"))
    {
        return KNX_TILE_BINARY;
    }
    if (!strcmp(type, "button"))
    {
        return KNX_TILE_SWITCH;
    }
    if (!strcmp(type, "slider"))
    {
        return KNX_TILE_SLIDER;
    }
    if (!strcmp(type, "light_tile"))
    {
        return KNX_TILE_LIGHT;
    }
    if (!strcmp(type, "scene_tile"))
    {
        return KNX_TILE_ACTION;
    }
    if (!strcmp(type, "cover_tile"))
    {
        return KNX_TILE_COVER;
    }
    if (!strcmp(type, "climate_tile"))
    {
        return KNX_TILE_CLIMATE;
    }
    if (!strcmp(type, "fan_tile"))
    {
        return KNX_TILE_FAN;
    }
    if (!strcmp(type, "select_tile"))
    {
        return KNX_TILE_SELECT;
    }
    return KNX_TILE_NONE;
}

/** @brief Internal role indices; layouts persist the corresponding stable names. */
typedef enum
{
    KNX_SWITCH_COMMAND,
    KNX_SWITCH_FEEDBACK,
    KNX_BRIGHTNESS_COMMAND,
    KNX_BRIGHTNESS_FEEDBACK,
    KNX_VALUE_FEEDBACK,
    KNX_NUMBER_FEEDBACK,
    KNX_FIXED_ACTION,
    KNX_MOVE_COMMAND,
    KNX_STOP_COMMAND,
    KNX_POSITION_COMMAND,
    KNX_POSITION_FEEDBACK,
    KNX_TARGET_TEMPERATURE_COMMAND,
    KNX_TARGET_TEMPERATURE_FEEDBACK,
    KNX_CURRENT_TEMPERATURE_FEEDBACK,
    KNX_HVAC_MODE_COMMAND,
    KNX_HVAC_MODE_FEEDBACK,
    KNX_FAN_SPEED_COMMAND,
    KNX_FAN_SPEED_FEEDBACK,
    KNX_PRESET_COMMAND,
    KNX_PRESET_FEEDBACK,
    KNX_ANGLE_COMMAND,
    KNX_ANGLE_FEEDBACK,
    KNX_ROLE_COUNT
} knx_role_t;

#define KNX_CONTROL_ROLES KNX_ROLE_COUNT

/** @return Borrowed persisted role name, or NULL for an invalid index. */
static inline const char *knx_role_name(knx_role_t role)
{
    static const char *const names[] = {"switch_command",
                                        "switch_feedback",
                                        "brightness_command",
                                        "brightness_feedback",
                                        "value_feedback",
                                        "number_feedback",
                                        "action",
                                        "move_command",
                                        "stop_command",
                                        "position_command",
                                        "position_feedback",
                                        "target_temperature_command",
                                        "target_temperature_feedback",
                                        "current_temperature_feedback",
                                        "hvac_mode_command",
                                        "hvac_mode_feedback",
                                        "fan_speed_command",
                                        "fan_speed_feedback",
                                        "preset_command",
                                        "preset_feedback",
                                        "angle_command",
                                        "angle_feedback"};
    return (unsigned) role < KNX_ROLE_COUNT ? names[role] : NULL;
}

/** @return True for roles that send a command, rather than read status. */
static inline bool knx_role_is_command(knx_role_t role)
{
    return role == KNX_SWITCH_COMMAND || role == KNX_BRIGHTNESS_COMMAND || role == KNX_FIXED_ACTION || role == KNX_MOVE_COMMAND ||
           role == KNX_STOP_COMMAND || role == KNX_POSITION_COMMAND || role == KNX_TARGET_TEMPERATURE_COMMAND ||
           role == KNX_HVAC_MODE_COMMAND || role == KNX_FAN_SPEED_COMMAND || role == KNX_PRESET_COMMAND || role == KNX_ANGLE_COMMAND;
}

/** @return Feedback role for a command with the same physical meaning, or KNX_ROLE_COUNT. */
static inline knx_role_t knx_command_feedback(knx_role_t role)
{
    switch (role)
    {
        case KNX_SWITCH_COMMAND:
        case KNX_BRIGHTNESS_COMMAND:
        case KNX_POSITION_COMMAND:
        case KNX_TARGET_TEMPERATURE_COMMAND:
        case KNX_HVAC_MODE_COMMAND:
        case KNX_FAN_SPEED_COMMAND:
        case KNX_PRESET_COMMAND:
        case KNX_ANGLE_COMMAND:
            return (knx_role_t) (role + 1);
        default:
            return KNX_ROLE_COUNT;
    }
}

/** @return True when the runtime keeps a separate control-binding record for this widget. */
static inline bool knx_tile_has_controls(knx_tile_kind_t kind)
{
    return kind == KNX_TILE_LIGHT || kind == KNX_TILE_SWITCH || kind == KNX_TILE_SLIDER || kind == KNX_TILE_COVER ||
           kind == KNX_TILE_CLIMATE || kind == KNX_TILE_FAN || kind == KNX_TILE_SELECT;
}

/** @return True if the widget accepts this role in its saved binding. */
static inline bool knx_tile_has_role(knx_tile_kind_t kind, knx_role_t role)
{
    switch (kind)
    {
        case KNX_TILE_LIGHT:
            return (unsigned) role <= KNX_BRIGHTNESS_FEEDBACK;
        case KNX_TILE_SWITCH:
            return role == KNX_SWITCH_COMMAND || role == KNX_SWITCH_FEEDBACK;
        case KNX_TILE_SLIDER:
            return role == KNX_BRIGHTNESS_COMMAND || role == KNX_BRIGHTNESS_FEEDBACK || role == KNX_ANGLE_COMMAND ||
                   role == KNX_ANGLE_FEEDBACK;
        case KNX_TILE_SELECT:
            return role == KNX_PRESET_COMMAND || role == KNX_PRESET_FEEDBACK;
        case KNX_TILE_BINARY:
            return role == KNX_SWITCH_FEEDBACK;
        case KNX_TILE_NUMBER:
            return role == KNX_NUMBER_FEEDBACK;
        case KNX_TILE_VALUE:
            return role == KNX_VALUE_FEEDBACK;
        case KNX_TILE_ACTION:
            return role == KNX_FIXED_ACTION;
        case KNX_TILE_COVER:
            return role >= KNX_MOVE_COMMAND && role <= KNX_POSITION_FEEDBACK;
        case KNX_TILE_CLIMATE:
            return role >= KNX_TARGET_TEMPERATURE_COMMAND && role <= KNX_HVAC_MODE_FEEDBACK;
        case KNX_TILE_FAN:
            return role == KNX_SWITCH_COMMAND || role == KNX_SWITCH_FEEDBACK || role == KNX_FAN_SPEED_COMMAND ||
                   role == KNX_FAN_SPEED_FEEDBACK;
        default:
            return false;
    }
}

/** @brief Interactive widget binding and latest runtime capabilities.
 *
 * The UI updates the capability flags and confirmed brightness under the
 * display lock, together with the projected state.
 */
typedef struct
{
    char id[KNX_CONTROL_ROLES][KNX_OBJECT_ID_MAX + 1]; /**< Owned role IDs; an empty entry is unassigned. */
    bool switch_writable, brightness_writable;
    int brightness;            /**< Displayed percentage, or -1 when unknown. */
    uint32_t writable_roles;   /**< Mask of roles writable in the current routing state and registry. */
    bool position_open_at_100; /**< True for open percentage; false for the usual KNX closed percentage. */
} knx_control_binding_t;

/** @return True when an angle-role ID is configured instead of a brightness-role binding. */
static inline bool knx_angle_slider(const knx_control_binding_t *binding)
{
    return binding && (binding->id[KNX_ANGLE_COMMAND][0] || binding->id[KNX_ANGLE_FEEDBACK][0]);
}

/** @return True when the runtime capability mask allows a write for this role. */
static inline bool knx_control_can_write(const knx_control_binding_t *binding, knx_role_t role)
{
    return binding && (unsigned) role < KNX_ROLE_COUNT && (binding->writable_roles & (UINT32_C(1) << role));
}
