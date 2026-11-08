/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723; adapted from cptkirki/BETTA-HA-PANEL 3dc138a.
 *
 * Alarm panel widget for a HA `alarm_control_panel.*` entity.
 *
 * The widget is a single always-visible page (size it to fill a dashboard
 * page) with three columns:
 *   - keypad + arm/disarm buttons (code is typed per action, never stored),
 *   - zone sensors, switch toggles (e.g. chime) and button entities
 *     (e.g. panic) which only fire after a continuous 2 s hold,
 *   - a history list pulled via the `logbook/get_events` WS command and
 *     extended live from the state changes this widget already receives.
 *
 * Layout JSON (besides entity_id / secondary_entity_id = keypad text sensor):
 *   "alarm_zones":   [{"entity_id": "binary_sensor.x", "name": "Front Door"}, ...]
 *   "alarm_actions": [{"entity_id": "switch.chime" | "button.panic", "name": "..."}, ...]
 *   "alarm_log_hours": 24
 */
#include "drivers/display_init.h"
#include "ui/ui_widget_factory.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"


#include "app_config.h"
#include "ha/ha_client.h"
#include "ui/theme/ui_typography.h"
#include "ui/theme/ui_style.h"
#include "ha/ha_alarm_state.h"
#include "ha/ha_alarm_panel_client.h"
#include "util/iso_time.h"
#include "ui/theme/theme_default.h"
#include "ui/ui_i18n.h"
#include "ui/ui_memory.h"

#define W_ALARM_TAG "w_alarm"
#define W_ALARM_MAX_ZONES 16
#define W_ALARM_MAX_ACTIONS 8
#define W_ALARM_LOG_MAX 40
#define W_ALARM_NAME_LEN 48
#define W_ALARM_STATE_LEN APP_MAX_STATE_LEN
#define W_ALARM_CODE_MAX 12
#define W_ALARM_HOLD_MS 2000
#define W_ALARM_CODE_CLEAR_MS 20000
#define W_ALARM_TICK_MS 100
#define W_ALARM_LOG_REFRESH_MS (30 * 60 * 1000)
#define W_ALARM_FETCH_RETRY_MS 1500
#define W_ALARM_FETCH_TIMEOUT_MS 15000
#define W_ALARM_DEFAULT_LOG_HOURS 24

/* alarm_control_panel supported_features bits (HA AlarmControlPanelEntityFeature). */
#define W_ALARM_FEAT_ARM_HOME 1U
#define W_ALARM_FEAT_ARM_AWAY 2U
#define W_ALARM_FEAT_ARM_NIGHT 4U
#define W_ALARM_FEAT_ARM_VACATION 32U

#define W_ALARM_COLOR_OK 0x2EB872
#define W_ALARM_COLOR_ARMED 0xE5484D
#define W_ALARM_COLOR_WARN 0xF5A524
#define W_ALARM_COLOR_TRIGGERED 0xFF1F3D
#define W_ALARM_COLOR_UNKNOWN 0x6B7280

typedef enum {
    W_ALARM_TONE_NEUTRAL = 0,
    W_ALARM_TONE_OK,
    W_ALARM_TONE_WARN,
    W_ALARM_TONE_ALERT,
} w_alarm_tone_t;

typedef struct {
    char entity_id[APP_MAX_ENTITY_ID_LEN];
    char name[W_ALARM_NAME_LEN];
    char state[W_ALARM_STATE_LEN];
    char device_class[24];
    bool known;
} w_alarm_slot_t;

typedef struct {
    int64_t when_s;
    char name[W_ALARM_NAME_LEN];
    char text[40];
    uint8_t tone;
} w_alarm_log_entry_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *state_pill;
    lv_obj_t *state_label;
    lv_obj_t *sub_label;
    lv_obj_t *code_label;
    lv_obj_t *msg_label;
    lv_obj_t *zone_chips[W_ALARM_MAX_ZONES];
    lv_obj_t *zone_names[W_ALARM_MAX_ZONES];
    lv_obj_t *action_btns[W_ALARM_MAX_ACTIONS];
    lv_obj_t *action_bars[W_ALARM_MAX_ACTIONS];
    lv_obj_t *action_labels[W_ALARM_MAX_ACTIONS];
    lv_obj_t *mode_btns[HA_ALARM_ACTION_COUNT];
    lv_obj_t *keypad;
    lv_obj_t *log_list;
    lv_obj_t *log_status;
    lv_timer_t *tick;
} w_alarm_page_t;

typedef struct {
    lv_obj_t *card;

    char title[APP_MAX_NAME_LEN];
    w_alarm_slot_t alarm;
    char keypad_entity_id[APP_MAX_ENTITY_ID_LEN];
    char keypad_text[64];
    char code_format[16];
    bool code_arm_required;
    uint32_t supported_features;

    w_alarm_slot_t zones[W_ALARM_MAX_ZONES];
    size_t zone_count;
    w_alarm_slot_t actions[W_ALARM_MAX_ACTIONS];
    size_t action_count;
    int log_hours;
    char attributes[APP_HA_ATTRS_MAX_LEN];
    bool visible;
    uint32_t client, history_client;
    bool show_name, connected;

    /* Built once in create(); page.root is the widget card itself. */
    w_alarm_page_t page;
    char code[W_ALARM_CODE_MAX + 1];
    size_t code_len;
    int64_t last_code_ms;
    int held_action;
    int64_t hold_start_ms;
    bool hold_fired;

    /* History shown in LVGL context. */
    w_alarm_log_entry_t log[W_ALARM_LOG_MAX];
    size_t log_count;
    bool log_loaded;
    bool fetching;
    int64_t fetch_started_ms;
    int64_t next_fetch_ms;



} w_alarm_ctx_t;

static void w_alarm_render_page_state(w_alarm_ctx_t *ctx);
static void w_alarm_render_log(w_alarm_ctx_t *ctx);
static bool w_alarm_action_is_hold(const w_alarm_slot_t *slot);
static bool w_alarm_action_available(const w_alarm_slot_t *slot)
{
    if (!slot->known || !strcmp(slot->state, "unavailable")) return false;
    /* HA buttons can be "unknown" until their first press. Switches need an on/off state. */
    return w_alarm_action_is_hold(slot) || !strcmp(slot->state, "on") || !strcmp(slot->state, "off");
}

static int64_t w_alarm_now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool w_alarm_entity_in_domain(const char *entity_id, const char *domain)
{
    size_t len = strlen(domain);
    return strncmp(entity_id, domain, len) == 0 && entity_id[len] == '.';
}

/* ----- State text / colors --------------------------------------------- */
static const char *w_alarm_state_text(const char *state)
{
    if (state == NULL || state[0] == '\0') {
        return ui_i18n_get("common.unknown", "Unknown");
    }
    if (strcmp(state, "disarmed") == 0) return ui_i18n_get("alarm.state.disarmed", "Disarmed");
    if (strcmp(state, "armed_home") == 0) return ui_i18n_get("alarm.state.armed_home", "Armed Home");
    if (strcmp(state, "armed_away") == 0) return ui_i18n_get("alarm.state.armed_away", "Armed Away");
    if (strcmp(state, "armed_night") == 0) return ui_i18n_get("alarm.state.armed_night", "Armed Night");
    if (strcmp(state, "armed_vacation") == 0) return ui_i18n_get("alarm.state.armed_vacation", "Armed Vacation");
    if (strcmp(state, "armed_custom_bypass") == 0) return ui_i18n_get("alarm.state.armed_custom_bypass", "Armed Bypass");
    if (strcmp(state, "arming") == 0) return ui_i18n_get("alarm.state.arming", "Arming");
    if (strcmp(state, "disarming") == 0) return ui_i18n_get("alarm.state.disarming", "Disarming");
    if (strcmp(state, "pending") == 0) return ui_i18n_get("alarm.state.pending", "Pending");
    if (strcmp(state, "triggered") == 0) return ui_i18n_get("alarm.state.triggered", "TRIGGERED");
    if (strcmp(state, "unavailable") == 0) return ui_i18n_get("common.unavailable", "unavailable");
    return state;
}

