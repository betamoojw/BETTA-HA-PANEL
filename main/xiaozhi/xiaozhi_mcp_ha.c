/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi_mcp_ha.h"
#include "xiaozhi_ha_control.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "ha/ha_client.h"
#include "ha/ha_layout_entities.h"
#include "ha/ha_model.h"
#include "layout/layout_store.h"

/* Same ID budget as the HA subscription collector. Fail closed at capacity,
 * rather than silently advertise an incomplete set of layout entities. */
#define ENTITY_CAPACITY ((APP_MAX_WIDGETS_TOTAL * 4) + (APP_MAX_PAGES * 9) + 4)
#define RESPONSE_BYTES 2800

static const char *text_arg(const cJSON *args, const char *key)
{
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(args, key));
}

static bool add_text(cJSON *obj, const char *key, const char *value)
{
    return value ? cJSON_AddStringToObject(obj, key, value) != NULL :
                   cJSON_AddNullToObject(obj, key) != NULL;
}

static bool contains(const char *text, const char *query)
{
    if (!query || !*query) return true;
    size_t n = strlen(query);
    for (; *text; text++) {
        size_t i = 0;
        while (i < n && text[i] &&
               tolower((unsigned char)text[i]) == tolower((unsigned char)query[i])) i++;
        if (i == n) return true;
    }
    return false;
}

static bool fits(const cJSON *obj)
{
    char *json = cJSON_PrintUnformatted(obj);
    bool ok = json && strlen(json) <= RESPONSE_BYTES;
    cJSON_free(json);
    return ok;
}

/* Only bounded operational attributes. In particular, do not forward camera
 * tokens, media URLs, or arbitrary integration attributes to the cloud. The
 * model already compacts attributes; this is not the complete HA state object. */
static cJSON *attributes(const cJSON *source)
{
    static const char *const keys[] = {
        "unit_of_measurement", "device_class", "state_class", "brightness",
        "color_temp_kelvin", "color_mode", "temperature", "current_temperature",
        "target_temp_high", "target_temp_low", "humidity", "current_humidity",
        "hvac_action", "fan_mode", "swing_mode", "preset_mode", "current_position",
        "current_tilt_position", "percentage", "direction", "oscillating",
        "battery_level", "fan_speed", "volume_level", "is_volume_muted",
        "media_title", "media_artist", "source", "wind_speed", "wind_bearing",
        "pressure", "cloud_coverage", "visibility", "apparent_temperature",
        "dew_point", "precipitation", "temperature_unit", "pressure_unit",
        "wind_speed_unit", "distance_unit", "last_triggered"
    };
    cJSON *out = cJSON_CreateObject();
    if (!out) return NULL;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(source, keys[i]);
        if (!value) continue;
        if (!(cJSON_IsNumber(value) || cJSON_IsBool(value) ||
              (cJSON_IsString(value) && strlen(value->valuestring) <= 160))) continue;
        cJSON *copy = cJSON_Duplicate(value, false);
        if (!copy || !cJSON_AddItemToObject(out, keys[i], copy)) {
            cJSON_Delete(copy); cJSON_Delete(out); return NULL;
        }
    }
    return out;
}

