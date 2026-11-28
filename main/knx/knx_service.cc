// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723

/** @file
 * @brief Registry lifecycle, routing worker, command queue and widget snapshots.
 */
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "knx.h"
#include "knx_config.h"
#include "knx_route.h"
#include "knx_widget_model.h"
#include "nvs.h"
extern "C"
{
#include "net/net_mgr.h"
#include "net/wifi_mgr.h"
#include "util/flash_gate.h"
}
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <numeric>

cJSON *KnxObjectJson(const KnxCommunicationObject &object);

namespace
{
std::mutex state_mutex, edit_mutex;
std::vector<KnxCommunicationObject> objects;
/** @brief Indices sorted by address for dispatch without changing registry order for HTTP or MCP. */
std::vector<uint16_t> by_address;
KnxRoutingParameters route;
std::string configuration, error;
const char *state = "disabled";
bool enabled, valid_config, initialized;
std::atomic<uint32_t> view_revision {1};
uint32_t registry_revision = 1;
uint32_t generation = 1, active_generation, received_count, sent_count, rejected_count;
uint32_t routing_epoch;
int64_t last_received;
QueueHandle_t commands;
TaskHandle_t worker;

struct Command
{
    uint32_t epoch;
    bool write, toggle;
    char id[49], value[256];
};

// Writers hold state_mutex. A zero cursor denotes a widget's first observation.
uint32_t next_view_revision()
{
    uint32_t next = view_revision.load() + 1;
    if (next == 0)
    {
        next = 1;
    }
    view_revision.store(next);
    return next;
}

/**
 * @brief Revoke widget feedback and invalidate queued commands for the previous routing epoch.
 * Cached commissioning values keep their timestamps.
 * @pre The caller holds state_mutex.
 */
void invalidate_views()
{
    ++routing_epoch;
    registry_revision = next_view_revision();
    for (auto &o : objects)
    {
        o.widget_revision = registry_revision;
        o.live_feedback = false;
        o.command_revision = 0;
    }
}

// Only scalar DPTs used by paired controls need a command display cache.
bool cacheable_command(const KnxCommunicationObject &o)
{
    const auto &d = o.datapoint_type;
    return o.writable &&
           (d.main == 1 || (d.has_subtype && ((d.main == 5 && (d.subtype == 1 || d.subtype == 3)) || (d.main == 9 && d.subtype == 1) ||
                                              (d.main == 20 && (d.subtype == 102 || d.subtype == 105)))));
}

void set_state(const char *next)
{
    if (strcmp(state, next) && (!strcmp(state, "running") || !strcmp(next, "running")))
    {
        if (!strcmp(next, "running"))
        {
            // A callback can run inside start(), before it returns.
            registry_revision = next_view_revision();
            for (auto &o : objects)
            {
                o.widget_revision = registry_revision;
            }
        }
        else
        {
            invalidate_views();
        }
    }
    state = next;
}

bool fail(char *out, size_t size, const char *reason)
{
    if (out && size)
    {
        snprintf(out, size, "%s", reason);
    }
    return false;
}

KnxCommunicationObject *find(const char *id)
{
    if (id && *id)
    {
        for (auto &o : objects)
        {
            if (o.id == id)
            {
                return &o;
            }
        }
    }
    return nullptr;
}

void install_objects(std::vector<KnxCommunicationObject> &parsed, KnxRoutingParameters &parameters)
{
    objects = std::move(parsed);
    route = std::move(parameters);
    by_address.resize(objects.size());
    std::iota(by_address.begin(), by_address.end(), 0);
    // The C library sort is already in every image; keep the routing index off the task stack.
    if (by_address.size() > 1)
    {
        std::qsort(by_address.data(), by_address.size(), sizeof(by_address[0]),
                   [](const void *a, const void *b)
                   {
                       return objects[*static_cast<const uint16_t *>(a)].parsed_group_address -
                              objects[*static_cast<const uint16_t *>(b)].parsed_group_address;
                   });
    }
}

/** @brief Update matching aliases from one external group write or response. */
void receive(const knx_telegram_t *t, void *)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    // Ignore our own multicast loopback: successful send is not bus feedback.
    if (!enabled || generation != active_generation || t->source == route.physical_address ||
        (t->command != KNX_COMMAND_RESPONSE && t->command != KNX_COMMAND_WRITE))
    {
        return;
    }
    KnxValue value;
    uint32_t revision = 0;
    int64_t received_ms = 0;
    bool valid_value = false;
    auto it = std::lower_bound(by_address.begin(), by_address.end(), t->destination,
                               [](uint16_t index, knx_address_t address) { return objects[index].parsed_group_address < address; });
    for (; it != by_address.end(); ++it)
    {
        auto &o = objects[*it];
        if (o.parsed_group_address != t->destination)
        {
            break;
        }
        const bool observe_command = t->command == KNX_COMMAND_WRITE && cacheable_command(o);
        if (!o.readable && !observe_command)
        {
            continue;
        }
        // Configuration enforces a single DPT per address. Decode once for all aliases.
        if (!revision)
        {
            const auto &dpt = o.datapoint_type;
            valid_value = KnxDecodeValue(dpt, t->data, t->data_length, value);
            if (!valid_value)
            {
                ++rejected_count;
                // A complete invalid HVAC report revokes the reading. A damaged
                // packet cannot supersede feedback, or a previously sent command.
                const bool no_temperature = dpt.main == 9 && t->data_length == 3 && t->data[1] == 0x7f && t->data[2] == 0xff;
                const bool reserved_mode = dpt.main == 20 && (dpt.subtype == 102 || dpt.subtype == 105) && t->data_length == 2;
                if (!dpt.has_subtype || !(no_temperature || reserved_mode))
                {
                    return;
                }
                value = std::monostate {};
            }
            revision = next_view_revision();
            received_ms = esp_timer_get_time() / 1000;
            last_received = received_ms;
            ++received_count;
        }
        if (observe_command && valid_value)
        {
            if (const auto scalar = KnxControlScalar(value))
            {
                o.last_command = *scalar;
                o.command_revision = revision;
            }
        }
        if (!o.readable)
        {
            continue;
        }
        o.current_value = value;
        o.valid = valid_value;
        o.live_feedback = valid_value;
        o.widget_revision = revision;
        o.last_update_ms = received_ms;
    }
}

