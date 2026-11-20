// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx.h"
#include "knx_config.h"
#include "knx_widget_model.h"
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
#include "net/net_mgr.h"
}
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

cJSON* KnxObjectJson(const KnxCommunicationObject& object);
namespace {
std::mutex state_mutex, edit_mutex;
std::vector<KnxCommunicationObject> objects;
std::string configuration, error;
const char* state = "disabled";
bool enabled, valid_config, initialized;
std::atomic<uint32_t> view_revision{1};
uint32_t registry_revision = 1;
uint32_t generation = 1, active_generation, received_count, sent_count, rejected_count;
knx_address_t physical;
int64_t last_received;
QueueHandle_t commands;
TaskHandle_t worker;
struct Command { uint32_t generation; bool write; char id[49], value[256]; };

// Writers hold state_mutex. A zero cursor denotes a widget's first observation.
uint32_t next_view_revision() {
    uint32_t next = view_revision.load() + 1;
    if (next == 0) next = 1;
    view_revision.store(next);
    return next;
}
// Called under state_mutex. Cached commissioning values keep their timestamps.
// Widgets require fresh feedback after the routing connection changes.
void invalidate_views() {
    registry_revision = next_view_revision();
    for (auto& o : objects) {
        o.widget_revision = registry_revision;
        o.live_feedback = false;
    }
}
void set_state(const char* next) {
    if (strcmp(state, next) && (!strcmp(state, "running") || !strcmp(next, "running"))) {
        if (!strcmp(next, "running")) {
            // A callback can run inside start(), before it returns.
            registry_revision = next_view_revision();
            for (auto& o : objects) o.widget_revision = registry_revision;
        } else invalidate_views();
    }
    state = next;
}
bool fail(char* out, size_t size, const char* reason) {
    if (out && size) snprintf(out, size, "%s", reason);
    return false;
}
KnxCommunicationObject* find(const char* id) {
    if (id) for (auto& o : objects) if (o.id == id) return &o;
    return nullptr;
}
void receive(const knx_telegram_t* t, void*) {
    std::lock_guard<std::mutex> guard(state_mutex);
    // Ignore our own multicast loopback: successful send is not bus feedback.
    if (!enabled || generation != active_generation || t->source == physical ||
        (t->command != KNX_COMMAND_RESPONSE && t->command != KNX_COMMAND_WRITE)) return;
    KnxValue value;
    uint32_t revision = 0;
    int64_t received_ms = 0;
    for (auto& o : objects) {
        if (!o.readable || o.parsed_group_address != t->destination) continue;
        // Configuration enforces a single DPT per address. Decode once and
        // publish the same observation to all readable aliases under the lock.
        if (!revision) {
            if (!KnxDecodeValue(o.datapoint_type, t->data, t->data_length, value)) {
                ++rejected_count;
                return;
            }
            revision = next_view_revision();
            received_ms = esp_timer_get_time() / 1000;
            last_received = received_ms;
            ++received_count;
        }
        o.current_value = value;
        o.valid = true;
        o.live_feedback = true;
        o.widget_revision = revision;
        o.last_update_ms = received_ms;
    }
}
void run(void*) {
    esp_knx_ip_handle_t handle = nullptr;
    uint32_t bound_ip = 0;
    esp_netif_t *bound_netif = nullptr;
    int64_t retry_at = 0;
    for (;;) {
        auto* netif = net_mgr_active_netif();
        esp_netif_ip_info_t ip{};
        bool online = net_mgr_is_connected() && netif && esp_netif_is_netif_up(netif) &&
                      esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr;
        bool restart, want;
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            want = enabled && valid_config;
            restart = !want || !online || bound_ip != ip.ip.addr || bound_netif != netif || active_generation != generation;
            if (restart) set_state(!enabled ? "disabled" : !valid_config ? "error" :
                                 !online ? "waiting_for_network" : "starting");
            if (handle && restart) active_generation = 0;
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
                if (result == ESP_OK) for (auto it = objects.begin(); it != objects.end(); ++it) {
                    if (std::any_of(objects.begin(), it, [&](const auto& other) {
                        return other.parsed_group_address == it->parsed_group_address;
                    })) continue;
                    result = esp_knx_ip_register_callback(handle, it->parsed_group_address, receive, nullptr);
                    if (result != ESP_OK) break;
                }
                active_generation = generation;
            }
            if (result == ESP_OK) result = esp_knx_ip_start(handle);
            {
                std::lock_guard<std::mutex> guard(state_mutex);
                if (result == ESP_OK) {
                    bound_ip = ip.ip.addr; bound_netif = netif;
                    if (enabled && generation == active_generation) { set_state("running"); error.clear(); }
                } else { active_generation = 0; invalidate_views(); set_state("error"); error = esp_err_to_name(result); }
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
    invalidate_views();
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
    set_state(value ? "starting" : "disabled");
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
    valid_config = true; generation++; error.clear(); set_state(enabled ? "starting" : "disabled");
    invalidate_views();
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
    cJSON_AddNumberToObject(j, "generation", generation);
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
static bool queue_command(KnxCommunicationObject* o, const char* value, bool write, char* out, size_t size) {
    if (!enabled || !initialized || strcmp(state, "running") || generation != active_generation)
        return fail(out, size, "KNX routing is not running");
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

extern "C" bool knx_command(const char* id, const char* address, const char* value,
                            bool write, char* out, size_t size) {
    std::lock_guard<std::mutex> guard(state_mutex);
    auto* object = find(id);
    if (!id && address) for (auto& o : objects) {
        if (o.group_address != address || !(write ? o.writable : o.readable)) continue;
        if (object) return fail(out, size, "Multiple objects match this address; select an object ID");
        object = &o;
    }
    return queue_command(object, value, write, out, size);
}

extern "C" uint32_t knx_widget_revision(void) { return view_revision.load(); }

extern "C" knx_view_result_t knx_widget_read(const char* key, knx_tile_kind_t kind,
                                              const char* preset, uint32_t* cursor, ha_state_t* out) {
    std::unique_lock<std::mutex> guard(state_mutex, std::try_to_lock);
    if (!guard.owns_lock()) return KNX_VIEW_BUSY;
    auto* o = find(knx_binding_id(key));
    const uint32_t revision = o ? o->widget_revision : registry_revision;
    if (*cursor == revision) return KNX_VIEW_UNCHANGED;
    memset(out, 0, sizeof(*out));
    snprintf(out->entity_id, sizeof(out->entity_id), "%s", key);
    KnxValue parsed;
    if (!o || !enabled || generation != active_generation || strcmp(state, "running") ||
        (kind == KNX_TILE_ACTION && (!preset || !KnxParseValue(o->datapoint_type, preset, parsed))) ||
        !KnxWidgetProject(*o, kind, *out)) {
        out->attributes_json[0] = '\0';
        strcpy(out->state, "unavailable");
    }
    *cursor = revision;
    return KNX_VIEW_UPDATED;
}

extern "C" esp_err_t knx_widget_control(const char* key, knx_widget_action_t action, int value, const char* preset) {
    std::unique_lock<std::mutex> guard(state_mutex, std::try_to_lock);
    if (!guard.owns_lock()) return ESP_ERR_TIMEOUT;
    auto* o = find(knx_binding_id(key));
    std::string text;
    if (!o || !KnxWidgetCommandValue(*o, action, value, preset, text)) return ESP_ERR_INVALID_ARG;
    return queue_command(o, text.c_str(), true, nullptr, 0) ? ESP_OK : ESP_FAIL;
}