static cJSON *entity(const char *id, ha_state_t *state, bool ready, bool detail)
{
    memset(state, 0, sizeof(*state));
    bool known = ha_model_get_state(id, state);
    char name[APP_MAX_NAME_LEN] = {0};
    ha_model_get_entity_name(id, name, sizeof(name));
    cJSON *attrs = known ? cJSON_Parse(state->attributes_json) : cJSON_CreateObject();
    if (!cJSON_IsObject(attrs)) { cJSON_Delete(attrs); attrs = cJSON_CreateObject(); }
    cJSON *out = cJSON_CreateObject();
    bool valid = ready && known && state->state[0] &&
                 strcmp(state->state, "unknown") && strcmp(state->state, "unavailable");
    bool ok = out && attrs && add_text(out, "entity_id", id) &&
        add_text(out, "name", name[0] ? name : id) &&
        add_text(out, "state", known ? state->state : NULL) &&
        add_text(out, "unit", text_arg(attrs, "unit_of_measurement")) &&
        cJSON_AddBoolToObject(out, "known", known) &&
        cJSON_AddBoolToObject(out, "valid", valid) &&
        cJSON_AddBoolToObject(out, "stale", known && !ready);
    if (ok && detail) {
        cJSON *selected = attributes(attrs);
        ok = selected && cJSON_AddItemToObject(out, "attributes", selected);
        if (!ok) cJSON_Delete(selected);
        ok = ok && cJSON_AddBoolToObject(out, "attributes_complete", false) &&
            cJSON_AddNumberToObject(out, "last_received_uptime_ms", state->last_changed_unix_ms) &&
            (state->ha_changed_s > 0 ? cJSON_AddNumberToObject(out, "last_changed_unix_s", state->ha_changed_s) :
                                     cJSON_AddNullToObject(out, "last_changed_unix_s"));
    }
    if (ok) {
        cJSON *controls = xiaozhi_ha_control_info(known ? state : NULL, ready);
        ok = controls && cJSON_AddBoolToObject(out, "controllable", cJSON_GetArraySize(controls) > 0);
        if (ok && detail) {
            ok = cJSON_AddItemToObject(out, "controls", controls);
            if (ok) controls = NULL;
        }
        cJSON_Delete(controls);
    }
    cJSON_Delete(attrs);
    if (!ok) { cJSON_Delete(out); return NULL; }
    return out;
}