void run(void *)
{
    esp_knx_ip_handle_t handle = nullptr;
    uint32_t bound_ip = 0;
    esp_netif_t *bound_netif = nullptr;
    int64_t retry_at = 0;
    for (;;)
    {
        auto *netif = net_mgr_active_netif();
        esp_netif_ip_info_t ip {};
        bool online =
            net_mgr_is_connected() && netif && esp_netif_is_netif_up(netif) && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr;
        bool restart, want;
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            want = enabled && valid_config;
            restart = !want || !online || bound_ip != ip.ip.addr || bound_netif != netif || active_generation != generation;
            if (restart)
            {
                set_state(!enabled ? "disabled" : !valid_config ? "error" : !online ? "waiting_for_network" : "starting");
            }
            if (handle && restart)
            {
                active_generation = 0;
            }
        }
        // stop waits for the receive callback, so never hold state_mutex here.
        if (handle && restart)
        {
            esp_knx_ip_stop(handle);
            esp_knx_ip_destroy(handle);
            handle = nullptr;
            bound_ip = 0;
            retry_at = 0;
        }
        if (want && online && !handle && esp_timer_get_time() >= retry_at)
        {
            esp_err_t result;
            {
                std::lock_guard<std::mutex> guard(state_mutex);
                esp_knx_ip_config_t cfg = ESP_KNX_IP_CONFIG_DEFAULT(netif);
                cfg.multicast_address = route.multicast_address.c_str();
                cfg.port = route.port;
                cfg.physical_address = route.physical_address;
                cfg.load_physical_address_from_nvs = false;
                result = betta_knx_route_create(&cfg, receive, &handle);
                active_generation = generation;
            }
            if (result == ESP_OK)
            {
                result = esp_knx_ip_start(handle);
            }
            {
                std::lock_guard<std::mutex> guard(state_mutex);
                if (result == ESP_OK)
                {
                    bound_ip = ip.ip.addr;
                    bound_netif = netif;
                    if (enabled && generation == active_generation)
                    {
                        set_state("running");
                        error.clear();
                    }
                }
                else
                {
                    active_generation = 0;
                    invalidate_views();
                    set_state("error");
                    error = esp_err_to_name(result);
                }
            }
            if (result != ESP_OK)
            {
                if (handle)
                {
                    esp_knx_ip_destroy(handle);
                }
                handle = nullptr;
                retry_at = esp_timer_get_time() + 10000000;
            }
        }
        Command c;
        if (xQueueReceive(commands, &c, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            auto *o = find(c.id);
            if (!enabled || !handle || routing_epoch != c.epoch || generation != active_generation || !o ||
                !(c.write ? o->writable : o->readable))
            {
                rejected_count++;
                continue;
            }
            std::vector<uint8_t> data(1, 0);
            KnxValue value;
            const char *text = c.value;
            if (c.toggle)
            {
                text = o->command_revision && o->last_command != 0 ? "off" : "on";
            }
            if (c.write && (!KnxParseValue(o->datapoint_type, text, value) || !KnxEncodeValue(o->datapoint_type, value, data)))
            {
                rejected_count++;
                continue;
            }
            esp_err_t result =
                esp_knx_ip_send(handle, o->parsed_group_address, c.write ? KNX_COMMAND_WRITE : KNX_COMMAND_READ, data.data(), data.size());
            if (result != ESP_OK)
            {
                rejected_count++;
                error = esp_err_to_name(result);
                continue;
            }
            ++sent_count;
            if (c.write && cacheable_command(*o))
            {
                const auto scalar = KnxControlScalar(value);
                if (!scalar)
                {
                    continue;
                }
                const auto revision = next_view_revision();
                for (auto &alias : objects)
                {
                    if (alias.writable && alias.parsed_group_address == o->parsed_group_address)
                    {
                        alias.last_command = *scalar;
                        alias.command_revision = revision;
                    }
                }
            }
        }
    }
}
} // namespace

