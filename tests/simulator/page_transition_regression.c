/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "ui/ui_transition.h"
#include "ui/ui_pages.h"
#include "ui/display_geometry.h"

static unsigned checks, shown, deleted;
static uint32_t ticks;
static unsigned clicks;
static lv_point_t point;
static bool pressed;
static unsigned visible_nav_buttons(lv_obj_t *root)
{
    if (!lv_obj_is_visible(root)) return 0;
    lv_area_t bounds;
    lv_obj_get_coords(root, &bounds);
    unsigned count = lv_obj_is_clickable(root) && bounds.y1 >= APP_VIEW_HEIGHT - 60 &&
                     lv_obj_get_height(root) < 60; /* exclude the bar itself */
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        count += visible_nav_buttons(lv_obj_get_child(root, i));
    }
    return count;
}
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "Transition line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static uint32_t now(void) { return ticks; }
static void advance(unsigned ms) { for (unsigned i = 0; i < ms; i += 20) { ticks += 20; lv_timer_handler(); } }
static void on_show(const char *id, uint16_t index) { (void)id; (void)index; shown++; }
static void on_delete(lv_event_t *event) { (void)event; deleted++; }
static void clicked(lv_event_t *event) { (void)event; clicks++; }
static void pointer_read(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input;
    data->point = point;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void tap(lv_indev_t *input, lv_obj_t *object)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    point = (lv_point_t){(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    pressed = true; lv_indev_read(input);
    pressed = false; lv_indev_read(input);
}

int sim_page_transition_regression(void)
{
    ticks = lv_tick_get();
    lv_tick_set_cb(now);
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, pointer_read);
    ui_pages_set_show_callback(on_show);
    for (int angle = 0; angle < 360; angle += 90) {
        display_geometry_set_rotation(angle);
        lv_display_set_resolution(lv_display_get_default(), APP_VIEW_WIDTH, APP_VIEW_HEIGHT);
        ui_transition_configure(true);
        ui_pages_init();
        lv_obj_update_layout(lv_screen_active());
        CHECK(visible_nav_buttons(lv_screen_active()) == 0); /* all pages hidden */
        lv_obj_t *a = ui_pages_add("home", "Home");
        lv_obj_t *b = ui_pages_add("house", "House");
        lv_obj_t *c = ui_pages_add("energy", "Energy");
        lv_obj_t *b_button = lv_button_create(b), *c_button = lv_button_create(c);
        lv_obj_set_pos(b_button, 200, 50); lv_obj_set_size(b_button, 60, 40);
        lv_obj_set_pos(c_button, 200, 50); lv_obj_set_size(c_button, 60, 40);
        lv_obj_add_event_cb(b_button, clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(c_button, clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t *viewport = lv_obj_get_parent(a);
        lv_obj_update_layout(lv_screen_active());
        uint32_t children = lv_obj_get_child_count(viewport);
        CHECK(ui_pages_show("home"));
        lv_obj_update_layout(lv_screen_active());
        CHECK(visible_nav_buttons(lv_screen_active()) == 3); /* restored pages */
        CHECK(lv_obj_get_style_translate_x(a, 0) == 0); /* no boot/rebuild entrance */
        CHECK(lv_obj_get_child_count(viewport) == children);
        CHECK(ui_pages_show_index(1));
        CHECK(lv_obj_is_hidden(a) && !lv_obj_is_hidden(b));
        CHECK(strcmp(ui_pages_current_id(), "house") == 0);
        CHECK(lv_obj_get_style_translate_x(b, 0) == -24);
        CHECK(lv_obj_get_child_count(viewport) == children + 1);
        lv_obj_t *shield = lv_obj_get_child(viewport, -1);
        CHECK(lv_obj_is_clickable(shield));
        CHECK(!lv_obj_is_scrollable(shield));
        unsigned clicks_before = clicks;
        tap(input, b_button); CHECK(clicks == clicks_before); /* actual hit testing */
        unsigned calls = shown;
        advance(40);
        int32_t intermediate = lv_obj_get_style_translate_x(b, 0);
        CHECK(intermediate > -24 && intermediate < 0);
        CHECK(ui_pages_show("house") && shown == calls); /* no duplicate refresh/worker */
        CHECK(lv_obj_get_style_translate_x(b, 0) == intermediate);
        CHECK(!ui_pages_show("missing") && !ui_pages_show_index(99));
        CHECK(ui_pages_next());
        CHECK(lv_obj_is_hidden(b) && lv_obj_get_style_translate_x(b, 0) == 0);
        CHECK(!lv_obj_is_hidden(c) && lv_obj_get_child_count(viewport) == children + 1);
        advance(140); /* Content must accept touches within the shorter entrance. */
        CHECK(lv_obj_get_style_translate_x(c, 0) == 0);
        CHECK(lv_obj_get_child_count(viewport) == children);
        tap(input, c_button); CHECK(clicks == clicks_before + 1);
        for (int i = 0; i < 100; i++) { CHECK(ui_pages_next()); advance(20); }
        ui_transition_configure(false);
        CHECK(lv_obj_get_child_count(viewport) == children);
        CHECK(ui_pages_show("house"));
        CHECK(lv_obj_get_style_translate_x(b, 0) == 0);
        CHECK(lv_obj_get_child_count(viewport) == children);
        ui_transition_configure(true);
        CHECK(ui_pages_show("home"));
        ui_pages_reset(); /* rotation/layout/theme rebuild cancels before deletion */
        advance(900);
        CHECK(ui_pages_count() == 0 && !strcmp(ui_pages_current_id(), ""));

        /* Direct parent deletion also retires the input shield and LVGL animation. */
        lv_obj_t *parent = lv_obj_create(lv_screen_active());
        lv_obj_t *page = lv_obj_create(parent);
        ui_transition_page_enter(page);
        lv_obj_delete(page);
        CHECK(lv_obj_get_child_count(parent) == 0);
        ui_transition_finish();
        lv_obj_delete(parent);

        lv_obj_t *view = lv_obj_create(lv_screen_active());
        lv_obj_add_event_cb(view, on_delete, LV_EVENT_DELETE, NULL);
        ui_transition_fade_in(view);
        CHECK(lv_obj_get_style_opa(view, 0) == 0);
        advance(40);
        CHECK(lv_obj_get_style_opa(view, 0) > 0 && lv_obj_get_style_opa(view, 0) < 255);
        advance(140);
        CHECK(lv_obj_get_style_opa(view, 0) == 255);
        unsigned before = deleted;
        ui_transition_close(view);
        advance(20); CHECK(deleted == before);
        advance(140); CHECK(deleted == before + 1);
        view = lv_obj_create(lv_screen_active());
        ui_transition_fade_in(view);
        lv_obj_delete(view); advance(900); /* interrupt opening fade */
        ui_transition_configure(false);
        view = lv_obj_create(lv_screen_active());
        lv_obj_add_event_cb(view, on_delete, LV_EVENT_DELETE, NULL);
        ui_transition_fade_in(view); CHECK(lv_obj_get_style_opa(view, 0) == 255);
        ui_transition_close(view); CHECK(deleted == before + 2);

        /* Reusable dialogs can reverse a dismissal, and disabling animations
         * must settle every active view, not just dashboard entrances. */
        ui_transition_configure(true);
        view = lv_obj_create(lv_screen_active());
        lv_obj_set_size(view, 300, 200);
        lv_obj_t *button = lv_button_create(view);
        lv_obj_set_size(button, 80, 40);
        lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, NULL);
        ui_transition_fade_in(view); advance(180);
        uint32_t count = lv_obj_get_child_count(view);
        clicks_before = clicks;
        ui_transition_hide(view);
        CHECK(ui_transition_is_closing(view) && !lv_obj_is_hidden(view));
        tap(input, button); CHECK(clicks == clicks_before);
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL); CHECK(clicks == clicks_before);
        advance(20);
        int32_t fading = lv_obj_get_style_opa(view, 0);
        ui_transition_hide(view); /* repeated close cannot extend the deadline */
        CHECK(lv_obj_get_style_opa(view, 0) == fading);
        ui_transition_fade_in(view);
        CHECK(!ui_transition_is_closing(view));
        CHECK(lv_obj_get_child_count(view) == count);
        CHECK(lv_obj_get_style_opa(view, 0) == fading); /* no flash on reversal */
        advance(180); tap(input, button); CHECK(clicks == clicks_before + 1);
        ui_transition_hide(view); advance(180);
        CHECK(lv_obj_is_hidden(view) && lv_obj_get_child_count(view) == count);
        ui_transition_fade_in(view);
        lv_obj_t *second = lv_obj_create(lv_layer_top());
        lv_obj_add_event_cb(second, on_delete, LV_EVENT_DELETE, NULL);
        ui_transition_fade_in(second); advance(20); ui_transition_close(second);
        before = deleted;
        ui_transition_configure(false);
        CHECK(deleted == before + 1);
        CHECK(!lv_obj_is_hidden(view) && lv_obj_get_style_opa(view, 0) == 255);
        ui_transition_configure(true);
        ui_transition_close(view);
        lv_obj_delete(view); advance(180); /* owner freed during fade */
        ui_transition_configure(false); /* no stale animation owner */
    }
    ui_pages_set_show_callback(NULL);
    lv_indev_delete(input);
    lv_tick_set_cb(SDL_GetTicks);
    printf("Page transitions: %u checks passed (four rotations, interruption, deletion, fades)\n", checks);
    return 0;
}
