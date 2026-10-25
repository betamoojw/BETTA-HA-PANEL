// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx.h"
#include "knx_config.h"
#include "esp_knx_ip/esp_knx_ip.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
extern "C" {
#include "util/flash_gate.h"
#include "net/wifi_mgr.h"
}
#include <algorithm>
#include <cstring>
#include <mutex>

cJSON* KnxObjectJson(const KnxCommunicationObject& object);
namespace {
std::mutex state_mutex, edit_mutex;
std::vector<KnxCommunicationObject> objects;
std::string configuration, error;
const char* state = "disabled";
bool enabled, valid_config, initialized;
uint32_t generation = 1, active_generation, received_count, sent_count, rejected_count;
knx_address_t physical;
int64_t last_received;
QueueHandle_t commands;
TaskHandle_t worker;
struct Command { uint32_t generation; bool write; char id[49], value[256]; };

bool fail(char* out, size_t size, const char* reason) {
    if (out && size) snprintf(out, size, "%s", reason);
    return false;
}
KnxCommunicationObject* find(const char* id, const char* address = nullptr) {
    for (auto& o : objects)
        if ((id && o.id == id) || (!id && address && o.group_address == address)) return &o;
    return nullptr;
}
void receive(const knx_telegram_t* t, void*) {
    std::lock_guard<std::mutex> guard(state_mutex);
    // Ignore our own multicast loopback: successful send is not bus feedback.
    if (!enabled || generation != active_generation || t->source == physical ||
        (t->command != KNX_COMMAND_RESPONSE && t->command != KNX_COMMAND_WRITE)) return;
    for (auto& o : objects) {
        if (!o.readable || o.parsed_group_address != t->destination) continue;
        KnxValue v;
        if (!KnxDecodeValue(o.datapoint_type, t->data, t->data_length, v)) { rejected_count++; continue; }
        o.current_value = std::move(v); o.valid = true;
        o.last_update_ms = esp_timer_get_time()/1000;
        last_received = o.last_update_ms; received_count++;
    }
}
void run(void*) {
    esp_knx_ip_handle_t handle = nullptr;
    uint32_t bound_ip = 0;
    int64_t retry_at = 0;
    for (;;) {
        auto* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip{};
        bool online = wifi_mgr_is_connected() && netif && esp_netif_is_netif_up(netif) &&
                      esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr;
        bool restart, want;
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            want = enabled && valid_config;
            restart = !want || !online || bound_ip != ip.ip.addr || active_generation != generation;
            if (restart) state = !enabled ? "disabled" : !valid_config ? "error" :
                                 !online ? "waiting_for_network" : "starting";
        }
        // stop waits for the receive callback, so never hold state_mutex here.
        if (handle && restart) {
            esp_knx_ip_stop(handle);
            esp_knx_ip_destroy(handle); handle = nullptr; bound_ip = 0; retry_at = 0;
        }
        if (want && online && !handle && esp_timer_get_time() >= retry_at) {
            esp_err_t result;
            {
                std::lock_guard<std::mutex> guard(state_mutex);
                cJSON* root = cJSON_Parse(configuration.c_str());
                if (!root) {
                    error = "Cannot allocate KNX configuration"; state = "error";
                    retry_at = esp_timer_get_time() + 10000000;
                    continue;
                }
                cJSON* media = cJSON_GetObjectItemCaseSensitive(root, "media_parameters");
                esp_knx_ip_config_t cfg = ESP_KNX_IP_CONFIG_DEFAULT(netif);
                cfg.multicast_address = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(media, "multicast_address"));
                cfg.port = cJSON_GetObjectItemCaseSensitive(media, "udp_port")->valueint;
                KnxParsePhysicalAddress(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "physical_address")), physical);
                cfg.physical_address = physical; cfg.load_physical_address_from_nvs = false;
                result = esp_knx_ip_create(&cfg, &handle);
                cJSON_Delete(root);
                if (result == ESP_OK) for (const auto& o : objects) {
                    result = esp_knx_ip_register_callback(handle, o.parsed_group_address, receive, nullptr);
                    if (result != ESP_OK) break;
                }
                active_generation = generation;
            }
            if (result == ESP_OK) result = esp_knx_ip_start(handle);
            {
                std::lock_guard<std::mutex> guard(state_mutex);
                if (result == ESP_OK) { bound_ip = ip.ip.addr; state = "running"; error.clear(); }
                else { state = "error"; error = esp_err_to_name(result); }
            }
            if (result != ESP_OK) {
                if (handle) esp_knx_ip_destroy(handle);
                handle = nullptr; retry_at = esp_timer_get_time() + 10000000;
            }
        }
        Command c;
        if (xQueueReceive(commands, &c, pdMS_TO_TICKS(200)) == pdTRUE) {
            std::lock_guard<std::mutex> guard(state_mutex);
            auto* o = find(c.id);
            if (!enabled || !handle || generation != c.generation || generation != active_generation ||
                !o || !(c.write ? o->writable : o->readable)) { rejected_count++; continue; }
            std::vector<uint8_t> data(1, 0);
            KnxValue value;
            if (c.write && (!KnxParseValue(o->datapoint_type, c.value, value) ||
                            !KnxEncodeValue(o->datapoint_type, value, data))) { rejected_count++; continue; }
            esp_err_t result = esp_knx_ip_send(handle, o->parsed_group_address,
                c.write ? KNX_COMMAND_WRITE : KNX_COMMAND_READ, data.data(), data.size());
            if (result == ESP_OK) sent_count++;
            else { rejected_count++; error = esp_err_to_name(result); }
        }
    }
}
} // namespace

