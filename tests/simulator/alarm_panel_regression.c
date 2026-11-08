/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "host_adapters.h"
#include "sim_clock.h"
#include "ui/ui_widget_factory.h"
#include "ui/ui_i18n.h"
#include "ui/widgets/w_alarm_panel.h"

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { fprintf(stderr, "alarm panel line %d: %s\n", __LINE__, #c); abort(); } } while (0)
static lv_obj_t *find(lv_obj_t *root, const lv_obj_class_t *type, const char *text)
{
    if (lv_obj_check_type(root, type)) {
        const char *value = text ? lv_label_get_text(root) : "";
        if (!text || !strcmp(value, text) || (!strncmp(value, text, strlen(text)) && value[strlen(text)] == ':')) return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *result = find(lv_obj_get_child(root, i), type, text);
        if (result) return result;
    }
    return NULL;
}
static lv_obj_t *button(ui_widget_instance_t *w, const char *text)
{
    lv_obj_t *label = find(w->obj, &lv_label_class, text);
    if (!label) fprintf(stderr, "Missing alarm control: %s\n", text);
    CHECK(label); return lv_obj_get_parent(label);
}
static void state(ui_widget_instance_t *w, const char *entity, const char *value, const char *attributes)
{
    ha_state_t s = {0};
    snprintf(s.entity_id, sizeof(s.entity_id), "%s", entity);
    snprintf(s.state, sizeof(s.state), "%s", value);
    snprintf(s.attributes_json, sizeof(s.attributes_json), "%s", attributes);
    ui_widget_factory_apply_state(w, &s);
    lv_obj_update_layout(w->obj);
}
static void digit(ui_widget_instance_t *w, unsigned n)
{
    lv_obj_t *keypad = find(w->obj, &lv_buttonmatrix_class, NULL);
    CHECK(keypad);
    lv_buttonmatrix_set_selected_button(keypad, n - 1);
    lv_obj_send_event(keypad, LV_EVENT_VALUE_CHANGED, NULL);
}
static void click(ui_widget_instance_t *w, const char *text)
{
    lv_obj_send_event(button(w, text), LV_EVENT_CLICKED, NULL);
}
void sim_alarm_panel_regression(void (*delay)(uint32_t))
{
    CHECK(ui_i18n_init("en") == ESP_OK);
    const char *numeric = "{\"code_format\":\"number\",\"code_arm_required\":true,\"supported_features\":2}";
    ui_widget_def_t def = {.w = 1240, .h = 640};
    strcpy(def.type, "alarm_panel"); strcpy(def.entity_id, "alarm_control_panel.house");
    cJSON *config = cJSON_Parse("{\"alarm_zones\":[{\"entity_id\":\"binary_sensor.door\",\"name\":\"Door\"}],\"alarm_actions\":[{\"entity_id\":\"switch.bypass\",\"name\":\"Bypass\"},{\"entity_id\":\"button.panic\",\"name\":\"Panic\"}]}");
    def.alarm_config = config;
    ui_widget_instance_t w = {0};
    sim_host_set_online(true); sim_host_service_reset(ESP_OK);
    CHECK(ui_widget_factory_create(&def, lv_screen_active(), &w) == ESP_OK);
    cJSON_Delete(config); /* The widget must own its copied configuration. */
    ui_widget_factory_set_visible(&w, true);
    const char *ids[8]; CHECK(w_alarm_panel_entity_ids(&w, ids, 8) == 4);
    CHECK(!strcmp(ids[1], "binary_sensor.door"));
    state(&w, def.entity_id, "disarmed", numeric);
    CHECK(!lv_obj_has_state(button(&w, "Arm away"), LV_STATE_DISABLED));
    CHECK(lv_obj_has_state(button(&w, "Arm home"), LV_STATE_DISABLED));
    click(&w, "Arm away"); CHECK(sim_host_service_calls()->count == 0);
    digit(&w, 1); digit(&w, 2); click(&w, "Arm home"); CHECK(sim_host_service_calls()->count == 0);
    click(&w, "Arm away"); CHECK(sim_host_service_calls()->count == 1);
    CHECK(!strcmp(sim_host_service_calls()->service, "alarm_arm_away"));
    cJSON *sent = cJSON_Parse(sim_host_service_calls()->data);
    CHECK(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(sent, "code")), "12"));
    cJSON_Delete(sent);
    click(&w, "Arm away"); CHECK(sim_host_service_calls()->count == 1); /* PIN cleared */
    digit(&w, 3); ui_widget_factory_set_visible(&w, false); ui_widget_factory_set_visible(&w, true);
    click(&w, "Arm away"); CHECK(sim_host_service_calls()->count == 1);
    digit(&w, 4); sim_host_set_online(false); delay(300); sim_host_set_online(true); delay(300);
    click(&w, "Arm away"); CHECK(sim_host_service_calls()->count == 1);
    state(&w, def.entity_id, "armed_away", "{\"code_format\":\"text\"}");
    digit(&w, 1); click(&w, "Disarm"); CHECK(sim_host_service_calls()->count == 1);
    state(&w, def.entity_id, "armed_away", "{}");
    click(&w, "Disarm"); CHECK(sim_host_service_calls()->count == 2);
    CHECK(!strcmp(sim_host_service_calls()->service, "alarm_disarm"));
    state(&w, "switch.bypass", "unknown", "{}");
    CHECK(lv_obj_has_state(button(&w, "Bypass"), LV_STATE_DISABLED));
    click(&w, "Bypass"); CHECK(sim_host_service_calls()->count == 2);
    state(&w, "switch.bypass", "off", "{}"); click(&w, "Bypass");
    CHECK(sim_host_service_calls()->count == 3 && !strcmp(sim_host_service_calls()->service, "turn_on"));
    state(&w, "button.panic", "unknown", "{}");
    lv_obj_t *panic = button(&w, "Panic\nHold 2 s");
    CHECK(!lv_obj_has_state(panic, LV_STATE_DISABLED));
    sim_clock_set(1760000000);
    lv_obj_send_event(panic, LV_EVENT_PRESSED, NULL);
    sim_virtual_delay(1000); delay(1000); lv_obj_send_event(panic, LV_EVENT_RELEASED, NULL);
    sim_virtual_delay(1500); delay(1500); CHECK(sim_host_service_calls()->count == 3);
    lv_obj_send_event(panic, LV_EVENT_PRESSED, NULL);
    sim_virtual_delay(2100); delay(2100); CHECK(sim_host_service_calls()->count == 4);
    sim_virtual_delay(2100); delay(2100); CHECK(sim_host_service_calls()->count == 4);
    CHECK(!strcmp(sim_host_service_calls()->service, "press"));
    lv_obj_send_event(panic, LV_EVENT_RELEASED, NULL);
    lv_obj_delete(w.obj); sim_virtual_delay(2100); delay(2100);
    CHECK(sim_host_service_calls()->count == 4);
    printf("alarm panel regression: %u checks passed\n", checks);
}
