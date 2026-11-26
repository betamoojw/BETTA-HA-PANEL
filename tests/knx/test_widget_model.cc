// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx_widget_model.h"
#include "layout/knx_layout_binding.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

cJSON* KnxObjectJson(const KnxCommunicationObject& object);

static KnxCommunicationObject feedback(const char* dpt, const char* value) {
    KnxCommunicationObject object;
    assert(KnxParseDpt(dpt, object.datapoint_type));
    assert(KnxParseValue(object.datapoint_type, value, object.current_value));
    object.readable = object.writable = object.live_feedback = true;
    object.last_update_ms = 325;
    return object;
}

static void test_scaling_sensor_feedback() {
    auto object = feedback("DPT-5.001", "0");
    object.group_address = "2/0/2";
    object.valid = true;
    ha_state_t state{};
    const struct { uint8_t wire; double percent; } samples[] = {{0, 0}, {158, 61.960784912109375}, {255, 100}};
    for (const auto& sample : samples) {
        const uint8_t telegram[] = {0, sample.wire};
        assert(KnxDecodeValue(object.datapoint_type, telegram, sizeof(telegram), object.current_value));
        assert(KnxWidgetProject(object, KNX_TILE_VALUE, state));
        assert(strstr(state.attributes_json, "\"knx_scaling\":true"));
        assert(strtod(state.state, nullptr) == sample.percent);
        assert(KnxWidgetProject(object, KNX_TILE_NUMBER, state));
        assert(!strstr(state.attributes_json, "knx_scaling") && strtod(state.state, nullptr) == sample.percent);
        cJSON *api = KnxObjectJson(object);
        assert(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(api, "value")) == sample.percent);
        assert(!cJSON_HasObjectItem(api, "knx_scaling"));
        cJSON_Delete(api);
    }
    object.display_precision = 2;
    assert(KnxWidgetProject(object, KNX_TILE_VALUE, state));
    assert(strstr(state.attributes_json, "\"display_precision\":2"));
    object.live_feedback = false;
    assert(!KnxWidgetProject(object, KNX_TILE_VALUE, state));
    auto humidity = feedback("DPT-9.007", "61.75");
    humidity.unit = "%";
    assert(KnxWidgetProject(humidity, KNX_TILE_VALUE, state));
    assert(!strstr(state.attributes_json, "knx_scaling") && !strcmp(state.state, "61.75"));
}

