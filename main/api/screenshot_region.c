/* SPDX-License-Identifier: MIT
 * Adapted from LVGL src/draw/snapshot/lv_snapshot.c by andre68723, 2026.
 * Region rendering borrows caller-owned memory without reshaping objects or buffers.
 *
 * MIT licence
 * Copyright (c) 2025 LVGL Kft
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "api/screenshot_region.h"
#if LV_USE_SNAPSHOT
#include "src/draw/lv_draw_private.h"
#include "src/core/lv_obj_draw_private.h"
#include "src/core/lv_refr_private.h"
#include "src/display/lv_display_private.h"
#include "src/core/lv_obj_private.h"
#define LV_SNAPSHOT_MAX_STALLED_DISPATCHES 512U
static uint32_t snapshot_pending_task_count(const lv_layer_t *layer);
static void snapshot_discard_layer_tasks(lv_layer_t *layer);
bool screenshot_render_region(lv_obj_t *obj, lv_color_format_t cf, const lv_area_t *area,
                              uint8_t *pixels, size_t capacity)
{
    if (!obj || !area || !pixels ||
        (cf != LV_COLOR_FORMAT_RGB888 && cf != LV_COLOR_FORMAT_ARGB8888)) return false;
    const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
    const uint32_t bpp = cf == LV_COLOR_FORMAT_RGB888 ? 3U : 4U;
    if (w <= 0 || h <= 0 || (uint64_t)w * h * bpp > capacity || capacity > UINT32_MAX) return false;
    lv_draw_buf_t buffer;
    if (lv_draw_buf_init(&buffer, w, h, cf, (uint32_t)w * bpp, pixels, (uint32_t)capacity) != LV_RESULT_OK)
        return false;
    lv_draw_buf_t *draw_buf = &buffer;
    const lv_area_t snapshot_area = *area;
    lv_obj_t * top_obj = lv_refr_get_top_obj(&snapshot_area, obj);
    if(top_obj == NULL) {
        /* Clear draw buffer when no top object*/
        lv_draw_buf_clear(draw_buf, NULL);
        top_obj = obj;
    }

    lv_layer_t layer;
    lv_layer_init(&layer);

    layer.draw_buf = draw_buf;
    layer.display = lv_obj_get_display(obj);
    layer.buf_area.x1 = snapshot_area.x1;
    layer.buf_area.y1 = snapshot_area.y1;
    layer.buf_area.x2 = snapshot_area.x1 + w - 1;
    layer.buf_area.y2 = snapshot_area.y1 + h - 1;
    layer.color_format = cf;
    layer._clip_area = snapshot_area;
    layer.phy_clip_area = snapshot_area;

    lv_draw_unit_send_event(NULL, LV_EVENT_CHILD_CREATED, &layer);

    lv_display_t * disp_old = lv_refr_get_disp_refreshing();
    lv_display_t * disp_new = lv_obj_get_display(obj);
    lv_layer_t * layer_old = disp_new->layer_head;
    disp_new->layer_head = &layer;

    lv_refr_set_disp_refreshing(disp_new);

    if(top_obj == obj) {
        lv_obj_redraw(&layer, top_obj);
    }
    else {
        lv_obj_refr(&layer, top_obj);

        lv_obj_t * parent = lv_obj_get_parent(top_obj);
        lv_obj_t * border_p = top_obj;

        /*Do until not reach the screen*/
        while(parent != NULL && border_p != obj) {
            bool go = false;
            uint32_t i;
            uint32_t child_cnt = lv_obj_get_child_count(parent);
            for(i = 0; i < child_cnt; i++) {
                lv_obj_t * child = parent->spec_attr->children[i];
                if(!go) {
                    if(child == border_p) go = true;
                }
                else {
                    /*Refresh the objects*/
                    lv_obj_refr(&layer, child);
                }
            }

            /*Call the post draw draw function of the parents of the to object*/
            lv_obj_send_event(parent, LV_EVENT_DRAW_POST_BEGIN, (void *)&layer);
            lv_obj_send_event(parent, LV_EVENT_DRAW_POST, (void *)&layer);
            lv_obj_send_event(parent, LV_EVENT_DRAW_POST_END, (void *)&layer);

            /*The new border will be the last parents,
            *so the 'younger' brothers of parent will be refreshed*/
            border_p = parent;
            /*Go a level deeper*/
            parent = lv_obj_get_parent(parent);
        }
    }

    layer.all_tasks_added = true;

    uint32_t stalled_dispatches = 0;
    uint32_t last_task_count = snapshot_pending_task_count(&layer);

    while(layer.draw_task_head) {
#if LV_USE_OS
        /*Without an OS this busy-spins on `_draw_info.dispatch_req`, so the
         *stall counter below would never be reached.*/
        lv_draw_dispatch_wait_for_request();
#endif
        lv_draw_dispatch();

        uint32_t task_count = snapshot_pending_task_count(&layer);
        if(task_count != last_task_count) {
            last_task_count = task_count;
            stalled_dispatches = 0;
        }
        else if(++stalled_dispatches > LV_SNAPSHOT_MAX_STALLED_DISPATCHES) {
            LV_LOG_WARN("Snapshot draw queue stalled, aborting");
            lv_draw_wait_for_finish();
            snapshot_discard_layer_tasks(&layer);
            disp_new->layer_head = layer_old;
            lv_refr_set_disp_refreshing(disp_old);

            /*Balance the CHILD_CREATED sent for the top layer above; sub-layers
             *were already balanced by lv_draw_cleanup_task().*/
            lv_draw_unit_send_event(NULL, LV_EVENT_CHILD_DELETED, &layer);
            return false;
        }
    }

    disp_new->layer_head = layer_old;
    lv_refr_set_disp_refreshing(disp_old);

    lv_draw_unit_send_event(NULL, LV_EVENT_SCREEN_LOAD_START, &layer);
    lv_draw_unit_send_event(NULL, LV_EVENT_CHILD_DELETED, &layer);

    return true;
}

