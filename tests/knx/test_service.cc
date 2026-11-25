// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
// Execute the production worker deterministically; no sockets, threads or bus.
#include "knx.h"
#include "knx_config.h"
#include "layout/knx_layout_binding.h"
#include <fstream>
#include "esp_knx_ip/esp_knx_ip.h"
#include "freertos/queue.h"
#include "esp_netif.h"
extern "C" {
#include "ui/ui_knx_sync.h"
}
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>
#include <functional>

bool test_online=true, test_storage_failure=false;
uint32_t test_ip=1;
int64_t test_now=1000000;
uint8_t test_enabled=0;
void (*test_worker)(void*);
static size_t command_size;
static std::deque<std::vector<unsigned char>> queue;
static std::function<void()> iteration;
static std::string persisted;
static unsigned starts, stops, sends, registrations;
struct Write { knx_address_t address; std::vector<uint8_t> data; };
static std::vector<Write> writes;
static ui_widget_instance_t tiles[8];
static ha_state_t scratch;
extern "C" void ui_widget_factory_apply_state(ui_widget_instance_t *widget, const ha_state_t *state) {
    assert(!strcmp(widget->entity_id, state->entity_id));
    strcpy(widget->rendered, state->state);
    ++widget->applications;
}
extern "C" void ui_widget_factory_mark_unavailable(ui_widget_instance_t *widget) {
    strcpy(widget->rendered, "unavailable");
}
static void drain() {
    for (unsigned turn = 0; ui_knx_refresh_pending(); ++turn) {
        assert(turn < 5);
        ui_knx_drain(tiles, 8, &scratch);
    }
}
static void check_readable_tiles(const char *expected) {
    for (unsigned i = 0; i < 6; ++i) assert(!strcmp(tiles[i].rendered, expected));
    assert(tiles[6].applications == 0); // HA entity with the same spelling as a KNX ID
    assert(!strcmp(tiles[7].rendered, "ready")); // write-only action
}
static esp_knx_ip_telegram_callback_t callback;
static knx_address_t source;
static bool fail_start;
struct Done {};
QueueHandle_t xQueueCreate(unsigned,size_t size) { command_size=size;return (void*)1; }
void vQueueDelete(QueueHandle_t) {}
int xQueueSend(QueueHandle_t,const void* data,unsigned) {
    if(queue.size()==8) return 0;
    const auto* b=static_cast<const unsigned char*>(data);queue.emplace_back(b,b+command_size);return pdTRUE;
}
int xQueueReceive(QueueHandle_t,void* out,unsigned) {
    test_now+=20000000; iteration();
    if(queue.empty()) return 0;
    memcpy(out,queue.front().data(),command_size);queue.pop_front();return pdTRUE;
}
extern "C" bool wifi_mgr_is_connected(void) { return test_online; }
extern "C" bool net_mgr_is_connected(void) { return test_online; }
extern "C" esp_netif_t *net_mgr_active_netif(void) { return test_online ? esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") : nullptr; }
extern "C" bool flash_gate_enter(uint32_t) { return !test_storage_failure; }
extern "C" void flash_gate_leave(void) {}
bool KnxLoadConfiguration(std::string& text,std::string&) { text=persisted;return true; }
bool KnxSaveConfiguration(const std::string& text,std::string& error) {
    if(test_storage_failure) {error="test write failed";return false;} persisted=text;return true;
}
extern "C" esp_err_t esp_knx_ip_create(const esp_knx_ip_config_t* cfg,esp_knx_ip_handle_t* h) {
    assert(cfg->port==3672 && !strcmp(cfg->multicast_address,"224.0.23.13"));
    assert(!cfg->load_physical_address_from_nvs);
    source=cfg->physical_address;*h=reinterpret_cast<esp_knx_ip_handle_t>(1);return ESP_OK;
}
extern "C" esp_err_t esp_knx_ip_register_callback(esp_knx_ip_handle_t,knx_address_t,esp_knx_ip_telegram_callback_t c,void*) {++registrations;callback=c;return ESP_OK;}
extern "C" esp_err_t esp_knx_ip_start(esp_knx_ip_handle_t) {starts++;return fail_start?ESP_FAIL:ESP_OK;}
extern "C" esp_err_t esp_knx_ip_stop(esp_knx_ip_handle_t) {stops++;return ESP_OK;}
extern "C" esp_err_t esp_knx_ip_destroy(esp_knx_ip_handle_t) {return ESP_OK;}
extern "C" esp_err_t esp_knx_ip_send(esp_knx_ip_handle_t,knx_address_t address,knx_command_t command,const uint8_t* data,size_t n) {
    assert(command == KNX_COMMAND_WRITE);
    writes.push_back({address, {data, data + n}});
    sends++;return ESP_OK;
}
static void status_is(const char* expected) {
    cJSON* s=knx_status();assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(s,"state")),expected));cJSON_Delete(s);
}
static bool cached() {
    cJSON* o=knx_object("lamp");assert(o);bool v=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o,"valid"));cJSON_Delete(o);return v;
}
static void exercise_widget_roles(const char *layout_path, const char *trace_path) {
    const char *registry = R"([{"id":"P","name":"Switch command","group_address":"2/0/1","datapoint_type":"DPT-1.001","readable":false,"writable":true},
      {"id":"S","name":"Switch feedback","group_address":"2/1/1","datapoint_type":"DPT-1.001","readable":true,"writable":false},
      {"id":"A","name":"Brightness command","group_address":"2/0/2","datapoint_type":"DPT-5.001","readable":false,"writable":true},
      {"id":"B","name":"Brightness feedback","group_address":"2/1/2","datapoint_type":"DPT-5.001","readable":true,"writable":false},
      {"id":"AB","name":"Shared level","group_address":"2/2/2","datapoint_type":"DPT-5.001","readable":true,"writable":true}])";
    std::string config = kKnxDefaultConfiguration;
    config.replace(config.find("3671"), 4, "3672");
    config.replace(config.find("224.0.23.12"), 11, "224.0.23.13");
    config.replace(config.find("[]"), 2, registry);
    char error[160];
    assert(knx_configure(config.c_str(), error, sizeof(error)) && knx_enable(true, error, sizeof(error)));
    knx_control_binding_t light{}, slider{}, shared{};
    strcpy(light.id[KNX_SWITCH_COMMAND], "P");
    strcpy(light.id[KNX_SWITCH_FEEDBACK], "S");
    for (auto* binding : {&slider, &light}) {
        strcpy(binding->id[KNX_BRIGHTNESS_COMMAND], "A");
        strcpy(binding->id[KNX_BRIGHTNESS_FEEDBACK], "B");
    }
    strcpy(shared.id[KNX_BRIGHTNESS_COMMAND], "AB");
    strcpy(shared.id[KNX_BRIGHTNESS_FEEDBACK], "AB");
    // An exported editor layout can drive the same worker used by this regression.
    cJSON *trace = trace_path ? cJSON_CreateObject() : nullptr;
    if (layout_path) {
        std::ifstream input(layout_path);
        std::string json{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        cJSON *root = cJSON_Parse(json.c_str());
        const cJSON *pages = cJSON_GetObjectItem(root, "pages"), *page, *widget;
        unsigned matched = 0;
        cJSON_ArrayForEach(page, pages) {
            cJSON_ArrayForEach(widget, cJSON_GetObjectItem(page, "widgets")) {
                const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(widget, "type"));
                const cJSON *binding = cJSON_GetObjectItem(widget, "binding");
                if (!binding || !type || strcmp(type, "slider")) continue;
                knx_layout_binding_t parsed;
                assert(knx_layout_binding(binding, type, &parsed));
                assert(parsed.controls[KNX_BRIGHTNESS_COMMAND] && parsed.controls[KNX_BRIGHTNESS_FEEDBACK]);
                strcpy(slider.id[KNX_BRIGHTNESS_COMMAND], parsed.controls[KNX_BRIGHTNESS_COMMAND]);
                strcpy(slider.id[KNX_BRIGHTNESS_FEEDBACK], parsed.controls[KNX_BRIGHTNESS_FEEDBACK]);
                ++matched;
            }
        }
        assert(matched == 1);
        cJSON_Delete(root);
    }
    uint32_t light_cursor = 0, slider_cursor = 0, shared_cursor = 0;
    auto read = [](knx_control_binding_t& binding, knx_tile_kind_t kind, uint32_t& cursor, const char* expected) {
        assert(knx_widget_read("knx:test", kind, nullptr, &binding, &cursor, &scratch) == KNX_VIEW_UPDATED);
        assert(!strcmp(scratch.state, expected));
    };
    auto telegram = [](unsigned middle, unsigned sub, uint8_t value, bool loopback, knx_command_t command) {
        knx_telegram_t t{};
        t.source = loopback ? source : 0x1102;
        t.destination = knx_group_address(2, middle, sub);
        t.command = command;
        t.data_length = sub == 1 ? 1 : 2;
        t.data[t.data_length - 1] = value;
        callback(&t, nullptr);
    };
    auto sent = [](unsigned middle, unsigned sub, uint8_t payload) {
        assert(writes.back().address == knx_group_address(2, middle, sub));
        std::vector<uint8_t> data = sub == 1 ? std::vector<uint8_t>{payload} : std::vector<uint8_t>{0,payload};
        assert(writes.back().data == data);
    };
    unsigned step = 0;
    const size_t before = writes.size();
    iteration = [&] {
        switch (step++) {
        case 0:
            read(slider, KNX_TILE_SLIDER, slider_cursor, "unavailable");
            read(light, KNX_TILE_LIGHT, light_cursor, "unavailable");
            assert(light.switch_writable && slider.brightness_writable && light.brightness == -1);
            assert(knx_widget_control_roles(&light, KNX_TOGGLE, 0) != ESP_OK);
            if (trace) cJSON_AddItemToObject(trace, "before", knx_objects(0, 20));
            assert(knx_widget_control_roles(&slider, KNX_SET_PERCENT, 80) == ESP_OK);
            break;
        case 1:
            sent(0, 2, 204);
            telegram(0, 2, 204, false, KNX_COMMAND_WRITE);
            telegram(1, 2, 204, true, KNX_COMMAND_WRITE);
            assert(knx_widget_read("knx:test", KNX_TILE_SLIDER, nullptr, &slider, &slider_cursor, &scratch) == KNX_VIEW_UNCHANGED);
            if (trace) cJSON_AddItemToObject(trace, "loopback", knx_objects(0, 20));
            telegram(1, 2, 158, false, KNX_COMMAND_RESPONSE);
            read(slider, KNX_TILE_SLIDER, slider_cursor, "62");
            if (trace) {
                cJSON_AddItemToObject(trace, "feedback", knx_objects(0, 20));
                cJSON_AddStringToObject(trace, "slider_state", scratch.state);
            }
            read(light, KNX_TILE_LIGHT, light_cursor, "unavailable");
            assert(light.brightness == 62);
            {
                cJSON *page = knx_objects(0, 20);
                const cJSON *b = cJSON_GetArrayItem(cJSON_GetObjectItem(page,"objects"),3);
                assert(cJSON_GetNumberValue(cJSON_GetObjectItem(b,"value")) == 61.960784912109375);
                cJSON_Delete(page);
            }
            assert(knx_widget_control_roles(&light, KNX_SET_POWER, 1) == ESP_OK);
            break;
        case 2:
            sent(0, 1, 1);
            assert(knx_widget_control_roles(&light, KNX_SET_POWER, 0) == ESP_OK);
            break;
        case 3:
            sent(0, 1, 0);
            telegram(1, 1, 0, false, KNX_COMMAND_WRITE);
            read(light, KNX_TILE_LIGHT, light_cursor, "off");
            assert(light.brightness == 62);
            assert(knx_widget_control_roles(&light, KNX_TOGGLE, 0) == ESP_OK);
            break;
        case 4:
            sent(0, 1, 1);
            assert(knx_widget_control_roles(&light, KNX_SET_PERCENT, 40) == ESP_OK);
            break;
        case 5:
            sent(0, 2, 102);
            telegram(1, 2, 255, false, KNX_COMMAND_WRITE);
            read(light, KNX_TILE_LIGHT, light_cursor, "off");
            assert(light.brightness == 100);
            assert(knx_widget_control_roles(&shared, KNX_SET_PERCENT, 80) == ESP_OK);
            break;
        case 6:
            sent(2, 2, 204);
            telegram(2, 2, 204, true, KNX_COMMAND_WRITE);
            read(shared, KNX_TILE_SLIDER, shared_cursor, "unavailable");
            telegram(2, 2, 51, false, KNX_COMMAND_WRITE);
            read(shared, KNX_TILE_SLIDER, shared_cursor, "20");
            strcpy(shared.id[KNX_BRIGHTNESS_FEEDBACK], "B");
            shared_cursor = 0;
            read(shared, KNX_TILE_SLIDER, shared_cursor, "100");
            assert(knx_widget_control_roles(&shared, KNX_SET_PERCENT, 0) == ESP_OK);
            break;
        case 7: {
            sent(2, 2, 0);
            assert(writes.size() == before + 7);
            for (const char *id : {"P", "B", "removed"}) {
                strcpy(slider.id[KNX_BRIGHTNESS_COMMAND], id);
                assert(knx_widget_control_roles(&slider, KNX_SET_PERCENT, 30) != ESP_OK);
            }
            for (const char *id : {"P", "B", "removed"}) {
                strcpy(light.id[KNX_SWITCH_FEEDBACK], id);
                assert(knx_widget_control_roles(&light, KNX_TOGGLE, 0) != ESP_OK);
            }
            strcpy(light.id[KNX_SWITCH_FEEDBACK], "S");
            light.id[KNX_BRIGHTNESS_COMMAND][0] = light.id[KNX_BRIGHTNESS_FEEDBACK][0] = 0;
            light_cursor = 0;
            read(light, KNX_TILE_LIGHT, light_cursor, "off");
            assert(light.brightness == -1 && !light.brightness_writable);
            assert(knx_widget_control_roles(&light, KNX_TOGGLE, 0) == ESP_OK);
            break;
        }
        default:
            sent(0, 1, 1);
            assert(writes.size() == before + 8);
            throw Done{};
        }
    };
    try { test_worker(nullptr); } catch (const Done&) {}
    if (trace) {
        char *json = cJSON_PrintUnformatted(trace);
        std::ofstream output(trace_path, std::ios::binary);
        output << json;
        assert(output.good());
        cJSON_free(json);
        cJSON_Delete(trace);
    }
}