static uint32_t w_alarm_state_color(const char *state)
{
    if (state == NULL) return W_ALARM_COLOR_UNKNOWN;
    if (strcmp(state, "disarmed") == 0) return W_ALARM_COLOR_OK;
    if (strncmp(state, "armed_", 6) == 0) return W_ALARM_COLOR_ARMED;
    if (strcmp(state, "triggered") == 0) return W_ALARM_COLOR_TRIGGERED;
    if (strcmp(state, "arming") == 0 || strcmp(state, "pending") == 0 || strcmp(state, "disarming") == 0) {
        return W_ALARM_COLOR_WARN;
    }
    return W_ALARM_COLOR_UNKNOWN;
}

static w_alarm_tone_t w_alarm_state_tone(const char *state)
{
    uint32_t color = w_alarm_state_color(state);
    if (color == W_ALARM_COLOR_OK) return W_ALARM_TONE_OK;
    if (color == W_ALARM_COLOR_WARN) return W_ALARM_TONE_WARN;
    if (color == W_ALARM_COLOR_ARMED || color == W_ALARM_COLOR_TRIGGERED) return W_ALARM_TONE_ALERT;
    return W_ALARM_TONE_NEUTRAL;
}

static bool w_alarm_zone_is_hazard(const w_alarm_slot_t *zone)
{
    static const char *const hazard_classes[] = {
        "smoke", "gas", "carbon_monoxide", "heat", "cold", "moisture", "safety", "problem", "tamper", "vibration", "sound",
    };
    for (size_t i = 0; i < sizeof(hazard_classes) / sizeof(hazard_classes[0]); i++) {
        if (strcmp(zone->device_class, hazard_classes[i]) == 0) {
            return true;
        }
    }
    /* Fault/trouble zones (e.g. "*_fault") are hazards even without a device_class. */
    return strstr(zone->entity_id, "fault") != NULL || strstr(zone->entity_id, "trouble") != NULL;
}

static bool w_alarm_zone_active(const w_alarm_slot_t *zone)
{
    return zone->known && strcmp(zone->state, "on") == 0;
}

/* Text for a zone / action / alarm state the way HA's frontend words it. */
static const char *w_alarm_slot_state_text(const w_alarm_slot_t *slot, const char *state, w_alarm_tone_t *out_tone)
{
    w_alarm_tone_t tone = W_ALARM_TONE_NEUTRAL;
    const char *text = state;
    if (state == NULL || state[0] == '\0' || strcmp(state, "unavailable") == 0 || strcmp(state, "unknown") == 0) {
        text = ui_i18n_get("common.unavailable", "unavailable");
    } else if (w_alarm_entity_in_domain(slot->entity_id, "alarm_control_panel")) {
        text = w_alarm_state_text(state);
        tone = w_alarm_state_tone(state);
    } else if (w_alarm_entity_in_domain(slot->entity_id, "button") ||
               w_alarm_entity_in_domain(slot->entity_id, "input_button")) {
        text = ui_i18n_get("alarm.pressed", "Pressed");
        tone = W_ALARM_TONE_WARN;
    } else if (w_alarm_entity_in_domain(slot->entity_id, "binary_sensor")) {
        bool on = strcmp(state, "on") == 0;
        const char *dc = slot->device_class;
        bool opening = strcmp(dc, "door") == 0 || strcmp(dc, "window") == 0 || strcmp(dc, "opening") == 0 ||
                       strcmp(dc, "garage_door") == 0 || dc[0] == '\0';
        bool motion = strcmp(dc, "motion") == 0 || strcmp(dc, "occupancy") == 0 || strcmp(dc, "presence") == 0;
        if (w_alarm_zone_is_hazard(slot)) {
            text = on ? ui_i18n_get("alarm.zone.detected", "Detected") : ui_i18n_get("alarm.zone.ok", "OK");
            tone = on ? W_ALARM_TONE_ALERT : W_ALARM_TONE_OK;
        } else if (motion) {
            text = on ? ui_i18n_get("alarm.zone.motion", "Motion") : ui_i18n_get("alarm.zone.clear", "Clear");
            tone = on ? W_ALARM_TONE_WARN : W_ALARM_TONE_OK;
        } else if (opening) {
            text = on ? ui_i18n_get("alarm.zone.open", "Open") : ui_i18n_get("alarm.zone.closed", "Closed");
            tone = on ? W_ALARM_TONE_WARN : W_ALARM_TONE_OK;
        } else {
            text = on ? ui_i18n_get("alarm.zone.active", "Active") : ui_i18n_get("alarm.zone.ok", "OK");
            tone = on ? W_ALARM_TONE_WARN : W_ALARM_TONE_OK;
        }
    } else if (strcmp(state, "on") == 0) {
        text = ui_i18n_get("common.on", "On");
        tone = W_ALARM_TONE_OK;
    } else if (strcmp(state, "off") == 0) {
        text = ui_i18n_get("common.off", "Off");
    }
    if (out_tone != NULL) {
        *out_tone = tone;
    }
    return text;
}

static uint32_t w_alarm_tone_color(w_alarm_tone_t tone)
{
    switch (tone) {
    case W_ALARM_TONE_OK:
        return W_ALARM_COLOR_OK;
    case W_ALARM_TONE_WARN:
        return W_ALARM_COLOR_WARN;
    case W_ALARM_TONE_ALERT:
        return W_ALARM_COLOR_ARMED;
    default:
        return W_ALARM_COLOR_UNKNOWN;
    }
}

/* ----- Slot lookup ------------------------------------------------------ */
static w_alarm_slot_t *w_alarm_find_slot(w_alarm_ctx_t *ctx, const char *entity_id, int *out_zone, int *out_action)
{
    if (out_zone != NULL) *out_zone = -1;
    if (out_action != NULL) *out_action = -1;
    if (strcmp(ctx->alarm.entity_id, entity_id) == 0) {
        return &ctx->alarm;
    }
    for (size_t i = 0; i < ctx->zone_count; i++) {
        if (strcmp(ctx->zones[i].entity_id, entity_id) == 0) {
            if (out_zone != NULL) *out_zone = (int)i;
            return &ctx->zones[i];
        }
    }
    for (size_t i = 0; i < ctx->action_count; i++) {
        if (strcmp(ctx->actions[i].entity_id, entity_id) == 0) {
            if (out_action != NULL) *out_action = (int)i;
            return &ctx->actions[i];
        }
    }
    return NULL;
}

static void w_alarm_set_slot_name_from_attrs(w_alarm_slot_t *slot, cJSON *attrs)
{
    if (slot->name[0] == '\0') {
        cJSON *friendly = cJSON_GetObjectItemCaseSensitive(attrs, "friendly_name");
        if (cJSON_IsString(friendly) && friendly->valuestring != NULL) {
            snprintf(slot->name, sizeof(slot->name), "%s", friendly->valuestring);
        }
    }
    cJSON *dc = cJSON_GetObjectItemCaseSensitive(attrs, "device_class");
    if (cJSON_IsString(dc) && dc->valuestring != NULL) {
        snprintf(slot->device_class, sizeof(slot->device_class), "%s", dc->valuestring);
    }
}

static const char *w_alarm_slot_display_name(const w_alarm_slot_t *slot)
{
    if (slot->name[0] != '\0') {
        return slot->name;
    }
    const char *dot = strchr(slot->entity_id, '.');
    return dot != NULL ? dot + 1 : slot->entity_id;
}