extern "C" void knx_init(void)
{
    std::lock_guard<std::mutex> edit(edit_mutex);
    if (initialized)
    {
        return;
    }
    std::string json, canonical, why;
    std::vector<KnxCommunicationObject> parsed;
    KnxRoutingParameters parameters;
    bool ok = KnxLoadConfiguration(json, why) && KnxParseConfiguration(json, parsed, canonical, why, &parameters);
    uint8_t saved = 0;
    nvs_handle_t h;
    if (nvs_open("knx", NVS_READONLY, &h) == ESP_OK)
    {
        nvs_get_u8(h, "enabled", &saved);
        nvs_close(h);
    }
    std::lock_guard<std::mutex> guard(state_mutex);
    configuration = ok ? std::move(canonical) : std::move(json);
    install_objects(parsed, parameters);
    valid_config = ok;
    enabled = saved != 0 && CONFIG_LWIP_MAX_SOCKETS >= 24;
    invalidate_views();
    error = why;
    commands = xQueueCreate(8, sizeof(Command));
    if (!commands || xTaskCreate(run, "knx", 6144, nullptr, 3, &worker) != pdPASS)
    {
        error = "Cannot start KNX worker";
        state = "error";
        if (commands)
        {
            vQueueDelete(commands);
            commands = nullptr;
        }
    }
    else
    {
        initialized = true;
    }
    ESP_LOGI("knx", "Configuration: %s, objects=%u, enabled=%d", ok ? "valid" : "invalid", static_cast<unsigned>(objects.size()), enabled);
}