static void exercise_composite_widgets() {
    const char* registry = R"([
      {"id":"M","name":"Move","group_address":"3/0/1","datapoint_type":"DPT-1.008","readable":false,"writable":true},
      {"id":"STOP","name":"Stop","group_address":"3/0/2","datapoint_type":"DPT-1.010","readable":false,"writable":true},
      {"id":"BREAK","name":"Break","group_address":"3/0/3","datapoint_type":"DPT-3.008","readable":false,"writable":true},
      {"id":"P","name":"Position command","group_address":"3/0/4","datapoint_type":"DPT-5.001","readable":false,"writable":true},
      {"id":"PF","name":"Position feedback","group_address":"3/1/4","datapoint_type":"DPT-5.001","readable":true,"writable":false},
      {"id":"T","name":"Target command","group_address":"3/0/5","datapoint_type":"DPT-9.001","readable":false,"writable":true},
      {"id":"TF","name":"Target feedback","group_address":"3/1/5","datapoint_type":"DPT-9.001","readable":true,"writable":false},
      {"id":"CF","name":"Current temperature","group_address":"3/1/6","datapoint_type":"DPT-9.001","readable":true,"writable":false},
      {"id":"CFA","name":"Temperature alias","group_address":"3/1/6","datapoint_type":"DPT-9.001","readable":true,"writable":false},
      {"id":"MODE","name":"Mode command","group_address":"3/0/7","datapoint_type":"DPT-20.105","readable":false,"writable":true},
      {"id":"MF","name":"Mode feedback","group_address":"3/1/7","datapoint_type":"DPT-20.105","readable":true,"writable":false},
      {"id":"POWER","name":"Power command","group_address":"3/0/8","datapoint_type":"DPT-1.001","readable":false,"writable":true},
      {"id":"SF","name":"Power feedback","group_address":"3/1/8","datapoint_type":"DPT-1.001","readable":true,"writable":false},
      {"id":"SPEED","name":"Speed command","group_address":"3/0/9","datapoint_type":"DPT-5.001","readable":false,"writable":true},
      {"id":"FF","name":"Speed feedback","group_address":"3/1/9","datapoint_type":"DPT-5.001","readable":true,"writable":false},
      {"id":"PRESET","name":"Operating preset","group_address":"3/0/10","datapoint_type":"DPT-20.102","readable":false,"writable":true}])";
    std::string config = kKnxDefaultConfiguration;
    config.replace(config.find("3671"), 4, "3672");
    config.replace(config.find("224.0.23.12"), 11, "224.0.23.13");
    config.replace(config.find("[]"), 2, registry);
    char error[160];
    assert(knx_configure(config.c_str(), error, sizeof(error)) && knx_enable(true, error, sizeof(error)));
    knx_control_binding_t cover{}, position{}, climate{}, fan{};
    strcpy(cover.id[KNX_MOVE_COMMAND], "M"); strcpy(cover.id[KNX_STOP_COMMAND], "STOP");
    strcpy(cover.id[KNX_POSITION_FEEDBACK], "PF");
    strcpy(position.id[KNX_POSITION_COMMAND], "P"); strcpy(position.id[KNX_POSITION_FEEDBACK], "PF");
    strcpy(climate.id[KNX_TARGET_TEMPERATURE_COMMAND], "T");
    strcpy(climate.id[KNX_TARGET_TEMPERATURE_FEEDBACK], "TF");
    strcpy(climate.id[KNX_CURRENT_TEMPERATURE_FEEDBACK], "CF");
    strcpy(climate.id[KNX_HVAC_MODE_COMMAND], "MODE"); strcpy(climate.id[KNX_HVAC_MODE_FEEDBACK], "MF");
    strcpy(fan.id[KNX_SWITCH_COMMAND], "POWER"); strcpy(fan.id[KNX_SWITCH_FEEDBACK], "SF");
    strcpy(fan.id[KNX_FAN_SPEED_COMMAND], "SPEED"); strcpy(fan.id[KNX_FAN_SPEED_FEEDBACK], "FF");
    uint32_t cover_cursor = 0, position_cursor = 0, climate_cursor = 0, fan_cursor = 0;
    auto read = [](knx_control_binding_t& binding, knx_tile_kind_t kind, uint32_t& cursor, const char* expected) {
        assert(knx_widget_read("knx:composite", kind, nullptr, &binding, &cursor, &scratch) == KNX_VIEW_UPDATED);
        assert(!strcmp(scratch.state, expected));
    };
    auto attribute = [](const char* key, double expected) {
        cJSON* attrs = cJSON_Parse(scratch.attributes_json);
        const cJSON* value = cJSON_GetObjectItemCaseSensitive(attrs, key);
        assert(cJSON_IsNumber(value) && value->valuedouble == expected);
        cJSON_Delete(attrs);
    };
    auto absent = [](const char* key) {
        cJSON* attrs = cJSON_Parse(scratch.attributes_json);
        assert(attrs && !cJSON_HasObjectItem(attrs, key)); cJSON_Delete(attrs);
    };
    auto publish = [](unsigned sub, std::initializer_list<uint8_t> data, bool loopback = false) {
        knx_telegram_t t{};
        t.source = loopback ? source : 0x1102;
        t.destination = knx_group_address(3, 1, sub);
        t.command = KNX_COMMAND_RESPONSE;
        t.data_length = data.size();
        std::copy(data.begin(), data.end(), t.data);
        callback(&t, nullptr);
    };
    struct CommandCase { std::function<esp_err_t()> send; unsigned sub; std::vector<uint8_t> payload; };
    const std::vector<CommandCase> commands = {
        {[&] { return knx_widget_control_roles(&cover, KNX_COVER_OPEN, 0); }, 1, {0}},
        {[&] { return knx_widget_control_roles(&cover, KNX_COVER_CLOSE, 0); }, 1, {1}},
        {[&] { return knx_widget_control_roles(&cover, KNX_COVER_STOP, 0); }, 2, {0}},
        {[&] { strcpy(cover.id[KNX_STOP_COMMAND], "BREAK"); return knx_widget_control_roles(&cover, KNX_COVER_STOP, 0); }, 3, {0}},
        {[&] { return knx_widget_control_roles(&position, KNX_COVER_OPEN, 0); }, 4, {0, 0}},
        {[&] { return knx_widget_control_roles(&position, KNX_COVER_CLOSE, 0); }, 4, {0, 255}},
        {[&] { position.position_open_at_100 = true; return knx_widget_control_roles(&position, KNX_COVER_OPEN, 0); }, 4, {0, 255}},
        {[&] { return knx_widget_control_roles(&position, KNX_COVER_CLOSE, 0); }, 4, {0, 0}},
        {[&] { return knx_widget_control_roles(&climate, KNX_SET_TARGET_TEMPERATURE, 215); }, 5, {0, 0x0c, 0x33}},
        {[&] { return knx_widget_control_roles(&climate, KNX_SET_HVAC_MODE, 1); }, 7, {0, 1}},
        {[&] { return knx_widget_control_roles(&climate, KNX_SET_HVAC_MODE, 6); }, 7, {0, 6}},
        {[&] { return knx_widget_control_roles(&climate, KNX_SET_HVAC_MODE, 14); }, 7, {0, 14}},
        {[&] { return knx_widget_control_roles(&fan, KNX_SET_FAN_PERCENT, 0); }, 9, {0, 0}},
        {[&] { return knx_widget_control_roles(&fan, KNX_SET_FAN_PERCENT, 100); }, 9, {0, 255}},
        {[&] { return knx_widget_control_roles(&fan, KNX_SET_POWER, 1); }, 8, {1}},
    };
    size_t next = 0, before = writes.size();
    enum { Sending, Offline, Reconnected } phase = Sending;
    iteration = [&] {
        if (phase == Sending) {
            if (next == 0) {
                read(cover, KNX_TILE_COVER, cover_cursor, "unknown"); absent("current_position");
                assert(knx_control_can_write(&cover, KNX_MOVE_COMMAND));
                read(climate, KNX_TILE_CLIMATE, climate_cursor, "unknown"); absent("temperature"); absent("current_temperature");
                assert(knx_control_can_write(&climate, KNX_HVAC_MODE_COMMAND));
                read(fan, KNX_TILE_FAN, fan_cursor, "unknown"); absent("percentage");
                assert(knx_widget_control_roles(&fan, KNX_TOGGLE, 0) != ESP_OK);
                assert(knx_widget_control_roles(&position, KNX_COVER_STOP, 0) != ESP_OK);
                assert(knx_widget_control_roles(&climate, KNX_SET_HVAC_MODE, 2) != ESP_OK);
            } else {
                assert(writes.size() == before + next);
                assert(writes.back().address == knx_group_address(3, 0, commands[next - 1].sub));
                assert(writes.back().data == commands[next - 1].payload);
                // A successful command never supplies missing feedback.
                assert(knx_widget_read("knx:composite", KNX_TILE_FAN, nullptr, &fan, &fan_cursor, &scratch) == KNX_VIEW_UNCHANGED);
            }
            if (next < commands.size()) { assert(commands[next++].send() == ESP_OK); return; }
            publish(4, {0, 0}, true);
            assert(knx_widget_read("knx:composite", KNX_TILE_COVER, nullptr, &cover, &cover_cursor, &scratch) == KNX_VIEW_UNCHANGED);
            for (const auto& sample : {std::pair<int, int>{0, 100}, {128, 50}, {255, 0}}) {
                publish(4, {0, static_cast<uint8_t>(sample.first)});
                read(cover, KNX_TILE_COVER, cover_cursor, sample.second ? "open" : "closed");
                attribute("current_position", sample.second);
            }
            read(position, KNX_TILE_COVER, position_cursor, "open"); attribute("current_position", 100);
            publish(5, {0, 0x0c, 0x65}); publish(6, {0, 0x0c, 0x33}); publish(7, {0, 3});
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "cool");
            attribute("temperature", 22.5); attribute("current_temperature", 21.5);
            publish(6, {0, 0x7f}); // malformed temperature keeps the preceding good observation
            assert(knx_widget_read("knx:composite", KNX_TILE_CLIMATE, nullptr, &climate, &climate_cursor, &scratch) == KNX_VIEW_UNCHANGED);
            publish(6, {0, 0x7f, 0xff}); // explicit invalid marker revokes all aliases
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "cool"); absent("current_temperature"); attribute("temperature", 22.5);
            for (const char* id : {"CF", "CFA"}) {
                cJSON* o = knx_object(id);
                assert(cJSON_IsFalse(cJSON_GetObjectItem(o, "valid")) && cJSON_IsFalse(cJSON_GetObjectItem(o, "live_feedback")));
                assert(!cJSON_HasObjectItem(o, "value")); cJSON_Delete(o);
            }
            publish(5, {0, 0x7f, 0xff});
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "cool"); absent("temperature");
            publish(6, {0, 0x0c, 0x33});
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "cool"); attribute("current_temperature", 21.5);
            publish(7, {0, 2}); // specified but unsupported HVAC control mode is never reinterpreted
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "unknown"); attribute("current_temperature", 21.5);
            publish(8, {0}); publish(9, {0, 128});
            read(fan, KNX_TILE_FAN, fan_cursor, "off"); attribute("percentage", 50); attribute("percentage_step", 25);
            strcpy(climate.id[KNX_HVAC_MODE_COMMAND], "PRESET");
            assert(knx_widget_control_roles(&climate, KNX_SET_HVAC_MODE, 3) != ESP_OK);
            strcpy(climate.id[KNX_HVAC_MODE_COMMAND], "MODE");
            strcpy(fan.id[KNX_FAN_SPEED_COMMAND], "FF");
            assert(knx_widget_control_roles(&fan, KNX_SET_FAN_PERCENT, 50) != ESP_OK);
            strcpy(fan.id[KNX_FAN_SPEED_COMMAND], "SPEED");
            test_online = false; phase = Offline;
        } else if (phase == Offline) {
            read(cover, KNX_TILE_COVER, cover_cursor, "unavailable"); absent("current_position");
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "unavailable"); absent("current_temperature");
            read(fan, KNX_TILE_FAN, fan_cursor, "unavailable"); absent("percentage");
            assert(!cover.writable_roles && !climate.writable_roles && !fan.writable_roles);
            assert(knx_widget_control_roles(&fan, KNX_SET_FAN_PERCENT, 50) != ESP_OK);
            test_online = true; phase = Reconnected;
        } else {
            read(cover, KNX_TILE_COVER, cover_cursor, "unknown"); absent("current_position");
            read(climate, KNX_TILE_CLIMATE, climate_cursor, "unknown"); absent("current_temperature"); absent("temperature");
            read(fan, KNX_TILE_FAN, fan_cursor, "unknown"); absent("percentage");
            assert(knx_control_can_write(&fan, KNX_FAN_SPEED_COMMAND));
            assert(knx_widget_control_roles(&fan, KNX_TOGGLE, 0) != ESP_OK);
            assert(writes.size() == before + commands.size());
            throw Done{};
        }
    };
    try { test_worker(nullptr); } catch (const Done&) {}
}

