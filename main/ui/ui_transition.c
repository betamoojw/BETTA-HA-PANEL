/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Independently implemented for LVGL 9.6 after reviewing SmartPanel 30bcd774:
 * Content entrance and popup fades, with shorter timings for direct touch.
 */
#include "ui/ui_transition.h"

/* The simulator opts in explicitly; firmware supplies its saved preference. */
static bool s_enabled;
static lv_obj_t *s_page, *s_shield;
typedef enum { FADE_OPEN, FADE_DELETE, FADE_DELETE_ASYNC, FADE_HIDE } fade_action_t;
typedef struct fade_state {
    lv_obj_t *view, *shield;
    fade_action_t action;
    struct fade_state *next;
} fade_state_t;
static fade_state_t *s_fades;
static void finish_fade(fade_state_t *state);

static void slide(void *object, int32_t x)
{
    lv_obj_set_style_translate_x(object, x, LV_PART_MAIN);
}

static void page_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_page) {
        s_page = NULL;
        if (s_shield != NULL) lv_obj_delete(s_shield);
        s_shield = NULL;
    }
}

void ui_transition_finish(void)
{
    if (s_page == NULL) return;
    lv_obj_t *page = s_page;
    s_page = NULL;
    lv_anim_delete(page, slide);
    lv_obj_remove_event_cb(page, page_deleted);
    slide(page, 0);
    if (s_shield != NULL) lv_obj_delete(s_shield);
    s_shield = NULL;
}

static void slide_done(lv_anim_t *animation)
{
    (void)animation;
    ui_transition_finish();
}

void ui_transition_configure(bool enabled)
{
    s_enabled = enabled;
    if (!enabled) {
        ui_transition_finish();
        while (s_fades != NULL) finish_fade(s_fades);
    }
}

void ui_transition_page_enter(lv_obj_t *page)
{
    ui_transition_finish();
    if (!s_enabled || page == NULL) return;
    s_page = page;
    lv_obj_add_event_cb(page, page_deleted, LV_EVENT_DELETE, NULL);
    /* Fixed sibling in the viewport: blocks moving controls, leaves tabs usable.
     * It cannot alter the page's scroll extent or inherit its translation. */
    s_shield = lv_obj_create(lv_obj_get_parent(page));
    if (s_shield == NULL) { ui_transition_finish(); return; }
    lv_obj_remove_style_all(s_shield);
    lv_obj_set_size(s_shield, lv_pct(100), lv_pct(100));
    lv_obj_set_ignore_layout(s_shield, true);
    lv_obj_set_floating(s_shield, true);
    lv_obj_set_clickable(s_shield, true);
    lv_obj_set_scrollable(s_shield, false);
    lv_obj_set_scroll_chain(s_shield, false);
    lv_obj_set_gesture_bubble(s_shield, false);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, page);
    lv_anim_set_exec_cb(&animation, slide);
    /* Short travel and no overshoot keep direct-touch navigation responsive. */
    int32_t distance = lv_obj_get_style_width(page, 0) / 4;
    if (distance > 24) distance = 24;
    if (distance < 1) distance = 1;
    lv_anim_set_values(&animation, -distance, 0);
    lv_anim_set_duration(&animation, 120);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, slide_done);
    if (lv_anim_start(&animation) == NULL) ui_transition_finish();
}

static void opacity(void *object, int32_t value)
{
    /* Ordinary inherited opacity, not opa_layered: no full-view blend buffer. */
    lv_obj_set_style_opa(object, (lv_opa_t)value, 0);
}

static fade_state_t *find_fade(const lv_obj_t *view)
{
    for (fade_state_t *state = s_fades; state; state = state->next)
        if (state->view == view) return state;
    return NULL;
}

bool ui_transition_is_closing(const lv_obj_t *view)
{
    fade_state_t *state = find_fade(view);
    return state && state->action != FADE_OPEN;
}

static void unlink_fade(fade_state_t *state)
{
    fade_state_t **link = &s_fades;
    while (*link && *link != state) link = &(*link)->next;
    if (*link) *link = state->next;
    lv_free(state);
}

static void fade_deleted(lv_event_t *event)
{
    fade_state_t *state = find_fade(lv_event_get_target_obj(event));
    /* LVGL deletes the shield and animations with their owner. */
    if (state) unlink_fade(state);
}

