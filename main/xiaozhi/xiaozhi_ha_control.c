/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_ha_control.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ha/ha_client.h"
#include "ha/ha_light_capabilities.h"

typedef enum { NONE, PERCENT, RGB, KELVIN, NUMBER, TEMPERATURE, CHOICE, BOOLEAN } value_kind_t;
typedef struct {
    const char *domains, *action, *service, *key;
    value_kind_t kind;
    const char *choices;
    unsigned feature;
} rule_t;
/* No arbitrary domain/service/data, wildcard, area, device or multi-entity
 * targets. Stateful actions use explicit values instead of retrying toggles. */
static const rule_t rules[] = {
    {"light|switch|input_boolean|scene|script", "turn_on", "turn_on", NULL, NONE, NULL, 0},
    {"light|switch|input_boolean|script", "turn_off", "turn_off", NULL, NONE, NULL, 0},
    {"light", "set_brightness", "turn_on", "brightness_pct", PERCENT, NULL, 0},
    {"light", "set_color", "turn_on", "rgb_color", RGB, NULL, 0},
    {"light", "set_color_temperature", "turn_on", "color_temp_kelvin", KELVIN, NULL, 0},
    {"light", "set_effect", "turn_on", "effect", CHOICE, "effect_list", 0},
    {"fan", "set_percentage", "set_percentage", "percentage", PERCENT, NULL, 1},
    {"fan", "oscillate", "oscillate", "oscillating", BOOLEAN, NULL, 2},
    {"fan", "turn_on", "turn_on", NULL, NONE, NULL, 32},
    {"fan", "turn_off", "turn_off", NULL, NONE, NULL, 16},
    {"fan", "set_preset_mode", "set_preset_mode", "preset_mode", CHOICE, "preset_modes", 8},
    {"cover", "open_cover", "open_cover", NULL, NONE, NULL, 1},
    {"cover", "close_cover", "close_cover", NULL, NONE, NULL, 2},
    {"cover", "stop_cover", "stop_cover", NULL, NONE, NULL, 8},
    {"cover", "set_position", "set_cover_position", "position", PERCENT, NULL, 4},
    {"climate", "set_temperature", "set_temperature", "temperature", TEMPERATURE, NULL, 1},
    {"climate", "set_hvac_mode", "set_hvac_mode", "hvac_mode", CHOICE, "hvac_modes", 0},
    {"climate", "set_fan_mode", "set_fan_mode", "fan_mode", CHOICE, "fan_modes", 8},
    {"climate", "set_swing_mode", "set_swing_mode", "swing_mode", CHOICE, "swing_modes", 32},
    {"climate", "set_preset_mode", "set_preset_mode", "preset_mode", CHOICE, "preset_modes", 16},
    {"number|input_number", "set_value", "set_value", "value", NUMBER, NULL, 0},
    {"select|input_select", "select_option", "select_option", "option", CHOICE, "options", 0},
    {"button|input_button", "press", "press", NULL, NONE, NULL, 0},
};

typedef struct {
    char domain[32];
    cJSON *attrs;
    ha_light_capabilities_t light;
} context_t;

static const cJSON *field(const cJSON *o, const char *key) { return cJSON_GetObjectItemCaseSensitive(o, key); }
static bool domain_matches(const char *set, const char *domain)
{
    const size_t n = strlen(domain);
    for (const char *p = set; p; p = strchr(p, '|')) {
        if (*p == '|') p++;
        size_t len = strcspn(p, "|");
        if (len == n && !strncmp(p, domain, n)) return true;
    }
    return false;
}

static bool prepare(context_t *ctx, const ha_state_t *state)
{
    memset(ctx, 0, sizeof(*ctx));
    if (!state || !state->entity_id[0] || strlen(state->entity_id) >= APP_MAX_ENTITY_ID_LEN) return false;
    const char *dot = strchr(state->entity_id, '.');
    if (!dot || dot == state->entity_id || !dot[1] || (size_t)(dot - state->entity_id) >= sizeof(ctx->domain)) return false;
    for (const char *p = state->entity_id; *p; p++) {
        if (p == dot) continue;
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') && *p != '_') return false;
    }
    memcpy(ctx->domain, state->entity_id, dot - state->entity_id);
    ctx->attrs = cJSON_Parse(state->attributes_json);
    if (!cJSON_IsObject(ctx->attrs)) { cJSON_Delete(ctx->attrs); ctx->attrs = NULL; return false; }
    if (!strcmp(ctx->domain, "light")) ha_light_capabilities_from_state(state, &ctx->light);
    return true;
}

