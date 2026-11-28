// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723

/** @file
 * @brief KNX widget capability checks, state projection and command conversion.
 */
#pragma once
#include "cJSON.h"
#include "knx_object.h"
#include "knx_widget.h"
#include <optional>

/** @brief Append a JSON value, representing 64-bit integers as strings to preserve precision. */
bool AddValue(cJSON *json, const KnxValue &value);

/** @return A scalar for paired controls, or an empty optional for unsupported or non-finite values. */
std::optional<float> KnxControlScalar(const KnxValue &value);

/** @brief Use live feedback when assigned; fall back to the last command only for an unassigned feedback role. */
std::optional<float> KnxWidgetDisplayValue(const knx_control_binding_t &binding, knx_role_t feedback_role,
                                           const KnxCommunicationObject *feedback, const KnxCommunicationObject *command);

/** @brief Check DPT and access permissions for a specific widget role. */
bool KnxWidgetRoleSupports(const KnxCommunicationObject &object, knx_role_t role);

/** @brief Check the legacy single-object binding and widget_types API compatibility. */
bool KnxWidgetSupports(const KnxCommunicationObject &object, knx_tile_kind_t kind);

/** @brief Project one object's feedback into a widget state. */
bool KnxWidgetProject(const KnxCommunicationObject &object, knx_tile_kind_t kind, ha_state_t &state);

/** @brief Project resolved roles into widget state and refresh the binding's write capabilities. */
bool KnxWidgetProjectRoles(const KnxCommunicationObject *const objects[KNX_ROLE_COUNT], knx_tile_kind_t kind, bool active,
                           knx_control_binding_t &binding, ha_state_t &state);

/** @brief Translate a widget action into text for the object's DPT. */
bool KnxWidgetCommandValue(const KnxCommunicationObject &object, knx_widget_action_t action, int value, const char *preset,
                           std::string &text);