int main(int argc, char **argv) {
    assert(argc == 1 || argc == 3);
    persisted=kKnxDefaultConfiguration;
    persisted.replace(persisted.find("3671"),4,"3672");
    persisted.replace(persisted.find("224.0.23.12"),11,"224.0.23.13");
    persisted.replace(persisted.find("[]"),2,R"([{"id":"lamp","name":"Light","group_address":"1/0/1","datapoint_type":"DPT-1.001","readable":true,"writable":true}])");
    persisted.insert(persisted.rfind(']'), R"(,{"id":"switch.lamp","name":"Feedback","group_address":"1/0/1","datapoint_type":"DPT-1.001","readable":true,"writable":false},{"id":"pulse","name":"Pulse","group_address":"1/0/1","datapoint_type":"DPT-1.001","readable":false,"writable":true})");
    for (unsigned i = 0; i < 8; ++i) {
        strcpy(tiles[i].entity_id, i == 7 ? "knx:pulse" : i == 6 ? "switch.lamp" :
               i % 2 ? "knx:switch.lamp" : "knx:lamp");
        strcpy(tiles[i].type, i == 7 ? "scene_tile" : i % 2 ? "sensor" : "button");
    }
    tiles[7].knx_write_value = "on";
    ui_knx_layout_changed();
    knx_init();assert(test_worker);char error[200];
    assert(!knx_command("lamp",nullptr,"on",true,error,sizeof(error)));assert(starts==0);
    assert(knx_enable(true,error,sizeof(error)));
    const std::string site_config = persisted;
    auto publish = [](knx_address_t address, bool on) {
        knx_telegram_t telegram{};
        telegram.source = 0x1102;
        telegram.destination = address;
        telegram.command = KNX_COMMAND_WRITE;
        telegram.data_length = 1;
        telegram.data[0] = on;
        callback(&telegram, nullptr);
        drain();
    };
    int step=0;
    iteration=[&] {
        switch(step++) {
        case 0: {
            status_is("running");assert(!cached());
            assert(!knx_command("missing",nullptr,"on",true,error,sizeof(error)));
            assert(!knx_command("lamp",nullptr,"invalid",true,error,sizeof(error)));
            assert(registrations == 1); // three aliases share one component callback
            drain(); check_readable_tiles("unavailable");
            assert(knx_widget_control("knx:lamp", KNX_TOGGLE, 0, nullptr) != ESP_OK);
            assert(!knx_command(nullptr,"1/0/1","on",true,error,sizeof(error)));
            knx_telegram_t t{};t.source=source;t.destination=knx_group_address(1,0,1);
            t.command=KNX_COMMAND_RESPONSE;t.data_length=1;t.data[0]=1;
            callback(&t,nullptr);assert(!cached()); // ignore multicast loopback
            t.source=0x1102;
            t.command=KNX_COMMAND_READ; callback(&t,nullptr); assert(!cached());
            t.command=KNX_COMMAND_WRITE; t.destination++; callback(&t,nullptr); assert(!cached());
            t.destination--; t.data_length=2; callback(&t,nullptr); assert(!cached());
            t.data_length=1; callback(&t,nullptr); assert(cached());
            assert(ui_knx_refresh_pending());
            ui_knx_drain(tiles,8,&scratch);
            assert(!strcmp(tiles[0].rendered,"on") && !strcmp(tiles[4].rendered,"unavailable"));
            drain(); check_readable_tiles("on");
            t.command=KNX_COMMAND_RESPONSE; t.data[0]=0; callback(&t,nullptr);
            ui_knx_drain(tiles,8,&scratch);
            t.data[0]=1; callback(&t,nullptr); // new feedback while the first scan is incomplete
            drain(); check_readable_tiles("on");
            t.data[0]=0; callback(&t,nullptr); drain(); check_readable_tiles("off");
            assert(knx_widget_control("knx:switch.lamp",KNX_SET_POWER,1,nullptr) != ESP_OK);
            assert(knx_widget_control("knx:pulse",KNX_SEND_VALUE,0,"bad") != ESP_OK);
            assert(knx_widget_control("knx:lamp",KNX_TOGGLE,0,nullptr) == ESP_OK);
            assert(!ui_knx_refresh_pending()); check_readable_tiles("off");
            cJSON *pulse = knx_object("pulse");
            assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(pulse,"valid"))); cJSON_Delete(pulse);
            std::string before=persisted;
            assert(!knx_configure("[]",error,sizeof(error)));assert(persisted==before);
            test_storage_failure=true;
            assert(!knx_configure(before.c_str(),error,sizeof(error)));assert(persisted==before);
            assert(!knx_enable(false,error,sizeof(error)));status_is("running");
            test_storage_failure=false;break;
        }
        case 1:
            assert(sends==1 && cached());
            assert(writes.back().address == knx_group_address(1,0,1) && writes.back().data == std::vector<uint8_t>{1});
            drain(); check_readable_tiles("off"); // transport success alone never moves the switch
            assert(knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            assert(knx_enable(false,error,sizeof(error)));break;
        case 2:
            status_is("disabled");assert(stops==1 && sends==1); // queued command revoked
            assert(!knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            assert(knx_enable(true,error,sizeof(error)));break;
        case 3:
            status_is("running");assert(starts==2);
            drain(); check_readable_tiles("unavailable");
            assert(knx_widget_control("knx:pulse", KNX_SEND_VALUE, 0, "on") == ESP_OK);
            test_online=false;break;
        case 4:
            status_is("waiting_for_network");assert(stops==2);
            test_online=true;test_ip=2;break;
        case 5: {
            status_is("running");assert(starts==3);
            assert(knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            std::string updated=persisted;updated.replace(updated.find("\"writable\":true"),15,"\"writable\":false");
            assert(knx_configure(updated.c_str(),error,sizeof(error)));break;
        }
        case 6:
            status_is("running");assert(starts==4 && stops==3 && sends==2 && !cached());
            assert(!knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            test_ip=3;fail_start=true;break;
        case 7:
            status_is("error");fail_start=false;break;
        case 8: {
            status_is("running");
            std::string renamed = site_config;
            renamed.replace(renamed.find("switch.lamp"), strlen("switch.lamp"), "other");
            assert(knx_configure(renamed.c_str(), error, sizeof(error)));
            break;
        }
        case 9: {
            publish(knx_group_address(1,0,1), true);
            for (unsigned i = 0; i < 6; ++i)
                assert(!strcmp(tiles[i].rendered, i % 2 ? "unavailable" : "on"));
            std::string restored = site_config;
            const size_t alias = restored.find("switch.lamp");
            restored.replace(restored.find("1/0/1", alias), strlen("1/0/1"), "1/0/8");
            assert(knx_configure(restored.c_str(), error, sizeof(error)));
            break;
        }
        case 10:
            publish(knx_group_address(1,0,1), false);
            publish(knx_group_address(1,0,8), true);
            for (unsigned i = 0; i < 6; ++i)
                assert(!strcmp(tiles[i].rendered, i % 2 ? "on" : "off"));
            assert(tiles[6].applications == 0);
            assert(knx_enable(false,error,sizeof(error)));break;
        default:
            status_is("disabled");assert(sends==2);throw Done{};
        }
    };
    try {test_worker(nullptr);} catch(const Done&) {}
    exercise_widget_roles(argc == 3 ? argv[1] : nullptr, argc == 3 ? argv[2] : nullptr);
    exercise_composite_widgets();
    puts("KNX worker: enable/disable, permissions, generation revocation, feedback, storage failure, reconnect and retry passed");
}