static bool range(const context_t *ctx, const rule_t *r, double *min, double *max, double *step)
{
    *step = 1;
    if (r->kind == PERCENT) { *min = 0; *max = 100; return true; }
    if (r->kind == KELVIN) {
        *min = ctx->light.min_color_temp_kelvin; *max = ctx->light.max_color_temp_kelvin;
        return *min > 0 && *max >= *min;
    }
    const bool temp = r->kind == TEMPERATURE;
    const cJSON *lo = field(ctx->attrs, temp ? "min_temp" : "min");
    const cJSON *hi = field(ctx->attrs, temp ? "max_temp" : "max");
    const cJSON *inc = field(ctx->attrs, temp ? "target_temp_step" : "step");
    if (!cJSON_IsNumber(lo) || !cJSON_IsNumber(hi) || !isfinite(lo->valuedouble) ||
        !isfinite(hi->valuedouble) || hi->valuedouble < lo->valuedouble) return false;
    *min = lo->valuedouble; *max = hi->valuedouble;
    *step = cJSON_IsNumber(inc) ? inc->valuedouble : 0;
    return isfinite(*step) && *step >= 0;
}

static bool enabled(const context_t *ctx, const rule_t *r)
{
    if (!domain_matches(r->domains, ctx->domain)) return false;
    if (!strcmp(ctx->domain, "light")) {
        if (r->kind == PERCENT && !ctx->light.can_dim) return false;
        if (r->kind == RGB && !ctx->light.can_color) return false;
        if (r->kind == KELVIN && !ctx->light.can_color_temp) return false;
    }
    if (r->feature) {
        const cJSON *bits = field(ctx->attrs, "supported_features");
        if (!cJSON_IsNumber(bits) || !isfinite(bits->valuedouble) || bits->valuedouble < 0 ||
            bits->valuedouble > UINT32_MAX || !((uint32_t)bits->valuedouble & r->feature)) return false;
    }
    if (r->kind == CHOICE) {
        const cJSON *choices = field(ctx->attrs, r->choices);
        for (const cJSON *v = cJSON_IsArray(choices) ? choices->child : NULL; v; v = v->next)
            if (cJSON_IsString(v) && v->valuestring[0] && strlen(v->valuestring) < 96) return true;
        return false;
    }
    if (r->kind == NUMBER || r->kind == TEMPERATURE || r->kind == KELVIN) {
        double lo, hi, step; return range(ctx, r, &lo, &hi, &step);
    }
    return true;
}

static bool usable(const ha_state_t *state)
{
    /* Never-run buttons/scenes can legitimately have state "unknown". */
    return state && state->state[0] && strcmp(state->state, "unavailable");
}

cJSON *xiaozhi_ha_control_info(const ha_state_t *state, bool ready)
{
    cJSON *out = cJSON_CreateArray();
    if (!out || !ready || !usable(state)) return out;
    context_t ctx;
    if (!prepare(&ctx, state)) { cJSON_Delete(out); return NULL; }
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        const rule_t *r = &rules[i];
        if (!enabled(&ctx, r)) continue;
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddItemToArray(out, item)) { cJSON_Delete(item); goto fail; }
        const char *kind = r->kind == NONE ? "omit" : r->kind == RGB ? "#RRGGBB" :
            r->kind == BOOLEAN ? "true|false" : r->kind == CHOICE ? "option" : "number";
        if (!cJSON_AddStringToObject(item, "action", r->action) || !cJSON_AddStringToObject(item, "value", kind)) goto fail;
        if (r->kind == PERCENT || r->kind == KELVIN || r->kind == NUMBER || r->kind == TEMPERATURE) {
            double lo, hi, step;
            if (!range(&ctx, r, &lo, &hi, &step) || !cJSON_AddNumberToObject(item, "min", lo) ||
                !cJSON_AddNumberToObject(item, "max", hi) || !cJSON_AddNumberToObject(item, "step", step)) goto fail;
        }
        if (r->kind == CHOICE) {
            cJSON *options = cJSON_AddArrayToObject(item, "options");
            if (!options) goto fail;
            int count = 0;
            for (const cJSON *v = field(ctx.attrs, r->choices)->child; v; v = v->next) {
                if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring) >= 96) continue;
                if (++count > 8) continue;
                cJSON *copy = cJSON_CreateString(v->valuestring);
                if (!copy || !cJSON_AddItemToArray(options, copy)) { cJSON_Delete(copy); goto fail; }
            }
            if (!cJSON_AddBoolToObject(item, "options_complete", count <= 8)) goto fail;
        }
    }
    cJSON_Delete(ctx.attrs);
    return out;
fail:
    cJSON_Delete(ctx.attrs); cJSON_Delete(out); return NULL;
}