/* ----- History ---------------------------------------------------------- */
static void w_alarm_log_push_front(w_alarm_ctx_t *ctx, const w_alarm_slot_t *slot, const char *state)
{

    if (ctx->log_count == W_ALARM_LOG_MAX) {
        ctx->log_count--;
    }
    memmove(&ctx->log[1], &ctx->log[0], sizeof(ctx->log[0]) * ctx->log_count);
    w_alarm_log_entry_t *e = &ctx->log[0];
    memset(e, 0, sizeof(*e));
    e->when_s = (int64_t)time(NULL);
    snprintf(e->name, sizeof(e->name), "%s", w_alarm_slot_display_name(slot));
    w_alarm_tone_t tone = W_ALARM_TONE_NEUTRAL;
    snprintf(e->text, sizeof(e->text), "%s", w_alarm_slot_state_text(slot, state, &tone));
    e->tone = (uint8_t)tone;
    ctx->log_count++;
}

/* Replies are drained on the UI task; no HA callback ever retains a widget. */
static void w_alarm_receive_log(w_alarm_ctx_t *ctx, cJSON *result)
{
    ctx->log_count = 0;
    for (int i = cJSON_GetArraySize(result) - 1; i >= 0 && ctx->log_count < W_ALARM_LOG_MAX; --i) {
        cJSON *item = cJSON_GetArrayItem(result, i);
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "entity_id"));
        const char *state = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "state"));
        const char *message = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "message"));
        w_alarm_slot_t *slot = id ? w_alarm_find_slot(ctx, id, NULL, NULL) : NULL;
        if (!slot || (!state && !message)) continue;
        w_alarm_log_entry_t *entry = &ctx->log[ctx->log_count++];
        memset(entry, 0, sizeof(*entry));
        cJSON *when = cJSON_GetObjectItemCaseSensitive(item, "when");
        if (cJSON_IsNumber(when)) entry->when_s = (int64_t)when->valuedouble;
        else if (cJSON_IsString(when)) (void)iso_time_parse(when->valuestring, &entry->when_s);
        snprintf(entry->name, sizeof(entry->name), "%s", w_alarm_slot_display_name(slot));
        w_alarm_tone_t tone = W_ALARM_TONE_NEUTRAL;
        snprintf(entry->text, sizeof(entry->text), "%s", state ? w_alarm_slot_state_text(slot, state, &tone) : message);
        entry->tone = tone;
    }
    ctx->log_loaded = true;
    w_alarm_render_log(ctx);
}

static void w_alarm_request_log(w_alarm_ctx_t *ctx)
{

    int64_t now_ms = w_alarm_now_ms();
    time_t now = time(NULL);
    if (now < 1700000000) {
        /* SNTP not synced yet: logbook needs an absolute start time. */
        if (ctx->page.log_status != NULL) {
            lv_label_set_text(ctx->page.log_status, ui_i18n_get("alarm.log.no_clock", "Waiting for clock sync"));
        }
        ctx->next_fetch_ms = now_ms + 5000;
        return;
    }

    cJSON *fields = cJSON_CreateObject();
    cJSON *ids = cJSON_CreateArray();
    if (fields == NULL || ids == NULL) {
        cJSON_Delete(fields);
        cJSON_Delete(ids);
        return;
    }
    time_t start = now - (time_t)ctx->log_hours * 3600;
    struct tm gm = {0};
    gmtime_r(&start, &gm);
    char start_iso[40];
    strftime(start_iso, sizeof(start_iso), "%Y-%m-%dT%H:%M:%S+00:00", &gm);
    cJSON_AddStringToObject(fields, "type", "logbook/get_events");
    cJSON_AddStringToObject(fields, "start_time", start_iso);
    cJSON_AddItemToArray(ids, cJSON_CreateString(ctx->alarm.entity_id));
    for (size_t i = 0; i < ctx->zone_count; i++) {
        cJSON_AddItemToArray(ids, cJSON_CreateString(ctx->zones[i].entity_id));
    }
    for (size_t i = 0; i < ctx->action_count; i++) {
        cJSON_AddItemToArray(ids, cJSON_CreateString(ctx->actions[i].entity_id));
    }
    cJSON_AddItemToObject(fields, "entity_ids", ids);

    ctx->fetching = true;
    ctx->fetch_started_ms = now_ms;
    esp_err_t err = ha_alarm_panel_submit(ctx->history_client, fields, true);
    if (err != ESP_OK) {
        ctx->fetching = false;
        ctx->next_fetch_ms = now_ms + W_ALARM_FETCH_RETRY_MS;
        if (err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(W_ALARM_TAG, "logbook request failed: %s", esp_err_to_name(err));
        }
    }
}

static void w_alarm_drain_staging(w_alarm_ctx_t *ctx)
{
    bool success = false, history = false;
    cJSON *reply = NULL;
    for (unsigned i = 0; i < 2; ++i) {
    if (!ha_alarm_panel_poll(i ? ctx->history_client : ctx->client, &success, &history, &reply)) continue;
    if (history) {
        ctx->fetching = false;
        lv_label_set_text(ctx->page.log_status, success ? "" : ui_i18n_get("alarm.log.failed", "History unavailable"));
        if (success) w_alarm_receive_log(ctx, reply);
        else ctx->next_fetch_ms = w_alarm_now_ms() + 30000;
    } else {
        lv_label_set_text(ctx->page.msg_label, success ? ui_i18n_get("alarm.msg.accepted", "Request accepted") :
            ui_i18n_get("alarm.msg.rejected", "Request failed or rejected"));
    }
    cJSON_Delete(reply);
    }
}

static void w_alarm_set_message(w_alarm_ctx_t *ctx, const char *text, lv_color_t color)
{
    if (ctx->page.msg_label != NULL) {
        lv_label_set_text(ctx->page.msg_label, text);
        lv_obj_set_style_text_color(ctx->page.msg_label, color, LV_PART_MAIN);
    }
}

static bool w_alarm_call(w_alarm_ctx_t *ctx, const char *domain, const char *service, const char *entity_id,
    const char *code)
{
    if (!ctx->visible || !ha_client_is_connected()) return false;
    cJSON *command = cJSON_CreateObject();
    if (!command) return false;
    cJSON *data = cJSON_AddObjectToObject(command, "service_data");
    if (!data || !cJSON_AddStringToObject(command, "type", "call_service") ||
        !cJSON_AddStringToObject(command, "domain", domain) || !cJSON_AddStringToObject(command, "service", service) ||
        !cJSON_AddStringToObject(data, "entity_id", entity_id) ||
        (code && code[0] && !cJSON_AddStringToObject(data, "code", code))) {
        cJSON_Delete(command); return false;
    }
    return ha_alarm_panel_submit(ctx->client, command, false) == ESP_OK;
}

static void w_alarm_clear_code(w_alarm_ctx_t *ctx)
{
    memset(ctx->code, 0, sizeof(ctx->code));
    ctx->code_len = 0;
}

static bool w_alarm_code_needed(const w_alarm_ctx_t *ctx, bool arming)
{
    if (ctx->code_format[0] == '\0') {
        return false;
    }
    return arming ? ctx->code_arm_required : true;
}