extern "C" bool knx_enable(bool value, char *out, size_t size)
{
    std::lock_guard<std::mutex> edit(edit_mutex);
    {
        std::lock_guard<std::mutex> guard(state_mutex);
        if (!initialized || (value && !valid_config))
        {
            return fail(out, size, "KNX configuration or worker unavailable");
        }
        if (enabled == value)
        {
            return true;
        }
    }
    // Old 16-socket sdkconfigs have no spare socket beyond the HTTP reserve.
    if (value && CONFIG_LWIP_MAX_SOCKETS < 24)
    {
        return fail(out, size, "Rebuild with at least 24 lwIP sockets");
    }
    if (!flash_gate_enter(1000))
    {
        return fail(out, size, "Storage busy");
    }
    nvs_handle_t h;
    esp_err_t result = nvs_open("knx", NVS_READWRITE, &h);
    if (result == ESP_OK)
    {
        result = nvs_set_u8(h, "enabled", value);
        if (result == ESP_OK)
        {
            result = nvs_commit(h);
        }
        nvs_close(h);
    }
    flash_gate_leave();
    if (result != ESP_OK)
    {
        return fail(out, size, esp_err_to_name(result));
    }
    std::lock_guard<std::mutex> guard(state_mutex);
    enabled = value;
    generation++;
    set_state(value ? "starting" : "disabled");
    return true;
}

extern "C" bool knx_configure(const char *json, char *out, size_t size)
{
    std::lock_guard<std::mutex> edit(edit_mutex);
    std::vector<KnxCommunicationObject> parsed;
    std::string canonical, why;
    KnxRoutingParameters parameters;
    if (!json || !KnxParseConfiguration(json, parsed, canonical, why, &parameters) || !KnxSaveConfiguration(canonical, why))
    {
        return fail(out, size, why.c_str());
    }
    std::lock_guard<std::mutex> guard(state_mutex);
    install_objects(parsed, parameters);
    configuration = std::move(canonical);
    valid_config = true;
    generation++;
    error.clear();
    set_state(enabled ? "starting" : "disabled");
    invalidate_views();
    return true;
}

extern "C" char *knx_configuration(void)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    return strdup(configuration.c_str());
}

extern "C" cJSON *knx_status(void)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    cJSON *j = cJSON_CreateObject();
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

extern "C" cJSON *knx_objects(int offset, int limit)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    const int count = static_cast<int>(objects.size());
    offset = std::clamp(offset, 0, count);
    limit = std::clamp(limit, 1, 20);
    const int end = std::min(count, offset + limit);
    cJSON *j = cJSON_CreateObject();
    cJSON *list = j ? cJSON_AddArrayToObject(j, "objects") : nullptr;
    if (list && cJSON_AddBoolToObject(j, "has_more", end < count) && cJSON_AddNumberToObject(j, "generation", generation) &&
        cJSON_AddNumberToObject(j, "routing_epoch", routing_epoch) && cJSON_AddNumberToObject(j, "next_offset", end) &&
        cJSON_AddStringToObject(j, "state", state))
    {
        for (int i = offset; i < end; ++i)
        {
            if (!cJSON_AddItemToArray(list, KnxObjectJson(objects[i])))
            {
                cJSON_Delete(j);
                return nullptr;
            }
        }
        return j;
    }
    cJSON_Delete(j);
    return nullptr;
}

extern "C" cJSON *knx_object(const char *id)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    auto *o = find(id);
    cJSON *j = o ? KnxObjectJson(*o) : nullptr;
    if (j && !cJSON_AddStringToObject(j, "state", state))
    {
        cJSON_Delete(j);
        return nullptr;
    }
    return j;
}

/**
 * @brief Validate and enqueue without waiting; bind the command to the current routing epoch.
 * @pre The caller holds state_mutex.
 */
