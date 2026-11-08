/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "ui/ui_widget_factory.h"
esp_err_t w_alarm_panel_create(const ui_widget_def_t *, lv_obj_t *, ui_widget_instance_t *);
void w_alarm_panel_apply_state(ui_widget_instance_t *, const ha_state_t *);
void w_alarm_panel_mark_unavailable(ui_widget_instance_t *);
void w_alarm_panel_set_visible(ui_widget_instance_t *, bool visible);
size_t w_alarm_panel_entity_ids(const ui_widget_instance_t *, const char **out, size_t max);
