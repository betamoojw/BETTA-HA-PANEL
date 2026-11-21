/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "knx/knx_widget.h"
#include "ui/ui_memory.h"
#include "ui/ui_widget_factory.h"

/* Copy borrowed layout IDs into the lifetime of the control. HA contexts keep
 * their normal allocation. All callers have pointer-aligned context sizes. */
static inline void *ui_knx_control_context(size_t size, const ui_widget_def_t *def, ui_widget_instance_t *instance)
{
    const char *const *ids = def->knx.controls;
    bool bound = ids[0] || ids[1] || ids[2] || ids[3];
    char *ctx = ui_calloc_prefer_psram(1, size + (bound ? sizeof(knx_control_binding_t) : 0));
    instance->knx_controls = NULL;
    if (ctx && bound) {
        knx_control_binding_t *controls = (knx_control_binding_t *)(ctx + size);
        for (unsigned i = 0; i < KNX_CONTROL_ROLES; ++i)
            if (ids[i]) strcpy(controls->id[i], ids[i]);
        controls->brightness = -1;
        instance->knx_controls = controls;
    }
    return ctx;
}