static void w_alarm_run_alarm_service(w_alarm_ctx_t *ctx, const char *service, bool arming, const char *label)
{
    if (!ctx->visible || !ha_client_is_connected() || ha_alarm_state_parse(ctx->alarm.state) == HA_ALARM_UNKNOWN) {
        w_alarm_set_message(ctx, ui_i18n_get("common.unavailable", "unavailable"), lv_color_hex(W_ALARM_COLOR_ARMED));
        return;
    }
    if (strcmp(ctx->code_format, "text") == 0 && w_alarm_code_needed(ctx, arming)) {
        w_alarm_set_message(ctx, ui_i18n_get("alarm.text_code", "Text code required: use Home Assistant"), theme_default_color_text_muted());
        w_alarm_clear_code(ctx);
        return;
    }
    if (w_alarm_code_needed(ctx, arming) && ctx->code_len == 0) {
        w_alarm_set_message(ctx, ui_i18n_get("alarm.msg.enter_code", "Enter code first"), lv_color_hex(W_ALARM_COLOR_WARN));
        return;
    }
    bool ok = w_alarm_call(ctx, "alarm_control_panel", service, ctx->alarm.entity_id, ctx->code);
    w_alarm_clear_code(ctx);
    char msg[64];
    snprintf(msg, sizeof(msg), "%s: %s", ok ? ui_i18n_get("alarm.msg.sent", "Sent") : ui_i18n_get("alarm.msg.failed", "Failed"),
        label);
    w_alarm_set_message(ctx, msg, ok ? theme_default_color_text_muted() : lv_color_hex(W_ALARM_COLOR_ARMED));
    w_alarm_render_page_state(ctx);
}

/* ----- Page rendering --------------------------------------------------- */
static void w_alarm_style_pill(lv_obj_t *pill, uint32_t color)
{
    lv_obj_set_style_bg_color(pill, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, LV_PART_MAIN);
}

static void w_alarm_render_summary(w_alarm_ctx_t *ctx)
{
    if (ctx->page.sub_label == NULL) {
        return;
    }
    char sub[96];
    size_t open = 0, unknown = 0;
    const char *first_open = NULL;
    for (size_t i = 0; i < ctx->zone_count; i++) {
        if (!ctx->zones[i].known || !strcmp(ctx->zones[i].state, "unknown") || !strcmp(ctx->zones[i].state, "unavailable")) ++unknown;
        if (w_alarm_zone_active(&ctx->zones[i])) {
            if (open == 0) {
                first_open = w_alarm_slot_display_name(&ctx->zones[i]);
            }
            open++;
        }
    }
    if (ctx->keypad_text[0] != '\0') {
        snprintf(sub, sizeof(sub), "%s", ctx->keypad_text);
    } else if (open == 1) {
        snprintf(sub, sizeof(sub), "%s: %s", ui_i18n_get("alarm.zone.not_ready", "Not ready"), first_open);
    } else if (open > 1) {
        snprintf(sub, sizeof(sub), "%u %s", (unsigned)open, ui_i18n_get("alarm.zones_active", "zones active"));
    } else if (unknown) {
        snprintf(sub, sizeof(sub), "%s", ui_i18n_get("common.unavailable", "unavailable"));
    } else if (ctx->zone_count > 0) {
        snprintf(sub, sizeof(sub), "%s", ui_i18n_get("alarm.zones_ready", "Ready - all zones clear"));
    } else {
        sub[0] = '\0';
    }
    lv_label_set_text(ctx->page.sub_label, sub);
}

static void w_alarm_render_code(w_alarm_ctx_t *ctx)
{
    if (ctx->page.code_label == NULL) {
        return;
    }
    if (ctx->code_len == 0) {
        lv_label_set_text(ctx->page.code_label,
            ctx->code_format[0] != '\0' ? ui_i18n_get("alarm.enter_code", "Enter code")
                                        : ui_i18n_get("alarm.no_code", "No code required"));
        lv_obj_set_style_text_color(ctx->page.code_label, theme_default_color_text_muted(), LV_PART_MAIN);
        return;
    }
    char masked[W_ALARM_CODE_MAX * 2 + 1] = {0};
    for (size_t i = 0; i < ctx->code_len; i++) {
        masked[i * 2] = '*';
        masked[i * 2 + 1] = ' ';
    }
    lv_label_set_text(ctx->page.code_label, masked);
    lv_obj_set_style_text_color(ctx->page.code_label, theme_default_color_text_primary(), LV_PART_MAIN);
}

static void w_alarm_render_page_state(w_alarm_ctx_t *ctx)
{

    w_alarm_page_t *page = &ctx->page;
    if (page->root == NULL) {
        return;
    }
    const char *state = ctx->alarm.known ? ctx->alarm.state : "";
    lv_label_set_text(page->state_label, w_alarm_state_text(state));
    w_alarm_style_pill(page->state_pill, w_alarm_state_color(state));
    w_alarm_render_summary(ctx);
    w_alarm_render_code(ctx);
    static const ha_alarm_action_t actions[] = {HA_ALARM_ACTION_DISARM, HA_ALARM_ACTION_ARM_AWAY,
        HA_ALARM_ACTION_ARM_HOME, HA_ALARM_ACTION_ARM_NIGHT, HA_ALARM_ACTION_ARM_VACATION, HA_ALARM_ACTION_ARM_CUSTOM_BYPASS};
    ha_alarm_action_t allowed[HA_ALARM_ACTION_COUNT];
    size_t count = ha_alarm_actions(ha_alarm_state_parse(state), ctx->attributes, allowed, HA_ALARM_ACTION_COUNT);
    for (unsigned i = 0; i < HA_ALARM_ACTION_COUNT; ++i) {
        bool enabled = false;
        for (size_t j = 0; j < count; ++j) if (allowed[j] == actions[i]) enabled = true;
        enabled = enabled && ctx->visible && ha_client_is_connected();
        if (page->mode_btns[i]) ui_style_action_button(page->mode_btns[i], lv_obj_get_child(page->mode_btns[i], 0), UI_CONTROL_ACTION, enabled);
    }

    for (size_t i = 0; i < ctx->zone_count; i++) {
        if (page->zone_chips[i] == NULL) {
            continue;
        }
        lv_label_set_text(page->zone_names[i], w_alarm_slot_display_name(&ctx->zones[i]));
        w_alarm_tone_t tone = W_ALARM_TONE_NEUTRAL;
        const char *text = w_alarm_slot_state_text(&ctx->zones[i], ctx->zones[i].known ? ctx->zones[i].state : "", &tone);
        lv_label_set_text(page->zone_chips[i], text);
        bool active = w_alarm_zone_active(&ctx->zones[i]);
        lv_obj_set_style_bg_opa(page->zone_chips[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_color(page->zone_chips[i], lv_color_hex(w_alarm_tone_color(tone)), LV_PART_MAIN);
        lv_obj_set_style_text_color(page->zone_chips[i],
            active ? lv_color_hex(0xFFFFFF) : lv_color_hex(w_alarm_tone_color(tone)), LV_PART_MAIN);
    }

    for (size_t i = 0; i < ctx->action_count; i++) {
        if (page->action_labels[i] == NULL) {
            continue;
        }
        bool available = w_alarm_action_available(&ctx->actions[i]) && ha_client_is_connected();
        bool on = ctx->actions[i].known && strcmp(ctx->actions[i].state, "on") == 0;
        ui_style_action_button(page->action_btns[i], page->action_labels[i], on ? UI_CONTROL_ACTIVE : UI_CONTROL_ACTION, available && ctx->visible);
        if (w_alarm_action_is_hold(&ctx->actions[i])) {
            lv_label_set_text_fmt(page->action_labels[i], "%s\n%s", w_alarm_slot_display_name(&ctx->actions[i]),
                ui_i18n_get("alarm.hold", "hold 2 s"));
            continue;
        }
        char text[80];
        snprintf(text, sizeof(text), "%s: %s", w_alarm_slot_display_name(&ctx->actions[i]),
            w_alarm_action_available(&ctx->actions[i]) ? (on ? ui_i18n_get("common.on", "On") : ui_i18n_get("common.off", "Off"))
                                  : ui_i18n_get("common.unavailable", "unavailable"));
        lv_label_set_text(page->action_labels[i], text);

    }
}

static void w_alarm_render_log(w_alarm_ctx_t *ctx)
{

    w_alarm_page_t *page = &ctx->page;
    if (page->log_list == NULL) {
        return;
    }
    lv_obj_clean(page->log_list);
    if (ctx->log_count == 0) {
        lv_obj_t *empty = lv_label_create(page->log_list);
        lv_label_set_text(empty, ctx->log_loaded ? ui_i18n_get("alarm.log.empty", "No events") : "...");
        lv_obj_set_style_text_color(empty, theme_default_color_text_muted(), LV_PART_MAIN);
        ui_text_set(empty, UI_TEXT_LABEL, false);
        return;
    }

    time_t now = time(NULL);
    struct tm today = {0};
    localtime_r(&now, &today);
    for (size_t i = 0; i < ctx->log_count; i++) {
        const w_alarm_log_entry_t *e = &ctx->log[i];
        lv_obj_t *row = lv_obj_create(page->log_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, 6, LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(APP_UI_COLOR_CARD_BORDER), LV_PART_MAIN);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 10, LV_PART_MAIN);
        lv_obj_set_clickable(row, false);

        char when_text[24] = "--:--";
        if (e->when_s > 0) {
            time_t t = (time_t)e->when_s;
            struct tm lt = {0};
            localtime_r(&t, &lt);
            bool same_day = lt.tm_yday == today.tm_yday && lt.tm_year == today.tm_year;
            strftime(when_text, sizeof(when_text), same_day ? "%H:%M" : "%a %H:%M", &lt);
        }
        lv_obj_t *when = lv_label_create(row);
        lv_label_set_text(when, when_text);
        lv_obj_set_width(when, 96);
        ui_text_set(when, UI_TEXT_META, false);
        lv_obj_set_style_text_color(when, theme_default_color_text_muted(), LV_PART_MAIN);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, e->name);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name, 1);
        ui_text_set(name, UI_TEXT_LABEL, false);
        lv_obj_set_style_text_color(name, theme_default_color_text_primary(), LV_PART_MAIN);

        lv_obj_t *text = lv_label_create(row);
        lv_label_set_text(text, e->text);
        ui_text_set(text, UI_TEXT_LABEL, false);
        lv_obj_set_style_text_color(text, lv_color_hex(w_alarm_tone_color((w_alarm_tone_t)e->tone)), LV_PART_MAIN);
    }
}