static void test_composite_roles_and_schema() {
    struct RoleCase { knx_role_t role; const char* allowed; const char* rejected; };
    for (const auto& c : {RoleCase{KNX_MOVE_COMMAND, "1.008", "1.007"},
                         RoleCase{KNX_STOP_COMMAND, "1.010", "1.007"},
                         RoleCase{KNX_POSITION_COMMAND, "5.001", "5.004"},
                         RoleCase{KNX_POSITION_FEEDBACK, "5.001", "5.004"},
                         RoleCase{KNX_TARGET_TEMPERATURE_COMMAND, "9.001", "9.002"},
                         RoleCase{KNX_CURRENT_TEMPERATURE_FEEDBACK, "9.001", "9.002"},
                         RoleCase{KNX_HVAC_MODE_COMMAND, "20.105", "20.102"},
                         RoleCase{KNX_HVAC_MODE_FEEDBACK, "20.105", "20.102"},
                         RoleCase{KNX_PRESET_COMMAND, "20.102", "20.105"},
                         RoleCase{KNX_PRESET_FEEDBACK, "20.102", "22.101"},
                         RoleCase{KNX_ANGLE_COMMAND, "5.003", "5.001"},
                         RoleCase{KNX_ANGLE_FEEDBACK, "5.003", "5.001"},
                         RoleCase{KNX_FAN_SPEED_COMMAND, "5.001", "5.010"},
                         RoleCase{KNX_FAN_SPEED_FEEDBACK, "5.001", "20.111"}}) {
        KnxCommunicationObject o;
        o.readable = o.writable = true;
        assert(KnxParseDpt(c.allowed, o.datapoint_type));
        assert(KnxWidgetRoleSupports(o, c.role));
        if (knx_role_is_command(c.role)) o.writable = false;
        else o.readable = false;
        assert(!KnxWidgetRoleSupports(o, c.role));
        o.readable = o.writable = true;
        assert(KnxParseDpt(c.rejected, o.datapoint_type));
        assert(!KnxWidgetRoleSupports(o, c.role));
    }
    assert(knx_tile_kind("cover_tile") == KNX_TILE_COVER);
    assert(knx_tile_kind("climate_tile") == KNX_TILE_CLIMATE);
    assert(knx_tile_kind("fan_tile") == KNX_TILE_FAN);
    assert(!knx_tile_has_role(KNX_TILE_LIGHT, KNX_TARGET_TEMPERATURE_COMMAND));
    assert(!knx_tile_has_role(KNX_TILE_FAN, KNX_BRIGHTNESS_COMMAND));
    assert(!knx_tile_has_role(KNX_TILE_COVER, KNX_SWITCH_COMMAND));
    assert(!knx_hvac_mode_name(2) && knx_hvac_mode_value("comfort") == -1);
    for (int value : {0, 1, 3, 6, 9, 14})
        assert(knx_hvac_mode_value(knx_hvac_mode_name(value)) == value);

    auto schema = [](const char* type, const char* json, bool valid) {
        cJSON* root = cJSON_Parse(json);
        assert(root);
        knx_layout_binding_t parsed;
        assert(knx_layout_binding(root, type, &parsed) == valid);
        if (valid) {
            char* saved = cJSON_PrintUnformatted(root);
            cJSON* reload = cJSON_Parse(saved);
            knx_layout_binding_t restored;
            assert(knx_layout_binding(reload, type, &restored));
            assert(parsed.position_open_at_100 == restored.position_open_at_100);
            for (int i = 0; i < KNX_CONTROL_ROLES; ++i) {
                assert(!!parsed.controls[i] == !!restored.controls[i]);
                if (parsed.controls[i]) assert(!strcmp(parsed.controls[i], restored.controls[i]));
            }
            cJSON_free(saved); cJSON_Delete(reload);
        }
        cJSON_Delete(root);
    };
    schema("cover_tile", R"({"source":"knx","roles":{"move_command":"M","position_feedback":"P"},"position_open_at_100":true})", true);
    schema("cover_tile", R"({"source":"knx","roles":{"position_command":"P"},"position_open_at_100":false})", true);
    schema("climate_tile", R"({"source":"knx","roles":{"target_temperature_command":"T","target_temperature_feedback":"TF","current_temperature_feedback":"CF","hvac_mode_command":"M","hvac_mode_feedback":"MF"}})", true);
    schema("fan_tile", R"({"source":"knx","roles":{"switch_command":"P","fan_speed_feedback":"S"}})", true);
    schema("fan_tile", R"({"source":"knx","roles":{"fan_speed_feedback":"S"},"position_open_at_100":false})", false);
    schema("cover_tile", R"({"source":"knx","roles":{"position_feedback":"P"},"position_open_at_100":1})", false);
    schema("cover_tile", R"({"source":"knx","roles":{"position_feedback":"P","position_feedback":"Q"}})", false);
    schema("cover_tile", R"({"source":"knx","roles":{"hvac_mode_command":"M"}})", false);
    schema("climate_tile", R"({"source":"knx","roles":{}})", false);
    for (const char* binding : {R"({"source":"knx","roles":{"brightness_command":"A"}})",
            R"({"source":"knx","roles":{"brightness_command":"A","brightness_feedback":null}})",
            R"({"source":"knx","roles":{"brightness_command":"A","brightness_feedback":""}})",
            R"({"source":"knx","roles":{"angle_command":"A","angle_feedback":"B"}})",
            R"({"source":"knx","object_id":"legacy"})"}) schema("slider", binding, true);
    schema("slider", R"({"source":"knx","roles":{"brightness_command":"A","angle_feedback":"B"}})", false);
    schema("slider", R"({"source":"knx","roles":{"brightness_command":"A","brightness_feedback":false}})", false);
    schema("select_tile", R"({"source":"knx","roles":{"preset_command":"P","preset_feedback":null}})", true);
    schema("select_tile", R"({"source":"knx","object_id":"ambiguous"})", false);
    for (const char* type : {"cover_tile", "climate_tile", "fan_tile"})
        schema(type, R"({"source":"knx","object_id":"legacy"})", false);
}

