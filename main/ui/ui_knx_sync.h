/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "ui/ui_widget_factory.h"
#include "knx/knx_widget.h"

bool ui_knx_refresh_pending(void);
/* Reset, apply and drain are called with display ownership. */
void ui_knx_layout_changed(void);
knx_view_result_t ui_knx_apply(ui_widget_instance_t *widget, ha_state_t *scratch);
void ui_knx_drain(ui_widget_instance_t *widgets, size_t count, ha_state_t *scratch);
