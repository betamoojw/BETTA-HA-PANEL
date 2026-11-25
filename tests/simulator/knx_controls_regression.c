/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "host_adapters.h"
#include "ui/ui_widget_factory.h"
#include "ui/ui_i18n.h"
#include "ui/fonts/app_symbols.h"
#include "ui/theme/ui_style.h"
#include "popup_contract_regression.h"
#include "src/libs/lodepng/lodepng.h"

esp_err_t w_cover_create(const ui_widget_def_t *, lv_obj_t *, ui_widget_instance_t *);
void w_cover_apply_state(ui_widget_instance_t *, const ha_state_t *);
esp_err_t w_climate_create(const ui_widget_def_t *, lv_obj_t *, ui_widget_instance_t *);
void w_climate_apply_state(ui_widget_instance_t *, const ha_state_t *);
esp_err_t w_fan_create(const ui_widget_def_t *, lv_obj_t *, ui_widget_instance_t *);
void w_fan_apply_state(ui_widget_instance_t *, const ha_state_t *);

static unsigned checks;
static const char *phase;
static void (*wait_ms)(uint32_t);
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "KNX controls (%s) line %d: %s\n", phase, __LINE__, #c); exit(1); } } while (0)
#define WRITABLE(role) (UINT32_C(1) << (role))

static lv_obj_t *find_label(lv_obj_t *root, const char *text, bool button_only)
{
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text) &&
        (!button_only || lv_obj_check_type(lv_obj_get_parent(root), &lv_button_class))) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, i), text, button_only);
        if (found) return found;
    }
    return NULL;
}

static lv_obj_t *button(lv_obj_t *root, const char *text)
{
    lv_obj_t *found = find_label(root, text, true);
    if (!found) fprintf(stderr, "Missing button: %s\n", text);
    CHECK(found != NULL);
    return lv_obj_get_parent(found);
}

static lv_obj_t *bar(lv_obj_t *root)
{
    if (lv_obj_check_type(root, &lv_bar_class)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = bar(lv_obj_get_child(root, i));
        if (found) return found;
    }
    return NULL;
}

static void click(lv_obj_t *root, const char *text)
{
    /* Deliver even to disabled controls: callbacks must also reject stale events. */
    lv_obj_send_event(button(root, text), LV_EVENT_CLICKED, NULL);
    wait_ms(100);
    sim_popup_contract_check();
}

static void disabled(lv_obj_t *root, const char *text, bool expected)
{
    const bool actual = lv_obj_has_state(button(root, text), LV_STATE_DISABLED);
    if (actual != expected) fprintf(stderr, "Button %s disabled=%d, expected=%d\n", text, actual, expected);
    CHECK(actual == expected);
}

static void apply(ui_widget_instance_t *widget, void (*fn)(ui_widget_instance_t *, const ha_state_t *),
                  const char *id, const char *value, const char *attributes)
{
    ha_state_t state = {0};
    snprintf(state.entity_id, sizeof(state.entity_id), "%s", id);
    snprintf(state.state, sizeof(state.state), "%s", value);
    snprintf(state.attributes_json, sizeof(state.attributes_json), "%s", attributes);
    fn(widget, &state);
    lv_obj_update_layout(widget->obj);
}

static void command(unsigned before, const char *id, knx_widget_action_t action, int value)
{
    CHECK(sim_knx_writes() == before + 1);
    CHECK(sim_knx_last_write(id, action, value));
}

static void temperature(lv_obj_t *root, float value, bool expected)
{
    char text[32];
    ui_format_temp_c(text, sizeof(text), true, value);
    CHECK((find_label(root, text, false) != NULL) == expected);
}

static bool text_contains(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (text_contains(lv_obj_get_child(root, i), text)) return true;
    return false;
}