static void test_composite_commands() {
    std::string text;
    auto move = feedback("1.008", "off"), close = feedback("1.009", "off");
    for (const auto* o : {&move, &close}) {
        assert(KnxWidgetCommandValue(*o, KNX_COVER_OPEN, 0, nullptr, text) && text == "off");
        assert(KnxWidgetCommandValue(*o, KNX_COVER_CLOSE, 0, nullptr, text) && text == "on");
    }
    auto step = feedback("1.007", "off"), stop = feedback("1.010", "on"), blind = feedback("3.008", "true,1");
    assert(!KnxWidgetCommandValue(step, KNX_COVER_STOP, 0, nullptr, text));
    assert(KnxWidgetCommandValue(stop, KNX_COVER_STOP, 0, nullptr, text) && text == "off");
    assert(KnxWidgetCommandValue(blind, KNX_COVER_STOP, 0, nullptr, text) && text == "false,0");
    auto temperature = feedback("9.001", "21.5"), difference = feedback("9.002", "1");
    assert(KnxWidgetCommandValue(temperature, KNX_SET_TARGET_TEMPERATURE, 225, nullptr, text) && text == "22.5");
    assert(!KnxWidgetCommandValue(temperature, KNX_SET_TARGET_TEMPERATURE, -3000, nullptr, text));
    assert(!KnxWidgetCommandValue(difference, KNX_SET_TARGET_TEMPERATURE, 225, nullptr, text));
    auto mode = feedback("20.105", "0"), preset = feedback("20.102", "0");
    assert(KnxWidgetCommandValue(mode, KNX_SET_HVAC_MODE, 3, nullptr, text) && text == "3");
    assert(!KnxWidgetCommandValue(mode, KNX_SET_HVAC_MODE, 2, nullptr, text));
    assert(!KnxWidgetCommandValue(preset, KNX_SET_HVAC_MODE, 3, nullptr, text));
    for (int code = 0; code <= 4; ++code)
        assert(KnxWidgetCommandValue(preset, KNX_SET_PRESET, code, nullptr, text) && text == std::to_string(code));
    assert(!KnxWidgetCommandValue(preset, KNX_SET_PRESET, 5, nullptr, text));
    assert(!KnxWidgetCommandValue(mode, KNX_SET_PRESET, 0, nullptr, text));
    auto angle = feedback("5.003", "0");
    for (int value : {0, 180, 270, 360})
        assert(KnxWidgetCommandValue(angle, KNX_SET_ANGLE, value, nullptr, text) && text == std::to_string(value));
    assert(!KnxWidgetCommandValue(angle, KNX_SET_ANGLE, 361, nullptr, text));
    auto speed = feedback("5.001", "0");
    for (int percent : {0, 25, 50, 75, 100})
        assert(KnxWidgetCommandValue(speed, KNX_SET_FAN_PERCENT, percent, nullptr, text) && text == std::to_string(percent));
    assert(!KnxWidgetCommandValue(speed, KNX_SET_FAN_PERCENT, -1, nullptr, text));
    assert(!KnxWidgetCommandValue(speed, KNX_SET_FAN_PERCENT, 101, nullptr, text));
}

static void test_independent_value_sources() {
    auto target = feedback("9.001", "19.5");
    target.command_revision = 1;
    target.last_command = 21.5f;
    const KnxCommunicationObject* roles[KNX_ROLE_COUNT]{};
    roles[KNX_TARGET_TEMPERATURE_COMMAND] = &target;
    knx_control_binding_t binding{};
    ha_state_t state{};
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_CLIMATE, true, binding, state));
    assert(strstr(state.attributes_json, "\"temperature\":21.5") && !strstr(state.attributes_json, "current_temperature"));
    strcpy(binding.id[KNX_TARGET_TEMPERATURE_FEEDBACK], "missing");
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_CLIMATE, true, binding, state));
    assert(!strstr(state.attributes_json, "\"temperature\":"));
    auto move = feedback("1.008", "on");
    move.command_revision = 1;
    move.last_command = 1;
    roles[KNX_MOVE_COMMAND] = &move;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_COVER, true, binding, state));
    assert(!strcmp(state.state,"unknown") && !strstr(state.attributes_json,"current_position"));
    auto power = feedback("1.001", "on"), speed = feedback("5.001", "50");
    power.command_revision = speed.command_revision = 1;
    power.last_command = speed.last_command = 0;
    roles[KNX_SWITCH_COMMAND] = &power;
    roles[KNX_FAN_SPEED_COMMAND] = &speed;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_FAN, true, binding, state));
    assert(!strcmp(state.state, "off") && strstr(state.attributes_json, "\"percentage\":0"));
    roles[KNX_FAN_SPEED_FEEDBACK] = &speed;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_FAN, true, binding, state));
    assert(!strcmp(state.state, "off") && strstr(state.attributes_json, "\"percentage\":50"));
    roles[KNX_BRIGHTNESS_COMMAND] = &speed;
    roles[KNX_SWITCH_FEEDBACK] = &power;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_LIGHT, true, binding, state));
    assert(!strcmp(state.state, "on") && binding.brightness == 0);
    auto preset = feedback("20.102", "0");
    preset.command_revision = 1;
    preset.last_command = 0;
    roles[KNX_PRESET_COMMAND] = &preset;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_SELECT, false, binding, state));
    assert(!strcmp(state.state, "unavailable") && !knx_control_can_write(&binding, KNX_PRESET_COMMAND));
    preset.command_revision = 0;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_SELECT, true, binding, state));
    assert(!strcmp(state.state, "unknown") && knx_control_can_write(&binding, KNX_PRESET_COMMAND));
    preset.command_revision = 1;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_SELECT, true, binding, state));
    assert(!strcmp(state.state,"Auto"));
    roles[KNX_PRESET_FEEDBACK] = &preset;
    preset.live_feedback = false;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_SELECT, true, binding, state));
    assert(!strcmp(state.state, "unknown"));
    preset.live_feedback = true;
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_SELECT, true, binding, state));
    assert(!strcmp(state.state, "Auto"));
    preset.current_value = uint8_t(5);
    assert(KnxWidgetProjectRoles(roles, KNX_TILE_SELECT, true, binding, state));
    assert(!strcmp(state.state,"unknown"));
}

