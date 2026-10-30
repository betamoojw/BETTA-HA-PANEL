/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "host_adapters.h"
#include "ui/ui_transition.h"
#include "ui/ui_option_picker.h"
#include "ui/ui_pin_pad.h"
#include "ui/ui_conn_popup.h"
#include "ui/ui_history_popup.h"
#include "ui/ui_camera_view.h"
#include "ui/ui_alert_center.h"
#include "ui/ui_notification_popup.h"
#include "ui/ui_energy_timeline_popup.h"
#include "ui/ui_widget_factory.h"
#include "ui/widgets/weather_tile_internal.h"
#include "ui/fonts/app_symbols.h"

static unsigned checks, closed, chosen, submitted;
static uint32_t ticks;
static bool animated;
static const char *phase;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "Popup %s line %d: %s\n", phase, __LINE__, #c); exit(1); } } while (0)
static uint32_t now(void) { return ticks; }
static void advance(unsigned ms) { for (unsigned i = 0; i < ms; i += 20) { ticks += 20; lv_timer_handler(); } }
static lv_obj_t *find(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find(lv_obj_get_child(root, i), text);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *button(lv_obj_t *root, const char *text)
{
    lv_obj_t *label = find(root, text); CHECK(label);
    return lv_obj_get_parent(label);
}
static void click(lv_obj_t *object) { lv_obj_send_event(object, LV_EVENT_CLICKED, NULL); }
static void on_closed(void *user) { (void)user; closed++; }
static void on_chosen(void *user, size_t index) { (void)user; (void)index; chosen++; }
static void on_submit(void *user, const char *code) { (void)user; CHECK(!strcmp(code, "1")); submitted++; }
static void option_label(void *user, size_t index, char *out, size_t size)
{ (void)user; snprintf(out, size, "Choice %u", (unsigned)index); }

static lv_obj_t *opened(void)
{
    CHECK(lv_obj_get_child_count(lv_layer_top()) == 1);
    lv_obj_t *root = lv_obj_get_child(lv_layer_top(), 0);
    CHECK(lv_obj_get_style_opa(root, 0) == (animated ? 0 : 255));
    advance(40);
    int opa = lv_obj_get_style_opa(root, 0);
    CHECK(animated ? opa > 0 && opa < 255 : opa == 255);
    advance(140); CHECK(lv_obj_get_style_opa(root, 0) == 255);
    return root;
}
static void dismiss(void)
{
    lv_obj_t *root = opened();
    lv_obj_t *close = button(root, APP_SYMBOL_CLOSE);
    click(close);
    if (animated) {
        CHECK(ui_transition_is_closing(root)); advance(40);
        CHECK(lv_obj_get_child_count(lv_layer_top()) == 1);
        CHECK(lv_obj_get_style_opa(root, 0) < 255);
        click(close); /* late input is consumed, not a new close deadline */
    }
    advance(160); CHECK(lv_obj_get_child_count(lv_layer_top()) == 0);
}
static void notify(void)
{
    app_notification_t *n = calloc(1, sizeof(*n)); CHECK(n);
    strcpy(n->title, "Synthetic notification"); strcpy(n->message, "Popup lifecycle");
    ui_notification_popup_show(n);
}
static ui_widget_instance_t widget(const char *type, const char *entity, const char *value, const char *attributes)
{
    ui_widget_def_t def = {.w = 400, .h = 360};
    snprintf(def.type, sizeof(def.type), "%s", type);
    snprintf(def.entity_id, sizeof(def.entity_id), "%s", entity);
    ui_widget_instance_t instance;
    CHECK(ui_widget_factory_create(&def, lv_screen_active(), &instance) == ESP_OK);
    ha_state_t state = {0};
    snprintf(state.entity_id, sizeof(state.entity_id), "%s", entity);
    snprintf(state.state, sizeof(state.state), "%s", value);
    snprintf(state.attributes_json, sizeof(state.attributes_json), "%s", attributes);
    sim_host_state_add(&state); ui_widget_factory_apply_state(&instance, &state);
    lv_obj_update_layout(instance.obj);
    return instance;
}

static void wallbox(void)
{
    phase = "EVCC numeric target";
    ui_widget_def_t def = {.w=400, .h=360};
    strcpy(def.type, "wallbox_evcc"); strcpy(def.entity_id, "select.charge");
    strcpy(def.tertiary_entity_id, "number.target");
    ui_widget_instance_t w;
    CHECK(ui_widget_factory_create(&def, lv_screen_active(), &w) == ESP_OK);
    ha_state_t state = {0};
    strcpy(state.entity_id, "number.target"); strcpy(state.state, "80");
    strcpy(state.attributes_json, "{\"min\":20,\"max\":100,\"step\":5,\"unit_of_measurement\":\"%\"}");
    sim_host_state_add(&state); ui_widget_factory_apply_state(&w, &state);
    lv_obj_t *trigger = button(w.obj, "80 %");
    click(trigger); dismiss();
    click(trigger); lv_obj_t *root = opened(); click(button(root, APP_SYMBOL_CLOSE));
    lv_obj_delete(w.obj); advance(160); CHECK(lv_obj_get_child_count(lv_layer_top()) == 0);
}

int sim_popup_transition_regression(void)
{
    ticks = lv_tick_get(); lv_tick_set_cb(now); sim_host_set_online(true);
    for (unsigned mode = 0; mode < 2; mode++) {
        animated = mode != 0; ui_transition_configure(animated);
        phase = "Wi-Fi"; ui_conn_popup_show_wifi(); dismiss();
        phase = "HA"; ui_conn_popup_show_ha(); dismiss();
        phase = "MQTT"; ui_conn_popup_show_mqtt(); dismiss();
        if (animated) {
            ui_conn_popup_show_wifi(); lv_obj_t *old = opened();
            click(button(old, APP_SYMBOL_CLOSE)); ui_conn_popup_show_ha(); dismiss();
        }
        phase = "Alerts"; ui_alert_center_open(); dismiss();
        phase = "History"; ui_history_popup_open("sensor.synthetic", "History", "20 C", true, ""); dismiss();
        const char *ids[] = {"sensor.synthetic"};
        phase = "Daily energy"; ui_history_popup_open_days("Days", 0xffffff, ids, 1, false); dismiss();
        phase = "EVCC sessions"; ui_history_popup_open_evcc("select.synthetic"); dismiss();
        phase = "Energy timeline";
        ui_energy_timeline_popup_info_t info = {0};
        ha_energy_timeline_snapshot_t *snapshot = calloc(1, sizeof(*snapshot)); CHECK(snapshot);
        ui_energy_timeline_popup_open(&info, snapshot); free(snapshot); dismiss();
        phase = "Camera fullscreen";
        CHECK(ui_camera_view_open("camera.synthetic", "Camera", on_closed, NULL)); dismiss();
        CHECK(sim_camera_active() == 0);
        phase = "Notification"; notify(); dismiss();
        if (animated) {
            notify(); lv_obj_t *old = opened(); ui_notification_popup_dismiss();
            CHECK(ui_transition_is_closing(old));
            notify(); /* replacement while closing must remain visible */
            opened(); advance(120); CHECK(ui_notification_popup_is_active());
            ui_notification_popup_dismiss(); advance(160); CHECK(!ui_notification_popup_is_active());
        }
        phase = "Option picker";
        ui_option_picker_cfg_t cfg = {.title="Options", .count=2, .label=option_label, .chosen=on_chosen, .closed=on_closed};
        CHECK(ui_option_picker_open(&cfg)); dismiss();
        CHECK(ui_option_picker_open(&cfg)); lv_obj_t *root = opened();
        lv_obj_t *choice = button(root, "Choice 1"); unsigned before = chosen;
        click(choice);
        if (animated) click(choice);
        CHECK(chosen == before + 1); advance(160); CHECK(lv_obj_get_child_count(lv_layer_top()) == 0);
        phase = "PIN pad";
        ui_pin_pad_cfg_t pad = {.title="PIN", .closed=on_closed, .submit=on_submit};
        CHECK(ui_pin_pad_open(&pad)); dismiss();
        CHECK(ui_pin_pad_open(&pad)); root = opened();
        click(button(root, "1")); lv_obj_t *ok = button(root, "OK"); before = submitted;
        click(ok); if (animated) click(ok);
        CHECK(submitted == before + 1); advance(160); CHECK(lv_obj_get_child_count(lv_layer_top()) == 0);

        const char *types[] = {"room_tile", "solar_forecast", "datetime_tile", "light_tile", "climate_tile", "bambu_printer"};
        const char *entities[] = {"sensor.room", "sensor.solar", "input_datetime.time", "light.test", "climate.test", "sensor.printer"};
        const char *values[] = {"20", "3", "12:30:00", "on", "heat", "running"};
        const char *attrs[] = {"{}", "{}", "{\"has_time\":true,\"has_date\":false}",
            "{\"supported_color_modes\":[\"rgb\"],\"rgb_color\":[255,0,0]}",
            "{\"hvac_modes\":[\"heat\",\"off\"],\"fan_modes\":[\"auto\",\"low\"],\"swing_modes\":[\"off\",\"on\"]}", "{}"};
        for (unsigned i = 0; i < 6; i++) {
            phase = types[i]; ui_widget_instance_t w = widget(types[i], entities[i], values[i], attrs[i]);
            lv_obj_t *trigger = i == 3 || i == 4 ? lv_obj_get_child(w.obj, 5) :
                i == 5 ? button(w.obj, "Status") : w.obj;
            click(trigger); dismiss();
            if (i == 4) { /* operating, fan and swing modes have separate entry points */
                click(lv_obj_get_child(w.obj, 6)); dismiss();
                click(lv_obj_get_child(w.obj, 7)); dismiss();
            }
            click(trigger); root = opened();
            click(button(root, APP_SYMBOL_CLOSE));
            lv_obj_delete(w.obj); /* deletion during fade must retire timers and callbacks now */
            advance(160); CHECK(lv_obj_get_child_count(lv_layer_top()) == 0);
        }
        wallbox();
        phase = "Weather";
        w_weather_tile_ctx_t *weather = calloc(1, sizeof(*weather)); CHECK(weather);
        strcpy(weather->report_summary, "Synthetic weather report"); weather->render_rows[0].valid = true;
        lv_obj_t *trigger = lv_obj_create(lv_screen_active());
        lv_obj_add_event_cb(trigger, weather_report_clicked_cb, LV_EVENT_CLICKED, weather);
        click(trigger); dismiss(); CHECK(!weather->popup && !weather->popup_close_timer);
        lv_obj_remove_event_cb(trigger, weather_report_clicked_cb);
        lv_obj_add_event_cb(trigger, weather_col_clicked_cb, LV_EVENT_CLICKED, weather);
        click(trigger); dismiss(); CHECK(!weather->popup && !weather->popup_close_timer);
        click(trigger); opened(); ticks += 60000; advance(200); CHECK(!weather->popup && !weather->popup_close_timer);
        lv_obj_delete(trigger); free(weather);
        CHECK(sim_host_service_calls()->count == 0); /* only synthetic picker/PIN callbacks ran */
    }
    ui_transition_configure(false); lv_tick_set_cb(SDL_GetTicks);
    printf("Popup transitions: %u checks passed (on/off, close events, duplicate actions, timeout, replacement, owner teardown)\n", checks);
    return 0;
}