/* ----- Page events ------------------------------------------------------ */
static void w_alarm_keypad_event_cb(lv_event_t *event)
{

    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)lv_event_get_user_data(event);
    lv_obj_t *btnm = lv_event_get_target(event);
    uint32_t idx = lv_buttonmatrix_get_selected_button(btnm);
    if (ctx == NULL || !ctx->visible || !ha_client_is_connected() || idx == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }
    const char *txt = lv_buttonmatrix_get_button_text(btnm, idx);
    if (txt == NULL) {
        return;
    }
    ctx->last_code_ms = w_alarm_now_ms();
    if (strcmp(txt, "C") == 0) {
        w_alarm_clear_code(ctx);
    } else if (strcmp(txt, "<") == 0) {
        if (ctx->code_len > 0) {
            ctx->code[--ctx->code_len] = '\0';
        }
    } else if (txt[0] >= '0' && txt[0] <= '9' && txt[1] == '\0' && ctx->code_len < W_ALARM_CODE_MAX) {
        ctx->code[ctx->code_len++] = txt[0];
        ctx->code[ctx->code_len] = '\0';
    }
    w_alarm_set_message(ctx, "", theme_default_color_text_muted());
    w_alarm_render_code(ctx);
}

typedef struct {
    const char *service;
    const char *label_key;
    const char *label_fallback;
    bool arming;
} w_alarm_mode_t;

static const w_alarm_mode_t s_alarm_modes[] = {
    {"alarm_disarm", "alarm.btn.disarm", "Disarm", false},
    {"alarm_arm_away", "alarm.btn.away", "Arm Away", true},
    {"alarm_arm_home", "alarm.btn.home", "Arm Home", true},
    {"alarm_arm_night", "alarm.btn.night", "Arm Night", true},
    {"alarm_arm_vacation", "alarm.btn.vacation", "Vacation", true},
    {"alarm_arm_custom_bypass", "alarm.action.arm_custom_bypass", "Custom bypass", true},
};

static void w_alarm_mode_event_cb(lv_event_t *event)
{

    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)lv_event_get_user_data(event);
    lv_obj_t *btn = lv_event_get_target(event);
    if (ctx == NULL) {
        return;
    }
    uintptr_t mode = (uintptr_t)lv_obj_get_user_data(btn);
    if (mode >= sizeof(s_alarm_modes) / sizeof(s_alarm_modes[0])) {
        return;
    }
    ha_alarm_action_t allowed[HA_ALARM_ACTION_COUNT];
    size_t count = ha_alarm_actions(ha_alarm_state_parse(ctx->alarm.state), ctx->attributes, allowed, HA_ALARM_ACTION_COUNT);
    bool can_send = false;
    for (size_t i = 0; i < count; ++i) if (!strcmp(ha_alarm_action_service(allowed[i]), s_alarm_modes[mode].service)) can_send = true;
    if (!can_send) return;
    const w_alarm_mode_t *m = &s_alarm_modes[mode];
    w_alarm_run_alarm_service(ctx, m->service, m->arming, ui_i18n_get(m->label_key, m->label_fallback));
}

static bool w_alarm_action_is_hold(const w_alarm_slot_t *slot)
{
    return w_alarm_entity_in_domain(slot->entity_id, "button") || w_alarm_entity_in_domain(slot->entity_id, "input_button");
}

static void w_alarm_action_event_cb(lv_event_t *event)
{

    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)lv_event_get_user_data(event);
    lv_obj_t *btn = lv_event_get_current_target(event);
    if (ctx == NULL) {
        return;
    }
    int idx = (int)(uintptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || (size_t)idx >= ctx->action_count) {
        return;
    }
    w_alarm_slot_t *slot = &ctx->actions[idx];
    lv_event_code_t code = lv_event_get_code(event);
    if (!ctx->visible || !ha_client_is_connected() || !w_alarm_action_available(slot)) {
        ctx->held_action = -1; return;
    }

    if (!w_alarm_action_is_hold(slot)) {
        if (code == LV_EVENT_CLICKED) {
            const char *domain = w_alarm_entity_in_domain(slot->entity_id, "input_boolean") ? "input_boolean" : "switch";
            if (!w_alarm_call(ctx, domain, !strcmp(slot->state, "on") ? "turn_off" : "turn_on", slot->entity_id, NULL)) {

                w_alarm_set_message(ctx, ui_i18n_get("alarm.msg.failed", "Failed"), lv_color_hex(W_ALARM_COLOR_ARMED));
            }
        }
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        ctx->held_action = idx;
        ctx->hold_start_ms = w_alarm_now_ms();
        ctx->hold_fired = false;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (ctx->held_action == idx) {
            ctx->held_action = -1;
            if (ctx->page.action_bars[idx] != NULL) {
                lv_bar_set_value(ctx->page.action_bars[idx], 0, LV_ANIM_OFF);
            }
        }
    }
}