static bool same_color(lv_color_t a, lv_color_t b)
{
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

static void capture_if_requested(lv_obj_t *widget)
{
    const char *path = getenv("BETTA_KNX_FAN_CAPTURE");
    if (!path || !*path) return;
    lv_draw_buf_t *snapshot = lv_snapshot_take(widget, LV_COLOR_FORMAT_ARGB8888);
    CHECK(snapshot != NULL);
    const unsigned width = snapshot->header.w, height = snapshot->header.h;
    unsigned char *rgba = malloc((size_t)width * height * 4);
    CHECK(rgba != NULL);
    for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
        const unsigned char *pixel = snapshot->data + y * snapshot->header.stride + x * 4;
        unsigned char *out = rgba + ((size_t)y * width + x) * 4;
        out[0] = pixel[2]; out[1] = pixel[1]; out[2] = pixel[0]; out[3] = pixel[3];
    }
    unsigned char *png = NULL;
    size_t size = 0;
    CHECK(lodepng_encode32(&png, &size, rgba, width, height) == 0);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(png, 1, size, file) == size);
    CHECK(fclose(file) == 0);
    lv_free(png);
    free(rgba);
    lv_draw_buf_destroy(snapshot);
}

static void cover_test(void)
{
    phase = "cover without feedback";
    ui_widget_def_t def = {.w = 420, .h = 120};
    strcpy(def.type, "cover_tile"); strcpy(def.entity_id, "knx:cover_feedback");
    def.knx.controls[KNX_MOVE_COMMAND] = "cover_move";
    def.knx.controls[KNX_STOP_COMMAND] = "cover_stop";
    def.knx.controls[KNX_POSITION_FEEDBACK] = "cover_feedback";
    ui_widget_instance_t widget = {0};
    CHECK(w_cover_create(&def, lv_screen_active(), &widget) == ESP_OK);
    CHECK(widget.knx_controls != NULL);
    widget.knx_controls->writable_roles = WRITABLE(KNX_MOVE_COMMAND) | WRITABLE(KNX_STOP_COMMAND);
    apply(&widget, w_cover_apply_state, def.entity_id, "unknown", "{\"supported_features\":11}");
    CHECK(find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));
    disabled(widget.obj, APP_SYMBOL_ARROW_UP, false);
    disabled(widget.obj, APP_SYMBOL_STOP, false);
    disabled(widget.obj, APP_SYMBOL_ARROW_DOWN, false);
    unsigned before = sim_knx_writes();
    click(widget.obj, APP_SYMBOL_ARROW_UP); command(before++, "cover_move", KNX_COVER_OPEN, 0);
    click(widget.obj, APP_SYMBOL_STOP); command(before++, "cover_stop", KNX_COVER_STOP, 0);
    click(widget.obj, APP_SYMBOL_ARROW_DOWN); command(before++, "cover_move", KNX_COVER_CLOSE, 0);
    CHECK(find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));

    phase = "cover confirmed state and permissions";
    apply(&widget, w_cover_apply_state, def.entity_id, "open", "{\"current_position\":37,\"supported_features\":11}");
    CHECK(bar(widget.obj) && lv_bar_get_value(bar(widget.obj)) == 37);
    click(widget.obj, APP_SYMBOL_ARROW_DOWN); command(before++, "cover_move", KNX_COVER_CLOSE, 0);
    CHECK(lv_bar_get_value(bar(widget.obj)) == 37);
    widget.knx_controls->writable_roles = 0;
    apply(&widget, w_cover_apply_state, def.entity_id, "open", "{\"current_position\":37,\"supported_features\":0}");
    disabled(widget.obj, APP_SYMBOL_ARROW_UP, true);
    disabled(widget.obj, APP_SYMBOL_ARROW_DOWN, true);
    click(widget.obj, APP_SYMBOL_ARROW_UP); click(widget.obj, APP_SYMBOL_STOP);
    CHECK(sim_knx_writes() == before);
    apply(&widget, w_cover_apply_state, def.entity_id, "unavailable", "{}");
    CHECK(find_label(widget.obj, ui_i18n_get("common.unavailable", "unavailable"), false));
    CHECK(lv_bar_get_value(bar(widget.obj)) == 0);
    widget.knx_controls->writable_roles = WRITABLE(KNX_MOVE_COMMAND);
    apply(&widget, w_cover_apply_state, def.entity_id, "unknown", "{\"supported_features\":3}");
    CHECK(lv_bar_get_value(bar(widget.obj)) == 0 && find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));
    disabled(widget.obj, APP_SYMBOL_ARROW_UP, false);
    lv_obj_delete(widget.obj);

    phase = "cover position-only commands";
    def.knx.controls[KNX_MOVE_COMMAND] = def.knx.controls[KNX_STOP_COMMAND] = NULL;
    def.knx.controls[KNX_POSITION_COMMAND] = "cover_position";
    widget = (ui_widget_instance_t){0};
    CHECK(w_cover_create(&def, lv_screen_active(), &widget) == ESP_OK);
    widget.knx_controls->writable_roles = WRITABLE(KNX_POSITION_COMMAND);
    apply(&widget, w_cover_apply_state, def.entity_id, "unknown", "{\"supported_features\":7}");
    click(widget.obj, APP_SYMBOL_ARROW_UP); command(before++, "cover_position", KNX_COVER_OPEN, 0);
    click(widget.obj, APP_SYMBOL_ARROW_DOWN); command(before++, "cover_position", KNX_COVER_CLOSE, 0);
    CHECK(lv_obj_is_hidden(button(widget.obj, APP_SYMBOL_STOP)));
    click(widget.obj, APP_SYMBOL_STOP);
    CHECK(sim_knx_writes() == before);
    lv_obj_delete(widget.obj);
}