static bool number(const char *value, double *out)
{
    if (!value || !*value) return false;
    for (const char *p = value; *p; p++) if (!strchr("0123456789+-.eE", *p)) return false;
    errno = 0; char *end; *out = strtod(value, &end);
    return end != value && !*end && errno != ERANGE && isfinite(*out);
}

static bool add_value(cJSON *data, const context_t *ctx, const rule_t *r, const cJSON *input)
{
    if (r->kind == NONE) return input == NULL;
    const char *value = cJSON_GetStringValue(input);
    if (!value || !*value) return false;
    if (r->kind == BOOLEAN) {
        if (strcmp(value, "true") && strcmp(value, "false")) return false;
        return cJSON_AddBoolToObject(data, r->key, !strcmp(value, "true")) != NULL;
    }
    if (r->kind == CHOICE) {
        if (strlen(value) >= 96) return false;
        const cJSON *v = NULL;
        cJSON_ArrayForEach(v, field(ctx->attrs, r->choices))
            if (cJSON_IsString(v) && !strcmp(v->valuestring, value)) return cJSON_AddStringToObject(data, r->key, value) != NULL;
        return false;
    }
    if (r->kind == RGB) {
        if (strlen(value) != 7 || value[0] != '#') return false;
        for (int i = 1; i < 7; i++) if (!isxdigit((unsigned char)value[i])) return false;
        unsigned long rgb = strtoul(value + 1, NULL, 16);
        cJSON *arr = cJSON_AddArrayToObject(data, r->key);
        if (!arr) return false;
        for (int shift = 16; shift >= 0; shift -= 8) {
            cJSON *v = cJSON_CreateNumber((rgb >> shift) & 255);
            if (!v || !cJSON_AddItemToArray(arr, v)) { cJSON_Delete(v); return false; }
        }
        return true;
    }
    double n, lo, hi, step;
    if (!number(value, &n) || !range(ctx, r, &lo, &hi, &step) || n < lo || n > hi) return false;
    if (step > 0) {
        double steps = (n - lo) / step;
        if (!isfinite(steps) || fabs(steps - round(steps)) > 1e-6) return false;
    }
    return cJSON_AddNumberToObject(data, r->key, n) != NULL;
}

cJSON *xiaozhi_ha_control_execute(const ha_state_t *state, const cJSON *args, char *error, size_t error_size)
{
    const char *message = "Entity/action is unavailable; inspect self.ha.get_entity controls";
    context_t ctx = {0}; cJSON *data = NULL, *result = NULL; char *payload = NULL;
    if (!usable(state) || !prepare(&ctx, state)) goto fail;
    const char *action = cJSON_GetStringValue(field(args, "action"));
    const rule_t *rule = NULL;
    for (size_t i = 0; action && i < sizeof(rules) / sizeof(rules[0]); i++)
        if (!strcmp(action, rules[i].action) && enabled(&ctx, &rules[i])) { rule = &rules[i]; break; }
    if (!rule) goto fail;
    data = cJSON_CreateObject();
    if (!data || !cJSON_AddStringToObject(data, "entity_id", state->entity_id)) goto fail;
    message = "Invalid value or insufficient memory; use the entity's controls and range";
    if (!add_value(data, &ctx, rule, field(args, "value"))) goto fail;
#if APP_HA_LIGHT_USE_TRANSITION_ZERO
    if (!strcmp(ctx.domain, "light") && !cJSON_AddNumberToObject(data, "transition", 0)) goto fail;
#endif
    payload = cJSON_PrintUnformatted(data);
    result = cJSON_CreateObject();
    message = "Unable to allocate Home Assistant command response";
    /* Allocate every local response field before the side effect. Never report
     * a transport write as confirmed physical state; the shared HA model is
     * updated only by HA feedback, not by this command. No automatic retries. */
    if (!payload || !result || !cJSON_AddStringToObject(result, "entity_id", state->entity_id) ||
        !cJSON_AddStringToObject(result, "action", action) ||
        !cJSON_AddStringToObject(result, "service", rule->service) ||
        !cJSON_AddBoolToObject(result, "submitted", true) ||
        !cJSON_AddBoolToObject(result, "state_confirmed", false) ||
        !cJSON_AddStringToObject(result, "confirmation", "Read self.ha.get_entity again for HA feedback; do not infer completion or automatically retry")) goto fail;
    if (ha_client_call_service(ctx.domain, rule->service, payload) != ESP_OK) {
        message = "HA command transmission failed; check entity state before retrying"; goto fail;
    }
    cJSON_free(payload); cJSON_Delete(data); cJSON_Delete(ctx.attrs);
    return result;
fail:
    cJSON_free(payload); cJSON_Delete(data); cJSON_Delete(ctx.attrs); cJSON_Delete(result);
    snprintf(error, error_size, "%s", message);
    return NULL;
}