int main() {
    test_scaling_sensor_feedback();
    test_composite_roles_and_schema();
    test_composite_commands();
    test_independent_value_sources();
    auto temperature = feedback("DPT-9.001", "21.5");
    temperature.unit = "C";
    ha_state_t state{};
    assert(KnxWidgetProject(temperature, KNX_TILE_NUMBER, state));
    assert(!strcmp(state.state, "21.5") && strstr(state.attributes_json, "\"unit_of_measurement\":\"C\""));
    assert(state.last_changed_unix_ms == 325);
    assert(!KnxWidgetSupports(temperature, KNX_TILE_SLIDER));

    auto scaling = feedback("DPT-5.001", "42");
    assert(KnxWidgetProject(scaling, KNX_TILE_SLIDER, state));
    assert(!strcmp(state.state, "42") && strstr(state.attributes_json, "\"unit_of_measurement\":\"%\""));
    assert(KnxWidgetProject(scaling, KNX_TILE_LIGHT, state));
    assert(!strcmp(state.state, "on") && strstr(state.attributes_json, "\"brightness_pct\":42"));
    std::string command;
    assert(KnxWidgetCommandValue(scaling, KNX_SET_PERCENT, 75, nullptr, command) && command == "75");
    assert(!KnxWidgetCommandValue(scaling, KNX_SET_PERCENT, 101, nullptr, command));
    assert(!KnxWidgetCommandValue(scaling, KNX_SET_RGB, 0xff0000, nullptr, command));

    auto color = feedback("DPT-232.600", "128,64,0");
    assert(KnxWidgetProject(color, KNX_TILE_LIGHT, state));
    assert(!strcmp(state.state,"on") && strstr(state.attributes_json, "\"rgb_color\":[128,64,0]"));
    assert(strstr(state.attributes_json, "\"brightness_pct\":50"));
    assert(KnxWidgetCommandValue(color, KNX_SET_PERCENT, 100, nullptr, command) && command == "255,128,0");
    assert(KnxWidgetCommandValue(color, KNX_SET_RGB, 0x0102fe, nullptr, command) && command == "1,2,254");
    assert(KnxWidgetCommandValue(color, KNX_SET_POWER, 0, nullptr, command) && command == "0,0,0");
    auto dark = feedback("DPT-232.600","0,0,0");
    assert(KnxWidgetCommandValue(dark, KNX_SET_POWER, 1, nullptr, command) && command == "255,255,255");
    color.writable = false;
    assert(!KnxWidgetCommandValue(color, KNX_SET_RGB, 0xffffff, nullptr, command));

    auto relay = feedback("DPT-1.001", "off");
    assert(KnxWidgetProject(relay, KNX_TILE_BINARY, state) && !strcmp(state.state, "off"));
    assert(KnxWidgetCommandValue(relay, KNX_TOGGLE, 0, nullptr, command) && command == "on");
    relay.live_feedback = false;
    assert(!KnxWidgetProject(relay, KNX_TILE_BINARY, state));
    assert(!KnxWidgetCommandValue(relay, KNX_TOGGLE, 0, nullptr, command));
    relay.readable = false;
    assert(KnxWidgetProject(relay, KNX_TILE_ACTION, state) && !strcmp(state.state, "ready"));
    assert(KnxWidgetCommandValue(relay, KNX_SEND_VALUE, 0, "off", command) && command == "off");
}