static uint32_t snapshot_pending_task_count(const lv_layer_t * layer)
{
    uint32_t count = 0;

    while(layer) {
        const lv_draw_task_t * task = layer->draw_task_head;
        while(task) {
            count++;
            task = task->next;
        }
        layer = layer->next;
    }

    return count;
}

/**
 * Release every draw task a stalled snapshot left queued, freeing the sub-layers
 * they created along the way.
 *
 * `lv_draw_dispatch_layer()` only cleans up tasks that reached
 * `LV_DRAW_TASK_STATE_READY`, and a sub-layer is freed only when its parent's
 * LAYER task is cleaned up, so abandoning the redraw would otherwise leak both
 * the queued tasks and every sub-layer allocated for them - on the one path that
 * runs when memory is already short.  `lv_draw_cleanup_task()` frees a LAYER
 * task's sub-layer but not the tasks still queued inside it, so those are
 * released first by recursing into the sub-layer.
 * @param  layer   layer whose queued tasks are released.  Its own draw buffer
 *                 is left untouched: for the snapshot's top layer it belongs to
 *                 the caller, and a sub-layer's buffer is freed by the parent
 *                 LAYER task that owns it.
 */
static void snapshot_discard_layer_tasks(lv_layer_t * layer)
{
    lv_draw_task_t * task = layer->draw_task_head;

    while(task) {
        lv_draw_task_t * task_next = task->next;

        if(task->type == LV_DRAW_TASK_TYPE_LAYER) {
            lv_draw_image_dsc_t * image_dsc = task->draw_dsc;
            lv_layer_t * sub_layer = (lv_layer_t *)image_dsc->src;
            if(sub_layer) snapshot_discard_layer_tasks(sub_layer);
        }

        lv_draw_cleanup_task(task);

        task = task_next;
    }

    layer->draw_task_head = NULL;
}


#endif
