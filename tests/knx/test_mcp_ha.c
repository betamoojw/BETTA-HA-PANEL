/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include "ha/ha_model.h"
#include "ha/ha_service_request.h"
#include "xiaozhi/xiaozhi_mcp_ha.h"

static const char *layout;
static bool connected = true, synced = true, disconnect_midway;
static int connected_calls, lock_depth, fail_after = -1;
static size_t allocations;
static unsigned service_calls, service_attempts;
static bool send_failed;
static char last_request[2048];
static void *allocate(size_t size)
{
    if (fail_after == 0) return NULL;
    if (fail_after > 0) fail_after--;
    void *p = malloc(size);
    if (p) allocations++;
    return p;
}
static void release(void *p) { if (p) { assert(allocations); allocations--; free(p); } }
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps)
{
    (void)caps;
    void *p = allocate(count * size);
    if (p) memset(p, 0, count * size);
    return p;
}
void heap_caps_free(void *p) { release(p); }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &lock_depth; }
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) { (void)s; (void)ticks; assert(!lock_depth++); return 1; }
int xSemaphoreGive(SemaphoreHandle_t s) { (void)s; assert(lock_depth-- == 1); return 1; }
void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
cJSON *layout_store_load_parsed(void) { return layout ? cJSON_Parse(layout) : NULL; }
bool ha_client_is_connected(void) { return disconnect_midway && ++connected_calls == 2 ? false : connected; }
bool ha_client_is_initial_sync_done(void) { return synced; }
esp_err_t ha_client_call_service(const char *domain, const char *service, const char *data)
{
    assert(!lock_depth); service_attempts++;
    cJSON *request = ha_service_request_create(42, domain, service, data);
    if (!request) return ESP_ERR_NO_MEM;
    char *encoded = cJSON_PrintUnformatted(request); cJSON_Delete(request);
    if (!encoded) return ESP_ERR_NO_MEM;
    assert(strlen(encoded) < sizeof(last_request));
    snprintf(last_request, sizeof(last_request), "%s", encoded); cJSON_free(encoded);
    if (send_failed) return ESP_FAIL;
    service_calls++;
    return ESP_OK;
}
static const cJSON *field(const cJSON *o, const char *key) { return cJSON_GetObjectItemCaseSensitive(o, key); }
static const char *str(const cJSON *o, const char *key) { return cJSON_GetStringValue(field(o, key)); }
static void put(const char *id, const char *name, const char *value, const char *attrs)
{
    ha_state_t s = {0}; ha_entity_info_t e = {0};
    snprintf(s.entity_id, sizeof(s.entity_id), "%s", id);
    snprintf(s.state, sizeof(s.state), "%s", value);
    snprintf(s.attributes_json, sizeof(s.attributes_json), "%s", attrs);
    s.last_changed_unix_ms = 1234; s.ha_changed_s = 1793000000;
    snprintf(e.id, sizeof(e.id), "%s", id); snprintf(e.name, sizeof(e.name), "%s", name);
    assert(ha_model_upsert_state(&s) == ESP_OK); assert(ha_model_upsert_entity(&e) == ESP_OK);
}
static cJSON *call(const char *tool, const char *args, bool success)
{
    cJSON *j = cJSON_Parse(args); assert(j);
    char error[160] = {0}; connected_calls = 0;
    cJSON *out = xiaozhi_mcp_ha_call(tool, j, error, sizeof(error));
    cJSON_Delete(j);
    assert(success ? out != NULL : out == NULL && error[0]);
    if (out) {
        char *rendered = cJSON_PrintUnformatted(out); assert(rendered && strlen(rendered) <= 2800);
        cJSON_free(rendered);
    }
    return out;
}
static cJSON *get(const char *id, bool success)
{
    char args[160]; snprintf(args, sizeof(args), "{\"entity_id\":\"%s\"}", id);
    return call("self.ha.get_entity", args, success);
}

static void bind_entity(const char *id, const char *value, const char *attrs)
{
    static char fixture[512];
    snprintf(fixture, sizeof(fixture), "{\"pages\":[{\"widgets\":[{\"entity_id\":\"%s\"}]}]}", id);
    layout = fixture; put(id, "Control test", value, attrs);
}