static const char *climate_attrs = "{\"temperature\":21,\"current_temperature\":19.5,\"min_temp\":7,\"max_temp\":35,"
    "\"hvac_modes\":[\"auto\",\"heat\",\"cool\",\"off\",\"fan_only\",\"dry\"]}";
static const char *climate_no_feedback = "{\"min_temp\":7,\"max_temp\":35,"
    "\"hvac_modes\":[\"auto\",\"heat\",\"cool\",\"off\",\"fan_only\",\"dry\"]}";

static void climate_test(void)
{
    phase = "climate command-only mode";
    ui_widget_def_t def = {.w = 420, .h = 300};
    strcpy(def.type, "climate_tile"); strcpy(def.entity_id, "knx:target_feedback");
    def.knx.controls[KNX_TARGET_TEMPERATURE_COMMAND] = "target_command";
    def.knx.controls[KNX_TARGET_TEMPERATURE_FEEDBACK] = "target_feedback";
    def.knx.controls[KNX_CURRENT_TEMPERATURE_FEEDBACK] = "current_temperature";
    def.knx.controls[KNX_HVAC_MODE_COMMAND] = "mode_command";
    def.knx.controls[KNX_HVAC_MODE_FEEDBACK] = "mode_feedback";
    ui_widget_instance_t widget = {0};
    CHECK(w_climate_create(&def, lv_screen_active(), &widget) == ESP_OK);
    CHECK(widget.knx_controls != NULL);
    widget.knx_controls->writable_roles = WRITABLE(KNX_TARGET_TEMPERATURE_COMMAND) | WRITABLE(KNX_HVAC_MODE_COMMAND);
    apply(&widget, w_climate_apply_state, def.entity_id, "unknown", climate_no_feedback);
    disabled(widget.obj, APP_SYMBOL_MINUS, true);
    disabled(widget.obj, APP_SYMBOL_PLUS, true);
    disabled(widget.obj, "--", false);
    unsigned before = sim_knx_writes();
    click(widget.obj, APP_SYMBOL_PLUS); wait_ms(1000);
    CHECK(sim_knx_writes() == before);
    click(widget.obj, "--");
    click(lv_layer_top(), "Heat"); command(before++, "mode_command", KNX_SET_HVAC_MODE, 1);
    CHECK(find_label(widget.obj, "Heat", false) == NULL);

    phase = "climate deferred command keeps confirmed target";
    apply(&widget, w_climate_apply_state, def.entity_id, "heat", climate_attrs);
    disabled(widget.obj, APP_SYMBOL_PLUS, false);
    temperature(widget.obj, 21, true);
    CHECK(text_contains(widget.obj, "19.5"));
    click(widget.obj, APP_SYMBOL_PLUS);
    temperature(widget.obj, 21, true); temperature(widget.obj, 21.5f, false);
    wait_ms(1000); command(before++, "target_command", KNX_SET_TARGET_TEMPERATURE, 215);
    temperature(widget.obj, 21, true);
    click(widget.obj, APP_SYMBOL_PLUS); wait_ms(1000);
    command(before++, "target_command", KNX_SET_TARGET_TEMPERATURE, 215);
    click(widget.obj, APP_SYMBOL_PLUS); click(widget.obj, APP_SYMBOL_PLUS); wait_ms(1000);
    command(before++, "target_command", KNX_SET_TARGET_TEMPERATURE, 220);
    temperature(widget.obj, 21, true);
    /* A different bus setpoint supersedes a queued local draft. */
    click(widget.obj, APP_SYMBOL_PLUS);
    apply(&widget, w_climate_apply_state, def.entity_id, "heat", "{\"temperature\":19,\"current_temperature\":19.5}");
    wait_ms(1000); CHECK(sim_knx_writes() == before);
    temperature(widget.obj, 19, true); temperature(widget.obj, 21, false);

    phase = "climate mode values and stale feedback";
    apply(&widget, w_climate_apply_state, def.entity_id, "heat", climate_attrs);
    click(widget.obj, "Heat"); click(lv_layer_top(), "Cool");
    command(before++, "mode_command", KNX_SET_HVAC_MODE, 3);
    CHECK(find_label(widget.obj, "Heat", false) && !find_label(widget.obj, "Cool", false));
    const struct { const char *name; int value; } modes[] = {
        {"auto", 0}, {"off", 6}, {"fan_only", 9}, {"dry", 14}
    };
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        char key[40]; snprintf(key, sizeof(key), "climate.mode.%s", modes[i].name);
        click(widget.obj, "Heat"); click(lv_layer_top(), ui_i18n_get(key, modes[i].name));
        command(before++, "mode_command", KNX_SET_HVAC_MODE, modes[i].value);
        CHECK(find_label(widget.obj, "Heat", false));
    }
    click(widget.obj, APP_SYMBOL_PLUS);
    apply(&widget, w_climate_apply_state, def.entity_id, "unknown", climate_no_feedback);
    wait_ms(1000); CHECK(sim_knx_writes() == before);
    temperature(widget.obj, 21, false);
    disabled(widget.obj, APP_SYMBOL_PLUS, true);
    CHECK(!text_contains(widget.obj, "19.5"));
    /* Missing target must not be reused after reconnect or object reconfiguration. */
    widget.knx_controls->writable_roles = 0;
    apply(&widget, w_climate_apply_state, def.entity_id, "unavailable", "{}");
    widget.knx_controls->writable_roles = WRITABLE(KNX_TARGET_TEMPERATURE_COMMAND) | WRITABLE(KNX_HVAC_MODE_COMMAND);
    apply(&widget, w_climate_apply_state, def.entity_id, "unknown", climate_no_feedback);
    click(widget.obj, APP_SYMBOL_PLUS); wait_ms(1000);
    CHECK(sim_knx_writes() == before);
    disabled(widget.obj, APP_SYMBOL_PLUS, true);

    phase = "climate permission revocation cancels draft and popup";
    apply(&widget, w_climate_apply_state, def.entity_id, "heat", climate_attrs);
    click(widget.obj, APP_SYMBOL_PLUS);
    click(widget.obj, "Heat");
    widget.knx_controls->writable_roles = 0;
    apply(&widget, w_climate_apply_state, def.entity_id, "heat", climate_attrs);
    wait_ms(1000); CHECK(sim_knx_writes() == before);
    disabled(widget.obj, APP_SYMBOL_PLUS, true);
    disabled(widget.obj, "Heat", true);
    CHECK(find_label(lv_layer_top(), "Cool", true) == NULL);
    click(widget.obj, "Heat");
    CHECK(find_label(lv_layer_top(), "Cool", true) == NULL);
    click(widget.obj, APP_SYMBOL_PLUS); wait_ms(1000);
    CHECK(sim_knx_writes() == before);
    lv_obj_delete(widget.obj);
}

