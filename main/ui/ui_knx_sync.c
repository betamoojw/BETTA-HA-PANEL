/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/ui_knx_sync.h"
#include <stdatomic.h>

static atomic_uint_least32_t completed_revision;
static uint32_t scan_revision;
static size_t next_widget;

bool ui_knx_refresh_pending(void)
{
    return knx_widget_revision() != atomic_load(&completed_revision);
}

void ui_knx_layout_changed(void)
{
    next_widget = 0;
    atomic_store(&completed_revision, 0);
}

knx_view_result_t ui_knx_apply(ui_widget_instance_t *widget, ha_state_t *scratch)
{
    knx_view_result_t result = knx_widget_read(widget->entity_id, knx_tile_kind(widget->type),
                                               widget->knx_write_value, &widget->knx_revision, scratch);
    if (result == KNX_VIEW_UPDATED) ui_widget_factory_apply_state(widget, scratch);
    else if (result == KNX_VIEW_BUSY && widget->knx_revision == 0) ui_widget_factory_mark_unavailable(widget);
    return result;
}

void ui_knx_drain(ui_widget_instance_t *widgets, size_t count, ha_state_t *scratch)
{
    if (next_widget == 0) scan_revision = knx_widget_revision();
    unsigned updated = 0;
    while (next_widget < count && updated < 4) {
        ui_widget_instance_t *w = &widgets[next_widget];
        if (knx_binding_id(w->entity_id)) {
            knx_view_result_t result = ui_knx_apply(w, scratch);
            if (result == KNX_VIEW_BUSY) return;
            if (result == KNX_VIEW_UPDATED) ++updated;
        }
        ++next_widget;
    }
    if (next_widget == count) {
        atomic_store(&completed_revision, scan_revision);
        next_widget = 0;
    }
}