static void closing_input(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if ((code >= LV_EVENT_PRESSED && code <= LV_EVENT_LEAVE) ||
        code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_event_stop_processing(event);
        lv_event_stop_bubbling(event);
    }
}

static void block_input(lv_obj_t *object, bool block)
{
    if (block) lv_obj_add_event_cb(object, closing_input, LV_EVENT_ALL | LV_EVENT_PREPROCESS, NULL);
    else lv_obj_remove_event_cb(object, closing_input);
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); i++)
        block_input(lv_obj_get_child(object, i), block);
}

static void cancel_fade(fade_state_t *state)
{
    lv_anim_delete(state->view, opacity);
    lv_obj_remove_event_cb(state->view, fade_deleted);
    if (state->action != FADE_OPEN) block_input(state->view, false);
    if (state->shield) lv_obj_delete(state->shield);
    unlink_fade(state);
}

static void apply_end(lv_obj_t *view, fade_action_t action)
{
    if (action == FADE_DELETE) lv_obj_delete(view);
    else if (action == FADE_DELETE_ASYNC) lv_obj_delete_async(view);
    else {
        lv_obj_set_hidden(view, action == FADE_HIDE);
        opacity(view, LV_OPA_COVER);
    }
}

static void finish_fade(fade_state_t *state)
{
    lv_obj_t *view = state->view;
    fade_action_t action = state->action;
    cancel_fade(state);
    apply_end(view, action);
}

static void fade_done(lv_anim_t *animation)
{
    fade_state_t *state = find_fade(animation->var);
    /* This callback is already outside the originating input handler. */
    if (state && state->action == FADE_DELETE_ASYNC) state->action = FADE_DELETE;
    if (state) finish_fade(state);
}

static void fade(lv_obj_t *view, fade_action_t action)
{
    if (!view) return;
    fade_state_t *previous = find_fade(view);
    if (previous && previous->action != FADE_OPEN && action != FADE_OPEN) return;
    int32_t start = previous ? lv_obj_get_style_opa(view, 0) :
        action == FADE_OPEN ? 0 : lv_obj_get_style_opa(view, 0);
    if (previous) cancel_fade(previous);
    lv_anim_delete(view, opacity);
    if (!s_enabled) { apply_end(view, action); return; }
    fade_state_t *state = lv_malloc_zeroed(sizeof(*state));
    if (!state) { apply_end(view, action); return; }
    state->view = view;
    state->action = action;
    state->next = s_fades;
    s_fades = state;
    lv_obj_add_event_cb(view, fade_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_hidden(view, false);
    if (action != FADE_OPEN) {
        block_input(view, true);
        /* Capture the remaining gesture and new touches without changing any
         * control's enabled state (reusable dialogs must keep those states). */
        lv_obj_update_layout(view);
        state->shield = lv_obj_create(view);
        if (!state->shield) { finish_fade(state); return; }
        lv_obj_remove_style_all(state->shield);
        lv_obj_set_ignore_layout(state->shield, true);
        lv_obj_set_floating(state->shield, true);
        lv_obj_set_pos(state->shield, -lv_obj_get_style_pad_left(view, 0), -lv_obj_get_style_pad_top(view, 0));
        lv_obj_set_size(state->shield, lv_obj_get_width(view), lv_obj_get_height(view));
        lv_obj_set_clickable(state->shield, true);
        lv_obj_set_scrollable(state->shield, false);
        lv_obj_set_scroll_chain(state->shield, false);
        lv_obj_set_gesture_bubble(state->shield, false);
    }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view);
    lv_anim_set_exec_cb(&animation, opacity);
    lv_anim_set_values(&animation, start, action == FADE_OPEN ? LV_OPA_COVER : 0);
    lv_anim_set_duration(&animation, action == FADE_OPEN ? 160 : 120);
    lv_anim_set_completed_cb(&animation, fade_done);
    if (lv_anim_start(&animation) == NULL) finish_fade(state);
}

void ui_transition_fade_in(lv_obj_t *view) { ui_transition_finish(); fade(view, FADE_OPEN); }
void ui_transition_close(lv_obj_t *view) { fade(view, FADE_DELETE); }
void ui_transition_close_async(lv_obj_t *view) { fade(view, FADE_DELETE_ASYNC); }
void ui_transition_hide(lv_obj_t *view) { fade(view, FADE_HIDE); }