static void w_alarm_update_hold(w_alarm_ctx_t *ctx, int64_t now)
{
    if (!ctx->visible || !ha_client_is_connected()) { ctx->held_action = -1; return; }
    int idx = ctx->held_action;
    if (idx < 0 || (size_t)idx >= ctx->action_count || ctx->page.action_bars[idx] == NULL) {
        return;
    }
    if (!w_alarm_action_available(&ctx->actions[idx])) { ctx->held_action = -1; return; }
    int64_t elapsed = now - ctx->hold_start_ms;
    int pct = (int)((elapsed * 100) / W_ALARM_HOLD_MS);
    lv_bar_set_value(ctx->page.action_bars[idx], pct > 100 ? 100 : pct, LV_ANIM_OFF);
    if (elapsed < W_ALARM_HOLD_MS || ctx->hold_fired) {
        return;
    }
    ctx->hold_fired = true;
    w_alarm_slot_t *slot = &ctx->actions[idx];
    const char *domain = w_alarm_entity_in_domain(slot->entity_id, "input_button") ? "input_button" : "button";
    esp_err_t err = w_alarm_call(ctx, domain, "press", slot->entity_id, NULL) ? ESP_OK : ESP_FAIL;

    char msg[80];
    snprintf(msg, sizeof(msg), "%s: %s", err == ESP_OK ? ui_i18n_get("alarm.msg.sent", "Sent") : ui_i18n_get("alarm.msg.failed", "Failed"),
        w_alarm_slot_display_name(slot));
    w_alarm_set_message(ctx, msg, lv_color_hex(err == ESP_OK ? W_ALARM_COLOR_WARN : W_ALARM_COLOR_ARMED));
    ESP_LOGW(W_ALARM_TAG, "Held action fired: %s (%s)", slot->entity_id, esp_err_to_name(err));
}

static void w_alarm_page_tick_cb(lv_timer_t *timer)
{

    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)lv_timer_get_user_data(timer);
    if (ctx == NULL || ctx->page.root == NULL) {
        return;
    }
    if (!ctx->visible) return;
    ui_fonts_tile_begin("alarm_panel", lv_obj_get_width(ctx->card), lv_obj_get_height(ctx->card));
    int64_t now = w_alarm_now_ms();
    bool connected = ha_client_is_connected();
    if (connected != ctx->connected) {
        ctx->connected = connected;
        if (!connected) {
            w_alarm_clear_code(ctx);
            ctx->held_action = -1;
            ha_alarm_panel_cancel(ctx->client);
            ha_alarm_panel_cancel(ctx->history_client);
            ctx->fetching = false;
            ctx->next_fetch_ms = 0;
            for (size_t i = 0; i < ctx->action_count; ++i)
                if (ctx->page.action_bars[i]) lv_bar_set_value(ctx->page.action_bars[i], 0, LV_ANIM_OFF);
        }
        w_alarm_render_page_state(ctx);
    }

    w_alarm_update_hold(ctx, now);
    w_alarm_drain_staging(ctx);

    if (ctx->fetching && (now - ctx->fetch_started_ms) > W_ALARM_FETCH_TIMEOUT_MS) {
        ctx->fetching = false;
        ctx->next_fetch_ms = now + W_ALARM_FETCH_RETRY_MS * 10;
    }
    /* Live state changes extend the list; a periodic re-sync picks up anything
     * missed while HA was disconnected. Wait for the initial subscribe burst. */
    if (!ctx->fetching && now >= ctx->next_fetch_ms && ha_client_is_connected() &&
        ha_client_is_initial_sync_done()) {
        ctx->next_fetch_ms = now + W_ALARM_LOG_REFRESH_MS;
        w_alarm_request_log(ctx);
    }

    if (ctx->code_len > 0 && (now - ctx->last_code_ms) > W_ALARM_CODE_CLEAR_MS) {
        w_alarm_clear_code(ctx);
        w_alarm_render_code(ctx);
    }

    ui_fonts_tile_end();
}

static void w_alarm_refresh_log_event_cb(lv_event_t *event)
{
    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)lv_event_get_user_data(event);
    if (ctx == NULL || ctx->fetching) {
        return;
    }
    ctx->next_fetch_ms = 0;
    if (ctx->page.log_status != NULL) {
        lv_label_set_text(ctx->page.log_status, ui_i18n_get("alarm.log.loading", "Loading..."));
    }
}

/* ----- Page construction ------------------------------------------------ */
static lv_obj_t *w_alarm_column(lv_obj_t *parent, int grow)
{
    lv_obj_t *col = lv_obj_create(parent);
    theme_default_style_card(col);
    ui_style_apply_card(col);
    lv_obj_set_width(col, 0);
    lv_obj_set_height(col, LV_PCT(100));
    lv_obj_set_flex_grow(col, grow);
    lv_obj_set_style_pad_all(col, 16, LV_PART_MAIN);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 10, LV_PART_MAIN);
    lv_obj_set_scrollable(col, true);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);
    return col;
}

static lv_obj_t *w_alarm_heading(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    ui_text_set(label, UI_TEXT_LABEL, false);
    lv_obj_set_style_text_color(label, theme_default_color_text_muted(), LV_PART_MAIN);
    return label;
}