extern "C" void knx_init(void) {
    std::lock_guard<std::mutex> edit(edit_mutex);
    if (initialized) return;
    std::string json, canonical, why;
    std::vector<KnxCommunicationObject> parsed;
    bool ok = KnxLoadConfiguration(json, why) &&
        KnxParseConfiguration(json, kKnxMaximumObjects, CONFIG_ESP_KNX_IP_MAX_GROUP_ADDRESSES, parsed, canonical, why);
    uint8_t saved = 0; nvs_handle_t h;
    if (nvs_open("knx", NVS_READONLY, &h) == ESP_OK) { nvs_get_u8(h, "enabled", &saved); nvs_close(h); }
    std::lock_guard<std::mutex> guard(state_mutex);
    configuration = ok ? std::move(canonical) : std::move(json);
    objects = std::move(parsed); valid_config = ok; enabled = saved != 0 && CONFIG_LWIP_MAX_SOCKETS >= 24;
    error = why;
    commands = xQueueCreate(8, sizeof(Command));
    if (!commands || xTaskCreate(run, "knx", 6144, nullptr, 3, &worker) != pdPASS) {
        error = "Cannot start KNX worker"; state = "error";
        if (commands) { vQueueDelete(commands); commands = nullptr; }
    } else initialized = true;
    ESP_LOGI("knx", "Configuration: %s, objects=%u, enabled=%d", ok ? "valid" : "invalid",
             static_cast<unsigned>(objects.size()), enabled);
}

extern "C" bool knx_enable(bool value, char* out, size_t size) {
    std::lock_guard<std::mutex> edit(edit_mutex);
    { std::lock_guard<std::mutex> guard(state_mutex);
      if (!initialized || (value && !valid_config)) return fail(out, size, "KNX configuration or worker unavailable");
      if (enabled == value) return true;
    }
    // Old 16-socket sdkconfigs have no spare socket beyond the HTTP reserve.
    if (value && CONFIG_LWIP_MAX_SOCKETS < 24) return fail(out, size, "Rebuild with at least 24 lwIP sockets");
    if (!flash_gate_enter(1000)) return fail(out, size, "Storage busy");
    nvs_handle_t h; esp_err_t result = nvs_open("knx", NVS_READWRITE, &h);
    if (result == ESP_OK) {
        result = nvs_set_u8(h, "enabled", value);
        if (result == ESP_OK) result = nvs_commit(h);
        nvs_close(h);
    }
    flash_gate_leave();
    if (result != ESP_OK) return fail(out, size, esp_err_to_name(result));
    std::lock_guard<std::mutex> guard(state_mutex);
    enabled = value; generation++;
    state = value ? "starting" : "disabled";
    return true;
}