static bool queue_command(KnxCommunicationObject *o, const char *value, bool write, char *out, size_t size, bool toggle = false)
{
    if (!enabled || !initialized || strcmp(state, "running") || generation != active_generation)
    {
        return fail(out, size, "KNX routing is not running");
    }
    if (!o || !(write ? o->writable : o->readable))
    {
        return fail(out, size, "Unknown object or operation not permitted");
    }
    Command c {};
    c.epoch = routing_epoch;
    c.write = write;
    c.toggle = toggle;
    snprintf(c.id, sizeof(c.id), "%s", o->id.c_str());
    if (write)
    {
        KnxValue v;
        if (!value || strlen(value) >= sizeof(c.value) || !KnxParseValue(o->datapoint_type, value, v))
        {
            return fail(out, size, "Invalid value for configured DPT");
        }
        snprintf(c.value, sizeof(c.value), "%s", value);
    }
    return xQueueSend(commands, &c, 0) == pdTRUE || fail(out, size, "KNX command queue is busy");
}

extern "C" bool knx_command(const char *id, const char *address, const char *value, bool write, char *out, size_t size)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    auto *object = find(id);
    if (!id && address)
    {
        for (auto &o : objects)
        {
            if (o.group_address != address || !(write ? o.writable : o.readable))
            {
                continue;
            }
            if (object)
            {
                return fail(out, size, "Multiple objects match this address; select an object ID");
            }
            object = &o;
        }
    }
    return queue_command(object, value, write, out, size);
}

extern "C" uint32_t knx_widget_revision(void)
{
    return view_revision.load();
}

// Callers hold state_mutex across role resolution, projection and queue validation.
static KnxCommunicationObject *bound_object(const knx_control_binding_t &binding, knx_role_t role)
{
    auto *o = find(binding.id[role]);
    return o && KnxWidgetRoleSupports(*o, role) ? o : nullptr;
}

static bool routing_active()
{
    return enabled && generation == active_generation && !strcmp(state, "running");
}

static knx_view_result_t read_controls(knx_control_binding_t &binding, knx_tile_kind_t kind, const char *key, uint32_t *cursor,
                                       ha_state_t *out)
{
    const KnxCommunicationObject *resolved[KNX_ROLE_COUNT] {};
    // Compare ages so a revision rollover cannot hide fresh feedback.
    const uint32_t now = view_revision.load();
    uint32_t revision = registry_revision;
    for (int i = 0; i < KNX_ROLE_COUNT; ++i)
    {
        const auto role = static_cast<knx_role_t>(i);
        const auto *o = knx_tile_has_role(kind, role) ? bound_object(binding, role) : nullptr;
        resolved[i] = o;
        if (!o)
        {
            continue;
        }
        if (now - o->widget_revision < now - revision)
        {
            revision = o->widget_revision;
        }
        const knx_role_t feedback = knx_command_feedback(role);
        if (feedback != KNX_ROLE_COUNT && !binding.id[feedback][0] && o->command_revision && now - o->command_revision < now - revision)
        {
            revision = o->command_revision;
        }
    }
    if (*cursor == revision)
    {
        return KNX_VIEW_UNCHANGED;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->entity_id, sizeof(out->entity_id), "%s", key);
    if (!KnxWidgetProjectRoles(resolved, kind, routing_active(), binding, *out))
    {
        out->attributes_json[0] = '\0';
        strcpy(out->state, "unavailable");
    }
    *cursor = revision;
    return KNX_VIEW_UPDATED;
}

extern "C" knx_view_result_t knx_widget_read(const char *key, knx_tile_kind_t kind, const char *preset, knx_control_binding_t *controls,
                                             uint32_t *cursor, ha_state_t *out)
{
    std::unique_lock<std::mutex> guard(state_mutex, std::try_to_lock);
    if (!guard.owns_lock())
    {
        return KNX_VIEW_BUSY;
    }
    if (controls)
    {
        return read_controls(*controls, kind, key, cursor, out);
    }
    auto *o = find(knx_binding_id(key));
    const uint32_t revision = o ? o->widget_revision : registry_revision;
    if (*cursor == revision)
    {
        return KNX_VIEW_UNCHANGED;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->entity_id, sizeof(out->entity_id), "%s", key);
    KnxValue parsed;
    if (!o || !enabled || generation != active_generation || strcmp(state, "running") ||
        (kind == KNX_TILE_ACTION && (!preset || !KnxParseValue(o->datapoint_type, preset, parsed))) || !KnxWidgetProject(*o, kind, *out))
    {
        out->attributes_json[0] = '\0';
        strcpy(out->state, "unavailable");
    }
    *cursor = revision;
    return KNX_VIEW_UPDATED;
}