static lv_obj_t *w_alarm_button(lv_obj_t *parent, const char *text, uint32_t bg, const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(btn, 48, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(btn, ui_style_radius(12), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    ui_style_action_button(btn, label, UI_CONTROL_ACTION, true);
    lv_obj_center(label);
    return btn;
}

static void w_alarm_build_keypad_column(w_alarm_ctx_t *ctx, lv_obj_t *col)
{
    w_alarm_page_t *page = &ctx->page;
    if (ctx->show_name) w_alarm_heading(col, ctx->title);

    lv_obj_t *header = lv_obj_create(col);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 12, LV_PART_MAIN);

    page->state_pill = lv_obj_create(header);
    lv_obj_remove_style_all(page->state_pill);
    lv_obj_set_size(page->state_pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(page->state_pill, ui_style_radius(12), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(page->state_pill, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(page->state_pill, 6, LV_PART_MAIN);
    page->state_label = lv_label_create(page->state_pill);
    ui_text_set(page->state_label, UI_TEXT_LABEL, false);
    lv_obj_set_style_text_color(page->state_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    page->sub_label = lv_label_create(header);
    lv_label_set_long_mode(page->sub_label, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(page->sub_label, 1);
    ui_text_set(page->sub_label, UI_TEXT_LABEL, false);
    lv_obj_set_style_text_color(page->sub_label, theme_default_color_text_muted(), LV_PART_MAIN);

    page->code_label = lv_label_create(col);
    lv_obj_set_width(page->code_label, LV_PCT(100));
    lv_obj_set_style_text_align(page->code_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    ui_text_set(page->code_label, UI_TEXT_VALUE, false);
    lv_obj_set_style_pad_ver(page->code_label, 6, LV_PART_MAIN);

    static const char *const keypad_map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        "C", "0", "<", "",
    };
    lv_obj_t *keypad = lv_buttonmatrix_create(col);
    page->keypad = keypad;
    lv_obj_set_style_min_height(keypad, 200, LV_PART_MAIN);
    lv_buttonmatrix_set_map(keypad, keypad_map);
    lv_obj_set_width(keypad, LV_PCT(100));
    lv_obj_set_flex_grow(keypad, 1);
    lv_obj_set_style_bg_opa(keypad, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(keypad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(keypad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(keypad, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(keypad, ui_style_radius(12), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keypad, lv_color_hex(APP_UI_COLOR_TOPBAR_CHIP_BG), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keypad, theme_default_color_text_primary(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(keypad, ui_font_role(UI_TEXT_VALUE), LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(keypad, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(keypad, w_alarm_keypad_event_cb, LV_EVENT_VALUE_CHANGED, ctx);

    lv_obj_t *modes = lv_obj_create(col);
    lv_obj_remove_style_all(modes);
    lv_obj_set_size(modes, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(modes, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(modes, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(modes, 10, LV_PART_MAIN);

    for (size_t i = 0; i < sizeof(s_alarm_modes) / sizeof(s_alarm_modes[0]); i++) {

        lv_obj_t *btn = w_alarm_button(modes, ui_i18n_get(s_alarm_modes[i].label_key, s_alarm_modes[i].label_fallback),
            i == 0 ? W_ALARM_COLOR_OK : W_ALARM_COLOR_ARMED, i == 0 ? ui_font_role(UI_TEXT_LABEL) : ui_font_role(UI_TEXT_META));
        /* The longer custom-bypass label needs its own row, including in German. */
        bool full_row = i == 0 || i == HA_ALARM_ACTION_COUNT - 1;
        lv_obj_set_width(btn, full_row ? LV_PCT(100) : LV_PCT(46));
        lv_obj_set_flex_grow(btn, 0);
        page->mode_btns[i] = btn;
        lv_obj_set_user_data(btn, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(btn, w_alarm_mode_event_cb, LV_EVENT_CLICKED, ctx);
    }

    page->msg_label = lv_label_create(col);
    lv_label_set_text(page->msg_label, "");
    lv_obj_set_width(page->msg_label, LV_PCT(100));
    lv_obj_set_style_text_align(page->msg_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    ui_text_set(page->msg_label, UI_TEXT_LABEL, false);
}

static void w_alarm_build_zone_column(w_alarm_ctx_t *ctx, lv_obj_t *col)
{
    w_alarm_page_t *page = &ctx->page;
    w_alarm_heading(col, ui_i18n_get("alarm.zones", "Zones"));

    lv_obj_t *list = lv_obj_create(col);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, LV_PART_MAIN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    for (size_t i = 0; i < ctx->zone_count; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_clickable(row, false);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, w_alarm_slot_display_name(&ctx->zones[i]));
        page->zone_names[i] = name;
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name, 1);
        ui_text_set(name, UI_TEXT_LABEL, false);
        lv_obj_set_style_text_color(name, theme_default_color_text_primary(), LV_PART_MAIN);

        lv_obj_t *chip = lv_label_create(row);
        ui_text_set(chip, UI_TEXT_LABEL, false);
        lv_obj_set_style_radius(chip, ui_style_radius(12), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(chip, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(chip, 4, LV_PART_MAIN);
        page->zone_chips[i] = chip;
    }

    if (ctx->action_count == 0) {
        return;
    }
    w_alarm_heading(col, ui_i18n_get("alarm.controls", "Controls"));
    lv_obj_t *actions = lv_obj_create(col);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(actions, 8, LV_PART_MAIN);
    lv_obj_set_scrollable(actions, false);

    for (size_t i = 0; i < ctx->action_count; i++) {
        bool hold = w_alarm_action_is_hold(&ctx->actions[i]);
        lv_obj_t *btn = lv_button_create(actions);
        lv_obj_set_size(btn, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(btn, 52, LV_PART_MAIN);
        lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_radius(btn, ui_style_radius(12), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 6, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_hex(hold ? 0x8A1C2B : APP_UI_COLOR_TOPBAR_CHIP_BG), LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(btn, w_alarm_action_event_cb, LV_EVENT_ALL, ctx);

        lv_obj_t *label = lv_label_create(btn);
        ui_text_set(label, UI_TEXT_LABEL, false);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_clickable(label, false);
        if (hold) {
            char text[80];
            snprintf(text, sizeof(text), "%s\n%s", w_alarm_slot_display_name(&ctx->actions[i]),
                ui_i18n_get("alarm.hold", "hold 2 s"));
            lv_label_set_text(label, text);
            lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_t *bar = lv_bar_create(btn);
            lv_obj_set_size(bar, LV_PCT(100), 6);
            lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_bar_set_range(bar, 0, 100);
            lv_bar_set_value(bar, 0, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bar, LV_OPA_30, LV_PART_MAIN);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
            lv_obj_set_clickable(bar, false);
            page->action_bars[i] = bar;
        } else {
            lv_obj_center(label);
        }
        page->action_btns[i] = btn;
        page->action_labels[i] = label;
    }
}

static void w_alarm_build_log_column(w_alarm_ctx_t *ctx, lv_obj_t *col)
{
    w_alarm_page_t *page = &ctx->page;

    lv_obj_t *header = lv_obj_create(col);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(header, 8, LV_PART_MAIN);

    char heading[48];
    snprintf(heading, sizeof(heading), "%s (%dh)", ui_i18n_get("alarm.history", "History"), ctx->log_hours);
    w_alarm_heading(header, heading);

    lv_obj_t *refresh = w_alarm_button(header, ui_i18n_get("alarm.refresh", "Refresh"), APP_UI_COLOR_TOPBAR_CHIP_BG,
        ui_font_role(UI_TEXT_LABEL));
    lv_point_t refresh_size;
    const char *refresh_text = lv_label_get_text(lv_obj_get_child(refresh, 0));
    lv_text_get_size(&refresh_size, refresh_text, ui_font_role(UI_TEXT_LABEL), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(refresh, refresh_size.x + 24);
    lv_obj_add_event_cb(refresh, w_alarm_refresh_log_event_cb, LV_EVENT_CLICKED, ctx);

    page->log_status = lv_label_create(col);
    lv_label_set_text(page->log_status, ui_i18n_get("alarm.log.loading", "Loading..."));
    ui_text_set(page->log_status, UI_TEXT_META, false);
    lv_obj_set_style_text_color(page->log_status, theme_default_color_text_muted(), LV_PART_MAIN);

    page->log_list = lv_obj_create(col);
    lv_obj_remove_style_all(page->log_list);
    lv_obj_set_width(page->log_list, LV_PCT(100));
    lv_obj_set_flex_grow(page->log_list, 1);
    lv_obj_set_layout(page->log_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(page->log_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(page->log_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page->log_list, LV_SCROLLBAR_MODE_AUTO);
}

/* ----- Lifecycle -------------------------------------------------------- */
static void w_alarm_card_event_cb(lv_event_t *event)
{
    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)lv_event_get_user_data(event);
    if (ctx == NULL || lv_event_get_code(event) != LV_EVENT_DELETE) {
        return;
    }
    ha_alarm_panel_close(ctx->client);
    ha_alarm_panel_close(ctx->history_client);
    if (ctx->page.tick) lv_timer_delete(ctx->page.tick);
    w_alarm_clear_code(ctx);

    free(ctx);
}

static size_t w_alarm_parse_slots(const cJSON *json, const char *key, w_alarm_slot_t *slots, size_t max_slots)
{
    const cJSON *list = (json != NULL) ? cJSON_GetObjectItemCaseSensitive(json, key) : NULL;
    if (!cJSON_IsArray(list)) {
        return 0;
    }
    size_t count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, list) {
        if (count >= max_slots) {
            break;
        }
        const cJSON *entity = cJSON_GetObjectItemCaseSensitive(item, "entity_id");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        if (!cJSON_IsString(entity) || entity->valuestring == NULL || strchr(entity->valuestring, '.') == NULL) {
            continue;
        }
        w_alarm_slot_t *slot = &slots[count++];
        snprintf(slot->entity_id, sizeof(slot->entity_id), "%s", entity->valuestring);
        if (cJSON_IsString(name) && name->valuestring != NULL) {
            snprintf(slot->name, sizeof(slot->name), "%s", name->valuestring);
        }
    }
    return count;
}

esp_err_t w_alarm_panel_create(const ui_widget_def_t *def, lv_obj_t *parent, ui_widget_instance_t *out_instance)
{
    if (def == NULL || parent == NULL || out_instance == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    w_alarm_ctx_t *ctx = ui_calloc_prefer_psram(1, sizeof(w_alarm_ctx_t));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->client = ha_alarm_panel_open();
    if (!ctx->client) { free(ctx); return ESP_ERR_NO_MEM; }
    ctx->history_client = ha_alarm_panel_open();
    if (!ctx->history_client) { ha_alarm_panel_close(ctx->client); free(ctx); return ESP_ERR_NO_MEM; }
    ctx->visible = true;
    ctx->connected = ha_client_is_connected();
    ctx->show_name = !def->hide_name;

    snprintf(ctx->title, sizeof(ctx->title), "%s", def->title[0] ? def->title : def->id);
    snprintf(ctx->alarm.entity_id, sizeof(ctx->alarm.entity_id), "%s", def->entity_id);
    snprintf(ctx->keypad_entity_id, sizeof(ctx->keypad_entity_id), "%s", def->secondary_entity_id);
    ctx->zone_count = w_alarm_parse_slots(def->alarm_config, "alarm_zones", ctx->zones, W_ALARM_MAX_ZONES);
    ctx->action_count = w_alarm_parse_slots(def->alarm_config, "alarm_actions", ctx->actions, W_ALARM_MAX_ACTIONS);
    const cJSON *hours = (def->alarm_config != NULL) ? cJSON_GetObjectItemCaseSensitive(def->alarm_config, "alarm_log_hours") : NULL;
    ctx->log_hours = (cJSON_IsNumber(hours) && hours->valueint >= 1 && hours->valueint <= 168) ? hours->valueint
                                                                                              : W_ALARM_DEFAULT_LOG_HOURS;
    ctx->held_action = -1;

    /* Transparent container; each column is its own card. */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, def->x, def->y);
    lv_obj_set_size(card, def->w, def->h);
    lv_obj_set_scrollable(card, false);
    lv_obj_set_style_pad_column(card, 12, LV_PART_MAIN);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);

    ctx->card = card;
    ctx->page.root = card;
    lv_obj_add_event_cb(card, w_alarm_card_event_cb, LV_EVENT_DELETE, ctx);

    lv_obj_t *columns[3] = {w_alarm_column(card, 4), w_alarm_column(card, 4), w_alarm_column(card, 5)};
    if (def->w < 900) {
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(card, true);
        lv_obj_set_scroll_dir(card, LV_DIR_VER);
        for (unsigned i = 0; i < 3; ++i) {
            lv_obj_set_flex_grow(columns[i], 0);
            lv_obj_set_size(columns[i], LV_PCT(100), i == 0 ? 600 : 380);
        }
    }
    w_alarm_build_keypad_column(ctx, columns[0]);
    w_alarm_build_zone_column(ctx, columns[1]);
    w_alarm_build_log_column(ctx, columns[2]);
    ctx->page.tick = lv_timer_create(w_alarm_page_tick_cb, W_ALARM_TICK_MS, ctx);
    /* First history fetch waits for the HA initial sync (see tick). */
    ctx->next_fetch_ms = w_alarm_now_ms() + 3000;

    w_alarm_render_page_state(ctx);
    w_alarm_render_log(ctx);
    out_instance->obj = card;
    out_instance->ctx = ctx;
    return ESP_OK;
}


void w_alarm_panel_apply_state(ui_widget_instance_t *instance, const ha_state_t *state)
{

    if (instance == NULL || instance->ctx == NULL || state == NULL) {
        return;
    }
    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)instance->ctx;

    if (ctx->keypad_entity_id[0] != '\0' && strcmp(state->entity_id, ctx->keypad_entity_id) == 0) {
        snprintf(ctx->keypad_text, sizeof(ctx->keypad_text), "%s",
            strcmp(state->state, "unavailable") == 0 ? "" : state->state);
        w_alarm_render_summary(ctx);
        return;
    }

    int zone_idx = -1;
    int action_idx = -1;
    w_alarm_slot_t *slot = w_alarm_find_slot(ctx, state->entity_id, &zone_idx, &action_idx);
    if (slot == NULL) {
        return;
    }

    if (slot == &ctx->alarm) {
        ctx->code_format[0] = '\0';
        ctx->code_arm_required = true;
        ctx->supported_features = 0;
    }
    cJSON *attrs = (state->attributes_json[0] != '\0') ? cJSON_Parse(state->attributes_json) : NULL;
    if (attrs != NULL) {
        w_alarm_set_slot_name_from_attrs(slot, attrs);
        if (slot == &ctx->alarm) {
            cJSON *code_format = cJSON_GetObjectItemCaseSensitive(attrs, "code_format");
            snprintf(ctx->code_format, sizeof(ctx->code_format), "%s",
                cJSON_IsString(code_format) && code_format->valuestring != NULL ? code_format->valuestring : "");
            cJSON *arm_required = cJSON_GetObjectItemCaseSensitive(attrs, "code_arm_required");
            ctx->code_arm_required = cJSON_IsBool(arm_required) ? cJSON_IsTrue(arm_required) : true;
            cJSON *features = cJSON_GetObjectItemCaseSensitive(attrs, "supported_features");
            if (cJSON_IsNumber(features)) {
                ctx->supported_features = (uint32_t)features->valueint;
            }
        }
        cJSON_Delete(attrs);
    }

    bool changed = slot->known && strcmp(slot->state, state->state) != 0;
    snprintf(slot->state, sizeof(slot->state), "%s", state->state);
    slot->known = true;
    if (changed && ctx->log_loaded) {
        w_alarm_log_push_front(ctx, slot, state->state);
        w_alarm_render_log(ctx);
    }

    /* The runtime routes every configured zone and control independently. */
    if (slot == &ctx->alarm) {
        snprintf(ctx->attributes, sizeof(ctx->attributes), "%s", state->attributes_json);
        if (ha_alarm_state_parse(state->state) == HA_ALARM_UNKNOWN) { w_alarm_clear_code(ctx); ctx->held_action = -1; }
    }
    w_alarm_render_page_state(ctx);
}

void w_alarm_panel_mark_unavailable(ui_widget_instance_t *instance)
{
    if (instance == NULL || instance->ctx == NULL) {
        return;
    }
    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)instance->ctx;
    w_alarm_clear_code(ctx);
    ctx->held_action = -1;
    snprintf(ctx->alarm.state, sizeof(ctx->alarm.state), "unavailable");
    ctx->alarm.known = true;
    w_alarm_render_page_state(ctx);
}

bool w_alarm_panel_tracks_entity(const ui_widget_instance_t *instance, const char *entity_id)
{
    if (instance == NULL || instance->ctx == NULL) {
        return false;
    }
    w_alarm_ctx_t *ctx = (w_alarm_ctx_t *)instance->ctx;
    return w_alarm_find_slot(ctx, entity_id, NULL, NULL) != NULL;
}


void w_alarm_panel_set_visible(ui_widget_instance_t *instance, bool visible)
{
    w_alarm_ctx_t *ctx = instance ? instance->ctx : NULL;
    if (!ctx) return;
    ctx->visible = visible;
    if (!visible) {
        w_alarm_clear_code(ctx);
        ctx->held_action = -1;
        ha_alarm_panel_cancel(ctx->client);
        ha_alarm_panel_cancel(ctx->history_client);
        ctx->fetching = false;
        ctx->next_fetch_ms = 0;
        for (size_t i = 0; i < ctx->action_count; ++i)
            if (ctx->page.action_bars[i]) lv_bar_set_value(ctx->page.action_bars[i], 0, LV_ANIM_OFF);
    }
    w_alarm_render_page_state(ctx);
}

size_t w_alarm_panel_entity_ids(const ui_widget_instance_t *instance, const char **out, size_t max)
{
    w_alarm_ctx_t *ctx = instance ? instance->ctx : NULL;
    if (!ctx) return 0;
    size_t n = 0;
    if (n < max && ctx->alarm.entity_id[0]) out[n++] = ctx->alarm.entity_id;
    if (n < max && ctx->keypad_entity_id[0]) out[n++] = ctx->keypad_entity_id;
    for (size_t i = 0; i < ctx->zone_count && n < max; ++i) out[n++] = ctx->zones[i].entity_id;
    for (size_t i = 0; i < ctx->action_count && n < max; ++i) out[n++] = ctx->actions[i].entity_id;
    return n;
}
