// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx_widget_model.h"
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
    object.live_feedback = false;
    assert(!KnxWidgetProject(object, KNX_TILE_VALUE, state));
    auto humidity = feedback("DPT-9.007", "61.75");
    humidity.unit = "%";
    assert(KnxWidgetProject(humidity, KNX_TILE_VALUE, state));
    assert(!strstr(state.attributes_json, "knx_scaling") && !strcmp(state.state, "61.75"));
}

int main() {
    test_scaling_sensor_feedback();
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