extern "C" bool knx_configure(const char* json, char* out, size_t size) {
    std::lock_guard<std::mutex> edit(edit_mutex);
    std::vector<KnxCommunicationObject> parsed;
    std::string canonical, why;
    if (!json || !KnxParseConfiguration(json, kKnxMaximumObjects, CONFIG_ESP_KNX_IP_MAX_GROUP_ADDRESSES,
                                       parsed, canonical, why) || !KnxSaveConfiguration(canonical, why))
        return fail(out, size, why.c_str());
    std::lock_guard<std::mutex> guard(state_mutex);
    objects = std::move(parsed); configuration = std::move(canonical);
    valid_config = true; generation++; error.clear(); state = enabled ? "starting" : "disabled";
    return true;
}

extern "C" char* knx_configuration(void) {
    std::lock_guard<std::mutex> guard(state_mutex);
    return strdup(configuration.c_str());
}
extern "C" cJSON* knx_status(void) {
    std::lock_guard<std::mutex> guard(state_mutex);
    cJSON* j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "enabled", enabled);
    cJSON_AddBoolToObject(j, "configuration_valid", valid_config);
    cJSON_AddStringToObject(j, "state", state);
    cJSON_AddStringToObject(j, "last_error", error.c_str());
    cJSON_AddNumberToObject(j, "object_count", objects.size());
    cJSON_AddNumberToObject(j, "received", received_count);
    cJSON_AddNumberToObject(j, "sent", sent_count);
    cJSON_AddNumberToObject(j, "rejected", rejected_count);
    cJSON_AddNumberToObject(j, "last_received_ms", last_received);
    cJSON_AddStringToObject(j, "mode", "routing");
    return j;
}
extern "C" cJSON* knx_objects(int offset, int limit) {
    std::lock_guard<std::mutex> guard(state_mutex);
    offset = std::max(0, offset); limit = std::clamp(limit, 1, 20);
    cJSON* j = cJSON_CreateObject(); cJSON* list = cJSON_AddArrayToObject(j, "objects");
    int end = std::min(static_cast<int>(objects.size()), offset + limit);
    for (int i = offset; i < end; i++) cJSON_AddItemToArray(list, KnxObjectJson(objects[i]));
    cJSON_AddBoolToObject(j, "has_more", end < static_cast<int>(objects.size()));
    cJSON_AddNumberToObject(j, "next_offset", end); cJSON_AddStringToObject(j, "state", state);
    return j;
}
extern "C" cJSON* knx_object(const char* id) {
    std::lock_guard<std::mutex> guard(state_mutex);
    auto* o = find(id);
    cJSON* j = o ? KnxObjectJson(*o) : nullptr;
    if (j) cJSON_AddStringToObject(j, "state", state);
    return j;
}
extern "C" bool knx_command(const char* id, const char* address, const char* value,
                            bool write, char* out, size_t size) {
    std::lock_guard<std::mutex> guard(state_mutex);
    if (!enabled || !initialized || strcmp(state, "running") || generation != active_generation)
        return fail(out, size, "KNX routing is not running");
    auto* o = find(id, address);
    if (!o || !(write ? o->writable : o->readable)) return fail(out, size, "Unknown object or operation not permitted");
    Command c{}; c.generation = generation; c.write = write;
    snprintf(c.id, sizeof(c.id), "%s", o->id.c_str());
    if (write) {
        KnxValue v;
        if (!value || strlen(value) >= sizeof(c.value) || !KnxParseValue(o->datapoint_type, value, v))
            return fail(out, size, "Invalid value for configured DPT");
        snprintf(c.value, sizeof(c.value), "%s", value);
    }
    return xQueueSend(commands, &c, 0) == pdTRUE || fail(out, size, "KNX command queue is busy");
}