static void fan_test(void)
{
    phase = "fan explicit speed without feedback";
    ui_widget_def_t def = {.w = 420, .h = 320};
    strcpy(def.type, "fan_tile"); strcpy(def.entity_id, "knx:speed_feedback"); strcpy(def.title, "KNX Fan");
    def.knx.controls[KNX_SWITCH_COMMAND] = "fan_power";
    def.knx.controls[KNX_SWITCH_FEEDBACK] = "power_feedback";
    def.knx.controls[KNX_FAN_SPEED_COMMAND] = "fan_speed";
    def.knx.controls[KNX_FAN_SPEED_FEEDBACK] = "speed_feedback";
    ui_widget_instance_t widget = {0};
    CHECK(w_fan_create(&def, lv_screen_active(), &widget) == ESP_OK);
    CHECK(widget.knx_controls != NULL);
    widget.knx_controls->writable_roles = WRITABLE(KNX_SWITCH_COMMAND) | WRITABLE(KNX_FAN_SPEED_COMMAND);
    widget.knx_controls->switch_writable = true;
    apply(&widget, w_fan_apply_state, def.entity_id, "unknown", "{\"percentage_step\":25,\"supported_features\":1}");
    CHECK(find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));
    disabled(widget.obj, APP_SYMBOL_MINUS, true);
    disabled(widget.obj, APP_SYMBOL_PLUS, true);
    disabled(widget.obj, APP_SYMBOL_POWER, true);
    unsigned before = sim_knx_writes();
    click(widget.obj, APP_SYMBOL_PLUS); click(widget.obj, APP_SYMBOL_POWER);
    CHECK(sim_knx_writes() == before);
    disabled(widget.obj, "50%", false);
    click(widget.obj, "50%"); command(before++, "fan_speed", KNX_SET_FAN_PERCENT, 50);
    CHECK(find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));
    CHECK(bar(widget.obj) && lv_bar_get_value(bar(widget.obj)) == 0);

    phase = "fan relative changes and power remain independent";
    apply(&widget, w_fan_apply_state, def.entity_id, "unknown", "{\"percentage\":50,\"percentage_step\":25,\"supported_features\":1}");
    const lv_color_t idle = lv_obj_get_style_bg_color(button(widget.obj, "25%"), LV_PART_MAIN);
    CHECK(!same_color(idle, lv_obj_get_style_bg_color(button(widget.obj, "50%"), LV_PART_MAIN)));
    const char *unmatched[] = {"{\"percentage\":62,\"percentage_step\":25,\"supported_features\":1}",
                               "{\"percentage\":88,\"percentage_step\":25,\"supported_features\":1}"};
    const char *presets[] = {"25%", "50%", "75%", "100%"};
    for (unsigned i = 0; i < sizeof(unmatched) / sizeof(unmatched[0]); ++i) {
        apply(&widget, w_fan_apply_state, def.entity_id, "unknown", unmatched[i]);
        for (unsigned p = 0; p < sizeof(presets) / sizeof(presets[0]); ++p)
            CHECK(same_color(idle, lv_obj_get_style_bg_color(button(widget.obj, presets[p]), LV_PART_MAIN)));
    }
    apply(&widget, w_fan_apply_state, def.entity_id, "unknown", "{\"percentage\":50,\"percentage_step\":25,\"supported_features\":1}");
    disabled(widget.obj, APP_SYMBOL_PLUS, false);
    disabled(widget.obj, APP_SYMBOL_POWER, true);
    click(widget.obj, APP_SYMBOL_PLUS); command(before++, "fan_speed", KNX_SET_FAN_PERCENT, 75);
    CHECK(lv_bar_get_value(bar(widget.obj)) == 50 && find_label(widget.obj, "50 %", false));
    click(widget.obj, APP_SYMBOL_PLUS); command(before++, "fan_speed", KNX_SET_FAN_PERCENT, 75);
    click(widget.obj, APP_SYMBOL_MINUS); command(before++, "fan_speed", KNX_SET_FAN_PERCENT, 25);
    apply(&widget, w_fan_apply_state, def.entity_id, "off", "{\"percentage\":50,\"percentage_step\":25,\"supported_features\":1}");
    disabled(widget.obj, APP_SYMBOL_POWER, false);
    click(widget.obj, APP_SYMBOL_POWER); command(before++, "fan_power", KNX_TOGGLE, 0);
    CHECK(find_label(widget.obj, "OFF  50 %", false));
    CHECK(lv_bar_get_value(bar(widget.obj)) == 50);

    phase = "fan permission and feedback invalidation";
    widget.knx_controls->writable_roles = 0;
    widget.knx_controls->switch_writable = false;
    apply(&widget, w_fan_apply_state, def.entity_id, "on", "{\"percentage\":50,\"percentage_step\":25,\"supported_features\":0}");
    disabled(widget.obj, APP_SYMBOL_MINUS, true);
    disabled(widget.obj, APP_SYMBOL_PLUS, true);
    disabled(widget.obj, APP_SYMBOL_POWER, true);
    click(widget.obj, APP_SYMBOL_MINUS); click(widget.obj, APP_SYMBOL_POWER); click(widget.obj, "50%");
    CHECK(sim_knx_writes() == before);
    apply(&widget, w_fan_apply_state, def.entity_id, "unavailable", "{}");
    CHECK(lv_bar_get_value(bar(widget.obj)) == 0 && find_label(widget.obj, ui_i18n_get("common.unavailable", "unavailable"), false));
    widget.knx_controls->writable_roles = WRITABLE(KNX_SWITCH_COMMAND) | WRITABLE(KNX_FAN_SPEED_COMMAND);
    widget.knx_controls->switch_writable = true;
    apply(&widget, w_fan_apply_state, def.entity_id, "unknown", "{\"percentage_step\":25,\"supported_features\":1}");
    disabled(widget.obj, APP_SYMBOL_PLUS, true);
    disabled(widget.obj, APP_SYMBOL_POWER, true);
    CHECK(lv_bar_get_value(bar(widget.obj)) == 0 && find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));
    click(widget.obj, APP_SYMBOL_PLUS); click(widget.obj, APP_SYMBOL_POWER);
    CHECK(sim_knx_writes() == before);
    click(widget.obj, "100%"); command(before++, "fan_speed", KNX_SET_FAN_PERCENT, 100);
    CHECK(find_label(widget.obj, ui_i18n_get("common.unknown", "unknown"), false));
    /* Percentage chips must fit at the existing large fan tile's minimum size. */
    lv_obj_set_size(widget.obj, 240, 220);
    lv_obj_update_layout(widget.obj);
    const char *speeds[] = {"25%", "50%", "75%", "100%"};
    for (unsigned i = 0; i < sizeof(speeds) / sizeof(speeds[0]); ++i) {
        lv_obj_t *text = find_label(widget.obj, speeds[i], true);
        CHECK(text && lv_obj_is_visible(text));
        lv_point_t size;
        lv_text_get_size(&size, speeds[i], lv_obj_get_style_text_font(text, LV_PART_MAIN),
                        lv_obj_get_style_text_letter_space(text, LV_PART_MAIN),
                        lv_obj_get_style_text_line_space(text, LV_PART_MAIN), LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(text)) fprintf(stderr, "Fan chip %s needs %dpx, has %dpx\n",
                                                    speeds[i], (int)size.x, (int)lv_obj_get_width(text));
        CHECK(size.x <= lv_obj_get_width(text));
    }
    capture_if_requested(widget.obj);
    lv_obj_delete(widget.obj);
}

void sim_knx_controls_regression(void (*delay)(uint32_t))
{
    wait_ms = delay;
    sim_host_set_online(false);
    sim_host_service_reset(ESP_OK);
    const unsigned bindings_before = sim_host_binding_calls();
    cover_test(); climate_test(); fan_test();
    phase = "HA transport isolation";
    wait_ms(1000);
    CHECK(sim_host_service_calls()->count == 0);
    CHECK(sim_host_binding_calls() == bindings_before);
    sim_host_set_online(true);
    printf("KNX native controls: %u checks passed (real LVGL, mocked KNX transport)\n", checks);
}