extern "C" esp_err_t knx_widget_control_roles(const knx_control_binding_t *controls, knx_widget_action_t action, int value)
{
    if (!controls)
    {
        return ESP_ERR_INVALID_ARG;
    }
    std::unique_lock<std::mutex> guard(state_mutex, std::try_to_lock);
    if (!guard.owns_lock())
    {
        return ESP_ERR_TIMEOUT;
    }
    knx_role_t role;
    switch (action)
    {
        case KNX_TOGGLE:
        case KNX_SET_POWER:
            role = KNX_SWITCH_COMMAND;
            break;
        case KNX_SET_PERCENT:
            role = KNX_BRIGHTNESS_COMMAND;
            break;
        case KNX_SET_TARGET_TEMPERATURE:
            role = KNX_TARGET_TEMPERATURE_COMMAND;
            break;
        case KNX_SET_HVAC_MODE:
            role = KNX_HVAC_MODE_COMMAND;
            break;
        case KNX_SET_FAN_PERCENT:
            role = KNX_FAN_SPEED_COMMAND;
            break;
        case KNX_SET_PRESET:
            role = KNX_PRESET_COMMAND;
            break;
        case KNX_SET_ANGLE:
            role = KNX_ANGLE_COMMAND;
            break;
        case KNX_COVER_STOP:
            role = KNX_STOP_COMMAND;
            break;
        case KNX_COVER_OPEN:
        case KNX_COVER_CLOSE:
            role = KNX_MOVE_COMMAND;
            if (!bound_object(*controls, role))
            {
                role = KNX_POSITION_COMMAND;
                const bool open = action == KNX_COVER_OPEN;
                value = open == controls->position_open_at_100 ? 100 : 0;
                action = KNX_SET_PERCENT;
            }
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    auto *target = bound_object(*controls, role);
    if (!target)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (action == KNX_TOGGLE)
    {
        if (!controls->id[KNX_SWITCH_FEEDBACK][0])
        {
            return queue_command(target, "on", true, nullptr, 0, true) ? ESP_OK : ESP_FAIL;
        }
        auto *feedback = bound_object(*controls, KNX_SWITCH_FEEDBACK);
        const auto on = KnxWidgetDisplayValue(*controls, KNX_SWITCH_FEEDBACK, feedback, target);
        if (!on)
        {
            return ESP_ERR_INVALID_STATE;
        }
        value = !*on;
        action = KNX_SET_POWER;
    }
    std::string text;
    if (!KnxWidgetCommandValue(*target, action, value, nullptr, text))
    {
        return ESP_ERR_INVALID_ARG;
    }
    return queue_command(target, text.c_str(), true, nullptr, 0) ? ESP_OK : ESP_FAIL;
}

extern "C" esp_err_t knx_widget_control(const char *key, knx_widget_action_t action, int value, const char *preset)
{
    std::unique_lock<std::mutex> guard(state_mutex, std::try_to_lock);
    if (!guard.owns_lock())
    {
        return ESP_ERR_TIMEOUT;
    }
    auto *o = find(knx_binding_id(key));
    std::string text;
    if (!o || !KnxWidgetCommandValue(*o, action, value, preset, text))
    {
        return ESP_ERR_INVALID_ARG;
    }
    return queue_command(o, text.c_str(), true, nullptr, 0) ? ESP_OK : ESP_FAIL;
}
