// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#pragma once
#include "knx_object.h"
#include "knx_widget.h"
#include "cJSON.h"

void AddValue(cJSON *json, const KnxValue& value);
bool KnxWidgetRoleSupports(const KnxCommunicationObject& object, knx_role_t role);
bool KnxWidgetSupports(const KnxCommunicationObject& object, knx_tile_kind_t kind);
bool KnxWidgetProject(const KnxCommunicationObject& object, knx_tile_kind_t kind, ha_state_t& state);
bool KnxWidgetProjectRoles(const KnxCommunicationObject *const objects[KNX_ROLE_COUNT], knx_tile_kind_t kind,
                          bool active, knx_control_binding_t& binding, ha_state_t& state);
bool KnxWidgetCommandValue(const KnxCommunicationObject& object, knx_widget_action_t action,
                           int value, const char *preset, std::string& text);