static cJSON *control(const char *id, const char *action, const char *value, bool success)
{
    cJSON *args = cJSON_CreateObject();
    cJSON_AddStringToObject(args, "entity_id", id); cJSON_AddStringToObject(args, "action", action);
    if (value) cJSON_AddStringToObject(args, "value", value);
    char *json = cJSON_PrintUnformatted(args); cJSON_Delete(args); assert(json);
    unsigned before = service_calls;
    uint32_t revision = ha_model_state_revision();
    cJSON *result = call("self.ha.control_entity", json, success); cJSON_free(json);
    assert(service_calls == before + (success ? 1 : 0));
    assert(ha_model_state_revision() == revision); /* No optimistic state writes. */
    if (result) {
        assert(cJSON_IsTrue(field(result, "submitted")) && cJSON_IsFalse(field(result, "state_confirmed")));
        cJSON_Delete(result);
        result = cJSON_Parse(last_request); assert(result);
        assert(!strcmp(str(field(result, "service_data"), "entity_id"), id));
        assert(!field(result, "target"));
    }
    return result;
}

static void test_controls(void)
{
    bind_entity("switch.lamp", "off", "{}");
    cJSON *r = get("switch.lamp", true);
    assert(cJSON_IsTrue(field(field(r, "entity"), "controllable")));
    assert(cJSON_GetArraySize(field(field(r, "entity"), "controls")) == 2); cJSON_Delete(r);
    r = control("switch.lamp", "turn_on", NULL, true);
    assert(!strcmp(str(r, "domain"), "switch") && !strcmp(str(r, "service"), "turn_on")); cJSON_Delete(r);
    r = get("switch.lamp", true); assert(!strcmp(str(field(r, "entity"), "state"), "off")); cJSON_Delete(r);
    put("switch.lamp", "Control test", "on", "{}");
    r = get("switch.lamp", true); assert(!strcmp(str(field(r, "entity"), "state"), "on")); cJSON_Delete(r);
    cJSON_Delete(control("switch.lamp", "turn_off", NULL, true));
    control("switch.lamp", "turn_on", "ignored", false);
    control("switch.lamp", "toggle", NULL, false);
    control("switch.lamp", "unlock", NULL, false);
    control("switch.other", "turn_on", NULL, false);
    connected = false; control("switch.lamp", "turn_on", NULL, false); connected = true;
    synced = false; control("switch.lamp", "turn_on", NULL, false); synced = true;
    send_failed = true; unsigned attempts = service_attempts;
    control("switch.lamp", "turn_on", NULL, false); assert(service_attempts == attempts + 1); send_failed = false;
    layout = "{\"pages\":[]}"; control("switch.lamp", "turn_on", NULL, false);
    bind_entity("switch.lamp", "unavailable", "{}"); control("switch.lamp", "turn_on", NULL, false);
    bind_entity("switch.a,switch.b", "off", "{}"); control("switch.a,switch.b", "turn_on", NULL, false);
    bind_entity("light.room", "off", "{\"supported_color_modes\":[\"onoff\"]}");
    control("light.room", "set_brightness", "50", false);
    control("light.room", "set_color", "#ff0000", false);
    bind_entity("light.room", "off", "{\"supported_color_modes\":[\"rgb\",\"color_temp\"],\"min_color_temp_kelvin\":2200,\"max_color_temp_kelvin\":6500,\"effect_list\":[\"Rainbow\",\"Solid\"]}");
    r = control("light.room", "set_brightness", "55", true);
    assert(field(field(r, "service_data"), "brightness_pct")->valueint == 55); cJSON_Delete(r);
    r = control("light.room", "set_color", "#A010ff", true);
    const cJSON *rgb = field(field(r, "service_data"), "rgb_color");
    assert(cJSON_GetArrayItem(rgb, 0)->valueint == 160 && cJSON_GetArrayItem(rgb, 1)->valueint == 16 && cJSON_GetArrayItem(rgb, 2)->valueint == 255); cJSON_Delete(r);
    r = control("light.room", "set_color_temperature", "4000", true);
    assert(field(field(r, "service_data"), "color_temp_kelvin")->valueint == 4000); cJSON_Delete(r);
    control("light.room", "set_color_temperature", "1000", false);
    control("light.room", "set_color_temperature", "7000", false);
    control("light.room", "set_color", "#12x345", false);
    cJSON_Delete(control("light.room", "set_effect", "Rainbow", true));
    control("light.room", "set_effect", "Unknown effect", false);
    const char *bad[] = {"101", "-1", "50.5", "NaN", "inf", "1e309", "0x32", "50%", " 50", "50 ", "{\"entity_id\":\"all\"}"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) control("light.room", "set_brightness", bad[i], false);
    bind_entity("fan.room", "off", "{\"supported_features\":59,\"preset_modes\":[\"eco\",\"normal\"]}");
    cJSON_Delete(control("fan.room", "turn_on", NULL, true));
    cJSON_Delete(control("fan.room", "turn_off", NULL, true));
    cJSON_Delete(control("fan.room", "set_percentage", "75", true));
    r = control("fan.room", "oscillate", "true", true);
    assert(cJSON_IsTrue(field(field(r, "service_data"), "oscillating"))); cJSON_Delete(r);
    control("fan.room", "oscillate", "yes", false);
    cJSON_Delete(control("fan.room", "set_preset_mode", "eco", true));
    bind_entity("fan.room", "off", "{\"supported_features\":0}"); control("fan.room", "set_percentage", "75", false);
    control("fan.room", "turn_on", NULL, false);
    bind_entity("cover.room", "closed", "{\"supported_features\":15}");
    cJSON_Delete(control("cover.room", "open_cover", NULL, true));
    cJSON_Delete(control("cover.room", "close_cover", NULL, true));
    cJSON_Delete(control("cover.room", "stop_cover", NULL, true));
    r = control("cover.room", "set_position", "40", true);
    assert(!strcmp(str(r, "service"), "set_cover_position") && field(field(r, "service_data"), "position")->valueint == 40); cJSON_Delete(r);
    bind_entity("climate.room", "off", "{\"supported_features\":57,\"min_temp\":16,\"max_temp\":30,\"target_temp_step\":0.5,\"hvac_modes\":[\"off\",\"heat\",\"cool\"],\"fan_modes\":[\"auto\",\"low\"],\"swing_modes\":[\"on\",\"off\"],\"preset_modes\":[\"eco\"]}");
    cJSON_Delete(control("climate.room", "set_temperature", "22.5", true));
    control("climate.room", "set_temperature", "22.2", false);
    control("climate.room", "set_temperature", "31", false);
    cJSON_Delete(control("climate.room", "set_hvac_mode", "heat", true));
    control("climate.room", "set_hvac_mode", "dry", false);
    cJSON_Delete(control("climate.room", "set_fan_mode", "auto", true));
    cJSON_Delete(control("climate.room", "set_swing_mode", "on", true));
    cJSON_Delete(control("climate.room", "set_preset_mode", "eco", true));
    bind_entity("climate.room", "heat_cool", "{\"supported_features\":2,\"min_temp\":16,\"max_temp\":30}");
    control("climate.room", "set_temperature", "22", false);
    bind_entity("number.room", "1", "{\"min\":-10,\"max\":20,\"step\":0.5}");
    cJSON_Delete(control("number.room", "set_value", "7.5", true));
    control("number.room", "set_value", "7.2", false);
    control("number.room", "set_value", "21", false);
    bind_entity("number.room", "1", "{}"); control("number.room", "set_value", "7", false);
    bind_entity("select.room", "old", "{\"options\":[\"old\",\"舒适\\\"模式\"]}");
    r = control("select.room", "select_option", "舒适\"模式", true);
    assert(!strcmp(str(field(r, "service_data"), "option"), "舒适\"模式")); cJSON_Delete(r);
    control("select.room", "select_option", "missing", false);
    const char *ids[] = {"input_boolean.room", "input_number.room", "input_select.room", "input_button.room", "button.room", "scene.room", "script.room"};
    const char *actions[] = {"turn_on", "set_value", "select_option", "press", "press", "turn_on", "turn_off"};
    const char *values[] = {NULL, "3", "a", NULL, NULL, NULL, NULL};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        bind_entity(ids[i], "unknown", "{\"min\":0,\"max\":5,\"options\":[\"a\"]}");
        cJSON_Delete(control(ids[i], actions[i], values[i], true));
    }
    bind_entity("sensor.room", "23", "{}"); control("sensor.room", "turn_on", NULL, false);
    r = get("sensor.room", true); assert(cJSON_IsFalse(field(field(r, "entity"), "controllable"))); cJSON_Delete(r);
    /* Maximum escaped cached choice text still leaves bounded control metadata. */
    cJSON *attrs = cJSON_Parse("{\"supported_features\":57,\"min_temp\":16,\"max_temp\":30}");
    const char *keys[] = {"hvac_modes", "fan_modes", "preset_modes", "swing_modes"};
    char escaped[41]; memset(escaped, '\\', sizeof(escaped) - 1); escaped[40] = 0;
    for (size_t i = 0; i < 4; i++) {
        cJSON *options = cJSON_AddArrayToObject(attrs, keys[i]);
        for (int k = 0; k < 3; k++) cJSON_AddItemToArray(options, cJSON_CreateString(escaped));
    }
    char *encoded = cJSON_PrintUnformatted(attrs); assert(strlen(encoded) < APP_HA_ATTRS_MAX_LEN);
    char name[APP_MAX_NAME_LEN]; memset(name, 1, sizeof(name) - 1); name[sizeof(name) - 1] = 0;
    bind_entity("climate.room", name, encoded); put("climate.room", name, name, encoded);
    r = get("climate.room", true);
    assert(cJSON_GetArraySize(field(field(r, "entity"), "controls")) == 5);
    cJSON_Delete(r); cJSON_free(encoded); cJSON_Delete(attrs);
    /* The production request builder must retain the target or fail completely. */
    const char *invalid[] = {"[]", "null", "{bad}", "{\"entity_id\":\"light.room\"} trailing"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) assert(!ha_service_request_create(9, "light", "turn_on", invalid[i]));
    size_t before = allocations; bool completed = false;
    for (int i = 0; i < 100 && !completed; i++) {
        fail_after = i;
        r = ha_service_request_create(9, "light", "turn_on", "{\"entity_id\":\"light.room\"}");
        fail_after = -1; completed = r != NULL;
        if (r) assert(!strcmp(str(field(r, "service_data"), "entity_id"), "light.room"));
        cJSON_Delete(r); assert(allocations == before);
    }
    assert(completed);
    /* OOM at any stage before transmission sends no command, leaks no memory. */
    bind_entity("light.room", "off", "{\"supported_color_modes\":[\"rgb\"]}");
    cJSON *args = cJSON_Parse("{\"entity_id\":\"light.room\",\"action\":\"set_color\",\"value\":\"#ff0088\"}"); assert(args);
    before = allocations; completed = false;
    for (int i = 0; i < 800 && !completed; i++) {
        unsigned calls_before = service_calls; char error[160] = {0};
        fail_after = i; r = xiaozhi_mcp_ha_call("self.ha.control_entity", args, error, sizeof(error));
        fail_after = -1; completed = r != NULL;
        assert(service_calls == calls_before + (completed ? 1 : 0));
        assert(r || error[0]); cJSON_Delete(r); assert(allocations == before && !lock_depth);
    }
    assert(completed); cJSON_Delete(args);
}
int main(void)
{
    cJSON_Hooks hooks = {.malloc_fn = allocate, .free_fn = release}; cJSON_InitHooks(&hooks);
    assert(ha_model_init() == ESP_OK);
    const size_t baseline = allocations;
    layout = "{\"pages\":[{\"widgets\":[{\"entity_id\":\"sensor.room\"},{\"entity_id\":\"light.room\"},{\"entity_id\":\"sensor.room\"}]}]}";
    put("sensor.room", "客厅温度", "23.5", "{\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"access_token\":\"secret\",\"entity_picture\":\"/token\"}");
    put("sensor.hidden", "Private", "99", "{}");
    cJSON *r = call("self.ha.list_entities", "{}", true);
    assert(field(r, "total_entities")->valueint == 2 && cJSON_IsTrue(field(r, "consistent")));
    cJSON_Delete(r);
    r = get("sensor.room", true);
    const cJSON *e = field(r, "entity");
    assert(!strcmp(str(e, "name"), "客厅温度") && !strcmp(str(e, "unit"), "°C"));
    assert(cJSON_IsTrue(field(e, "valid")) && !field(field(e, "attributes"), "access_token"));
    assert(!field(field(e, "attributes"), "entity_picture"));
    int revision = field(r, "state_revision")->valueint; cJSON_Delete(r);
    /* Real shared model update, including attribute-only changes and rename. */
    put("sensor.room", "室内温度", "24.1", "{\"unit_of_measurement\":\"°C\"}");
    r = get("sensor.room", true); e = field(r, "entity");
    assert(!strcmp(str(e, "state"), "24.1") && !strcmp(str(e, "name"), "室内温度"));
    assert(field(r, "state_revision")->valueint > revision); cJSON_Delete(r);
    put("sensor.room", "室内温度", "24.1", "{\"unit_of_measurement\":\"K\"}");
    r = get("sensor.room", true); assert(!strcmp(str(field(r, "entity"), "unit"), "K")); cJSON_Delete(r);
    get("sensor.hidden", false);
    r = get("light.room", true); assert(cJSON_IsNull(field(field(r, "entity"), "state")) && cJSON_IsFalse(field(field(r, "entity"), "known"))); cJSON_Delete(r);
    connected = false; r = get("sensor.room", true); e = field(r, "entity");
    assert(cJSON_IsFalse(field(e, "valid")) && cJSON_IsTrue(field(e, "stale"))); cJSON_Delete(r);
    connected = true; synced = false; r = get("sensor.room", true);
    assert(cJSON_IsFalse(field(field(r, "entity"), "valid"))); cJSON_Delete(r); synced = true;
    disconnect_midway = true; r = get("sensor.room", true);
    assert(cJSON_IsFalse(field(r, "consistent"))); cJSON_Delete(r); disconnect_midway = false;
    put("sensor.room", "室内温度", "unavailable", "{}");
    r = get("sensor.room", true); assert(cJSON_IsFalse(field(field(r, "entity"), "valid"))); cJSON_Delete(r);
    put("sensor.room", "室内温度", "unknown", "{}");
    r = get("sensor.room", true); assert(cJSON_IsFalse(field(field(r, "entity"), "valid"))); cJSON_Delete(r);
    r = call("self.ha.list_entities", "{\"search\":\"室内\",\"domain\":\"sensor\"}", true);
    assert(field(r, "matching_entities")->valueint == 1); cJSON_Delete(r);
    r = call("self.ha.list_entities", "{\"domain\":\"sen\"}", true);
    assert(field(r, "matching_entities")->valueint == 0); cJSON_Delete(r);
    r = call("self.ha.list_entities", "{\"search\":\"ROOM\",\"limit\":1}", true);
    char signature[9]; snprintf(signature, sizeof(signature), "%s", str(r, "layout_signature"));
    assert(cJSON_IsTrue(field(r, "has_more"))); cJSON_Delete(r);
    char args[180]; snprintf(args, sizeof(args), "{\"offset\":1,\"limit\":1,\"search\":\"ROOM\",\"layout_signature\":\"%s\"}", signature);
    r = call("self.ha.list_entities", args, true);
    assert(!strcmp(str(cJSON_GetArrayItem(field(r, "entities"), 0), "entity_id"), "sensor.room"));
    assert(cJSON_IsFalse(field(r, "has_more"))); cJSON_Delete(r);
    /* Removing a binding revokes access immediately, even while still cached. */
    layout = "{\"pages\":[{\"widgets\":[{\"entity_id\":\"sensor.new\"}]}]}";
    call("self.ha.list_entities", args, false); get("sensor.room", false);
    r = get("sensor.new", true); assert(cJSON_IsFalse(field(field(r, "entity"), "known"))); cJSON_Delete(r);
    put("sensor.new", "New", "8", "{}"); r = get("sensor.new", true);
    assert(cJSON_IsTrue(field(field(r, "entity"), "valid"))); cJSON_Delete(r);
    ha_model_reset(); r = get("sensor.new", true);
    assert(cJSON_IsFalse(field(field(r, "entity"), "known"))); cJSON_Delete(r);
    /* Use the production layout walker for nontrivial, secondary bindings. */
    layout = "{\"alerts\":[{\"entity_id\":\"event.door\",\"camera_entity_id\":\"camera.door\"}],\"pages\":[{\"type\":\"house_dashboard\",\"house\":{\"rooms\":[{\"entity_id\":\"sensor.temp\",\"room_entities\":[\"light.kitchen\"]}]}},{\"type\":\"energy_dashboard\",\"energy\":{\"source\":\"manual_live\",\"home_power_entity_id\":\"sensor.power\"}},{\"widgets\":[{\"entity_id\":\"climate.room\",\"secondary_entity_id\":\"sensor.room\"}]}]}";
    size_t cursor = 0, total = 0;
    do {
        snprintf(args, sizeof(args), "{\"offset\":%u%s%s%s}", (unsigned)cursor,
            cursor ? ",\"layout_signature\":\"" : "", cursor ? signature : "", cursor ? "\"" : "");
        r = call("self.ha.list_entities", args, true);
        total += cJSON_GetArraySize(field(r, "entities"));
        cursor = cJSON_IsTrue(field(r, "has_more")) ? field(r, "next_offset")->valueint : 0;
        snprintf(signature, sizeof(signature), "%s", str(r, "layout_signature")); cJSON_Delete(r);
    } while (cursor);
    assert(total == 7);
    call("self.ha.list_entities", "{\"offset\":1}", false);
    layout = "{\"pages\":[]}"; r = call("self.ha.list_entities", "{}", true);
    assert(!cJSON_GetArraySize(field(r, "entities"))); cJSON_Delete(r);
    layout = NULL; call("self.ha.list_entities", "{}", false);
    /* Escaped names/states must shrink a page without losing its next ID. */
    cJSON *large = cJSON_Parse("{\"pages\":[{\"widgets\":[{\"room_entities\":[]}]}]}");
    cJSON *bindings = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(field(large, "pages"), 0), "widgets"), 0), "room_entities");
    char escaped[APP_MAX_NAME_LEN]; memset(escaped, '\x01', sizeof(escaped) - 1); escaped[sizeof(escaped) - 1] = 0;
    for (int i = 0; i < 5; i++) {
        char id[APP_MAX_ENTITY_ID_LEN]; memset(id, 'a', sizeof(id) - 1); id[sizeof(id) - 1] = 0;
        memcpy(id, "sensor.", 7); id[7] = '0' + i;
        assert(cJSON_AddItemToArray(bindings, cJSON_CreateString(id)));
        put(id, escaped, escaped, "{}");
    }
    char *large_json = cJSON_PrintUnformatted(large); assert(large_json); layout = large_json;
    r = call("self.ha.list_entities", "{}", true);
    assert(cJSON_GetArraySize(field(r, "entities")) > 0 && cJSON_GetArraySize(field(r, "entities")) < 5);
    assert(cJSON_IsTrue(field(r, "has_more")) && field(r, "next_offset")->valueint == cJSON_GetArraySize(field(r, "entities")));
    cJSON_Delete(r); cJSON_free(large_json);
    /* Capacity exhaustion is an error, never an apparently complete scope. */
    for (int i = 5; i <= (APP_MAX_WIDGETS_TOTAL * 4) + (APP_MAX_PAGES * 9) + 4; i++) {
        char id[32]; snprintf(id, sizeof(id), "sensor.e%d", i);
        assert(cJSON_AddItemToArray(bindings, cJSON_CreateString(id)));
    }
    large_json = cJSON_PrintUnformatted(large); assert(large_json); layout = large_json;
    call("self.ha.list_entities", "{}", false);
    cJSON_free(large_json); cJSON_Delete(large);
    /* Allocation failures cannot leak buffers or return an invented state. */
    layout = "{\"pages\":[{\"widgets\":[{\"entity_id\":\"sensor.room\"}]}]}";
    put("sensor.room", "Temperature", "25", "{\"unit_of_measurement\":\"C\"}");
    for (int detail = 0; detail < 2; detail++) {
        cJSON *j = cJSON_Parse(detail ? "{\"entity_id\":\"sensor.room\"}" : "{}"); assert(j);
        size_t before = allocations; bool completed = false;
        for (int i = 0; i < 600 && !completed; i++) {
            fail_after = i; char error[160] = {0};
            r = xiaozhi_mcp_ha_call(detail ? "self.ha.get_entity" : "self.ha.list_entities", j, error, sizeof(error));
            fail_after = -1; completed = r != NULL;
            assert(r || error[0]); cJSON_Delete(r); assert(allocations == before && !lock_depth);
        }
        assert(completed); cJSON_Delete(j);
    }
    test_controls();
    assert(allocations == baseline);
    puts("HA MCP queries, scoped control matrix, capabilities, payloads, feedback, offline and allocation tests passed");
    return 0;
}