cJSON *xiaozhi_mcp_ha_call(const char *name, const cJSON *args, char *error, size_t error_size)
{
    const bool list = !strcmp(name, "self.ha.list_entities");
    const bool control = !strcmp(name, "self.ha.control_entity");
    const char *message = "Home Assistant snapshot allocation failed";
    cJSON *result = NULL, *root = NULL;
    char *ids = NULL;
    ha_state_t *state = NULL;
    if (!list && !control && strcmp(name, "self.ha.get_entity")) {
        message = "Unsupported Home Assistant tool"; goto fail;
    }
    /* Every request re-reads the saved layout. A previously discovered ID does
     * not grant access after the owner removes it. No separate exported cache. */
    root = layout_store_load_parsed();
    if (!cJSON_IsObject(root)) { message = "Panel layout is unavailable; retry later"; goto fail; }
    ids = heap_caps_calloc(ENTITY_CAPACITY, APP_MAX_ENTITY_ID_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    state = heap_caps_calloc(1, sizeof(*state), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ids || !state) goto fail;
    ha_layout_entity_set_t set = {.ids = ids, .capacity = ENTITY_CAPACITY};
    ha_layout_entities_collect(root, &set);
    cJSON_Delete(root); root = NULL;
    if (set.count == set.capacity) { message = "Layout entity limit reached; reduce layout bindings"; goto fail; }
    char signature[9];
    snprintf(signature, sizeof(signature), "%08lx", (unsigned long)ha_layout_entities_signature(ids, set.count));
    const char *expected = text_arg(args, "layout_signature");
    if (expected && strcmp(expected, signature)) {
        message = "Layout entities changed; restart discovery at offset 0"; goto fail;
    }
    uint32_t revision = ha_model_state_revision();
    bool connected = ha_client_is_connected(), synced = ha_client_is_initial_sync_done();
    if (control) {
        const char *id = text_arg(args, "entity_id");
        if (!id || !ha_layout_entities_contains(ids, set.count, id)) {
            message = "Entity is not in the current panel layout; use self.ha.list_entities"; goto fail;
        }
        if (!connected || !synced || !ha_model_get_state(id, state)) {
            message = "HA is disconnected, synchronizing or has no state for this entity; no command sent"; goto fail;
        }
        result = xiaozhi_ha_control_execute(state, args, error, error_size);
        heap_caps_free(state); heap_caps_free(ids);
        return result;
    }
    result = cJSON_CreateObject();
    if (!result || !add_text(result, "scope", "panel_layout") ||
        !add_text(result, "source", "home_assistant_cache") ||
        !add_text(result, "layout_signature", signature) ||
        !cJSON_AddBoolToObject(result, "connected", connected) ||
        !cJSON_AddBoolToObject(result, "sync_complete", synced) ||
        !cJSON_AddNumberToObject(result, "state_revision", revision) ||
        !cJSON_AddBoolToObject(result, "consistent", false)) goto fail;

    if (list) {
        const cJSON *off = cJSON_GetObjectItemCaseSensitive(args, "offset");
        const cJSON *lim = cJSON_GetObjectItemCaseSensitive(args, "limit");
        int offset = off ? off->valueint : 0, limit = lim ? lim->valueint : 5;
        if (offset < 0 || limit < 1 || limit > 5 || (offset && !expected)) {
            message = "Use limit 1..5 and the previous layout_signature for continuation"; goto fail;
        }
        const char *domain = text_arg(args, "domain"), *search = text_arg(args, "search");
        size_t matches = 0;
        for (size_t i = 0; i < set.count; i++) {
            char *id = ids + i * APP_MAX_ENTITY_ID_LEN;
            const char *dot = strchr(id, '.');
            if (domain && (!dot || strlen(domain) != (size_t)(dot - id) || strncmp(id, domain, dot - id))) continue;
            char friendly[APP_MAX_NAME_LEN] = {0};
            ha_model_get_entity_name(id, friendly, sizeof(friendly));
            if (!contains(id, search) && !contains(friendly, search)) continue;
            memmove(ids + matches++ * APP_MAX_ENTITY_ID_LEN, id, APP_MAX_ENTITY_ID_LEN);
        }
        if ((size_t)offset > matches) { message = "Offset outside matching entities; restart discovery"; goto fail; }
        cJSON *items = cJSON_AddArrayToObject(result, "entities");
        cJSON *next = cJSON_AddNumberToObject(result, "next_offset", 65535);
        cJSON *more = cJSON_AddBoolToObject(result, "has_more", false);
        if (!items || !next || !more || !cJSON_AddNumberToObject(result, "total_entities", set.count) ||
            !cJSON_AddNumberToObject(result, "matching_entities", matches)) goto fail;
        size_t cursor = (size_t)offset;
        while (cursor < matches && cJSON_GetArraySize(items) < limit) {
            cJSON *item = entity(ids + cursor * APP_MAX_ENTITY_ID_LEN, state, connected && synced, false);
            if (!item || !cJSON_AddItemToArray(items, item)) { cJSON_Delete(item); goto fail; }
            if (!fits(result)) {
                cJSON_DeleteItemFromArray(items, cJSON_GetArraySize(items) - 1);
                if (!cJSON_GetArraySize(items)) goto fail;
                break;
            }
            cursor++;
        }
        cJSON_SetNumberValue(next, cursor);
        cJSON_SetBoolValue(more, cursor < matches);
    } else {
        const char *id = text_arg(args, "entity_id");
        if (!id || !ha_layout_entities_contains(ids, set.count, id)) {
            message = "Entity is not in the current panel layout; use self.ha.list_entities"; goto fail;
        }
        cJSON *item = entity(id, state, connected && synced, true);
        if (!item || !cJSON_AddItemToObject(result, "entity", item)) { cJSON_Delete(item); goto fail; }
        cJSON *attrs = cJSON_GetObjectItemCaseSensitive(item, "attributes");
        while (!fits(result) && attrs->child) cJSON_DeleteItemFromObjectCaseSensitive(attrs, attrs->child->string);
        /* Long escaped choices must not hide the action/range metadata. Keep
         * a bounded sample and disclose that the choices are incomplete. */
        cJSON *rule = NULL;
        cJSON_ArrayForEach(rule, cJSON_GetObjectItemCaseSensitive(item, "controls")) {
            cJSON *options = cJSON_GetObjectItemCaseSensitive(rule, "options");
            while (!fits(result) && options && options->child) {
                cJSON_DeleteItemFromArray(options, cJSON_GetArraySize(options) - 1);
                cJSON_SetBoolValue(cJSON_GetObjectItemCaseSensitive(rule, "options_complete"), false);
            }
        }
    }
    /* A disconnect/change during collection must never be presented as an
     * atomic live snapshot. Callers should repeat queries when consistent=false. */
    cJSON_SetBoolValue(cJSON_GetObjectItemCaseSensitive(result, "consistent"),
        revision == ha_model_state_revision() && connected == ha_client_is_connected() &&
        synced == ha_client_is_initial_sync_done());
    if (!fits(result)) goto fail;
    heap_caps_free(state); heap_caps_free(ids);
    return result;
fail:
    cJSON_Delete(root); cJSON_Delete(result);
    heap_caps_free(state); heap_caps_free(ids);
    snprintf(error, error_size, "%s", message);
    return NULL;
}
