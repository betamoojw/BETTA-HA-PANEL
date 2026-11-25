/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "layout/layout_validate.h"
#include "layout/widget_types.h"

static const char *legacy = "{\"version\":1,\"pages\":[{\"id\":\"weather\",\"widgets\":[{"
    "\"id\":\"legacy_forecast\",\"type\":\"weather_3day\",\"entity_id\":\"weather.demo\","
    "\"forecast_today_entity_id\":\"sensor.report_today\",\"forecast_days\":3,"
    "\"rect\":{\"x\":12,\"y\":12,\"w\":320,\"h\":300}}]}]}";

static cJSON *first_widget(cJSON *root)
{
    return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root, "pages"), 0), "widgets"), 0);
}

static void normalization(void)
{
    cJSON *root = cJSON_Parse(legacy), *expected = cJSON_Parse(legacy);
    assert(root && expected);
    cJSON *widget = first_widget(expected);
    assert(cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(widget, "type"), "weather_tile"));
    assert(cJSON_AddStringToObject(widget, "weather_view", "days"));
    assert(widget_types_normalize_layout(root));
    assert(cJSON_Compare(root, expected, true)); /* IDs, bindings, sizes and options are retained. */
    assert(widget_types_normalize_layout(root));
    assert(cJSON_Compare(root, expected, true)); /* Repeated reads do not add duplicate fields. */
    assert(cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(first_widget(root), "type"), "weather_3day"));
    assert(cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(first_widget(root), "weather_view"), "today"));
    assert(widget_types_normalize_layout(root));
    assert(cJSON_Compare(root, expected, true)); /* The retired type always means forecast days. */
    cJSON_Delete(root); cJSON_Delete(expected);

    const char *current = "{\"pages\":[{\"widgets\":[{\"type\":\"weather_tile\",\"weather_view\":\"today\"},"
        "{\"type\":\"sensor\",\"entity_id\":\"sensor.weather_3day\"},{\"type\":\"roborock_tile\"}]}]}";
    root = cJSON_Parse(current); expected = cJSON_Parse(current);
    assert(root && expected && widget_types_normalize_layout(root));
    assert(cJSON_Compare(root, expected, true));
    cJSON_Delete(root); cJSON_Delete(expected);
    assert(widget_types_normalize_layout(NULL));
}

static void validation(void)
{
    layout_validation_result_t *result = calloc(1, sizeof(*result));
    assert(result && layout_validate_json(legacy, result));
    cJSON *root = cJSON_Parse(legacy);
    assert(root);
    cJSON *entity = cJSON_GetObjectItemCaseSensitive(first_widget(root), "entity_id");
    assert(cJSON_SetValuestring(entity, "light.invalid"));
    char *text = cJSON_PrintUnformatted(root);
    assert(text && !layout_validate_json(text, result));
    cJSON_free(text);
    assert(cJSON_SetValuestring(entity, "weather.demo"));
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(first_widget(root), "rect"), "w"), 100);
    text = cJSON_PrintUnformatted(root);
    assert(text && !layout_validate_json(text, result));
    cJSON_free(text); cJSON_Delete(root); free(result);
}

static void knx_bindings(void)
{
    static const struct { const char *type, *fields; bool valid; } cases[] = {
        {"sensor", "\"entity_id\":\"sensor.room\"", true},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"object_id\":\"sensor.room\"}", true},
        {"scene_tile", "\"binding\":{\"source\":\"knx\",\"object_id\":\"scene\",\"value\":\"false,3\"}", true},
        {"scene_tile", "\"binding\":{\"source\":\"knx\",\"object_id\":\"missing-preset\"}", false},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"object_id\":\"room\",\"value\":\"1\"}", false},
        {"sensor", "\"binding\":{\"source\":\"other\",\"object_id\":\"room\"}", false},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"object_id\":\"\"}", false},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"object_id\":\"room\"},\"entity_id\":\"sensor.other\"", false},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"object_id\":\"room\"},\"sensor_entities\":[\"sensor.other\"]", false},
        {"graph", "\"binding\":{\"source\":\"knx\",\"object_id\":\"room\"},\"graph_ha_history\":true", false},
        {"slider", "\"binding\":{\"source\":\"knx\",\"roles\":{\"brightness_command\":\"A\",\"brightness_feedback\":\"B\"}}", true},
        {"slider", "\"binding\":{\"source\":\"knx\",\"roles\":{\"brightness_command\":\"A\",\"brightness_feedback\":\"A\"}}", true},
        {"light_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"switch_command\":\"P\",\"switch_feedback\":\"S\",\"brightness_command\":\"A\",\"brightness_feedback\":\"B\"}}", true},
        {"light_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"switch_command\":\"P\"}}", true},
        {"button", "\"binding\":{\"source\":\"knx\",\"roles\":{\"switch_feedback\":\"removed\"}}", true},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"roles\":{\"value_feedback\":\"AB\"}}", true},
        {"scene_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"action\":\"pulse\"},\"value\":\"off\"}", true},
        {"sensor", "\"binding\":{\"source\":\"knx\",\"roles\":{\"value_feedback\":\"AB\"},\"object_id\":\"AB\"}", false},
        {"slider", "\"binding\":{\"source\":\"knx\",\"roles\":{}}", false},
        {"slider", "\"binding\":{\"source\":\"knx\",\"roles\":{\"switch_command\":\"P\"}}", false},
        {"light_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"brightness_feedback\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}}", false},
        {"scene_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"action\":\"A\"}}", false},
        {"camera_tile", "\"binding\":{\"source\":\"knx\",\"object_id\":\"room\"}", false},
        {"cover_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"move_command\":\"up_down\",\"stop_command\":\"stop\",\"position_command\":\"target\",\"position_feedback\":\"actual\"}}", true},
        {"cover_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"position_feedback\":\"missing\"},\"position_open_at_100\":true}", true},
        {"cover_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"move_command\":\"write_only\"},\"position_open_at_100\":false}", true},
        {"cover_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"move_command\":\"A\"},\"position_open_at_100\":1}", false},
        {"cover_tile", "\"binding\":{\"source\":\"knx\",\"object_id\":\"ambiguous\"}", false},
        {"cover_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"brightness_command\":\"A\"}}", false},
        {"climate_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"target_temperature_command\":\"set\",\"target_temperature_feedback\":\"target\",\"current_temperature_feedback\":\"room\",\"hvac_mode_command\":\"mode_set\",\"hvac_mode_feedback\":\"mode\"}}", true},
        {"climate_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"current_temperature_feedback\":\"room\"}}", true},
        {"climate_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"hvac_mode_command\":\"mode\"},\"position_open_at_100\":false}", false},
        {"climate_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"target_temperature_command\":\"A\",\"target_temperature_command\":\"B\"}}", false},
        {"fan_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"switch_command\":\"power\",\"switch_feedback\":\"status\",\"fan_speed_command\":\"speed\",\"fan_speed_feedback\":\"rpm_percent\"}}", true},
        {"fan_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"fan_speed_feedback\":\"readonly\"}}", true},
        {"fan_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"fan_speed_command\":\"A\"}},\"entity_id\":\"fan.mixed\"", false},
        {"fan_tile", "\"binding\":{\"source\":\"knx\",\"roles\":{\"fan_speed_command\":\"A\"}},\"secondary_entity_id\":\"sensor.mixed\"", false},
    };
    layout_validation_result_t *result = calloc(1, sizeof(*result));
    assert(result);
    for (size_t i = 0; i < sizeof(cases)/sizeof(*cases); ++i) {
        char json[640];
        snprintf(json, sizeof(json), "{\"version\":1,\"pages\":[{\"id\":\"p\",\"widgets\":[{"
            "\"id\":\"w\",\"type\":\"%s\",%s,\"rect\":{\"x\":0,\"y\":0,\"w\":260,\"h\":%d}}]}]}",
            cases[i].type, cases[i].fields, !strcmp(cases[i].type, "scene_tile") ? 100 : 200);
        assert(layout_validate_json(json, result) == cases[i].valid);
        if (cases[i].valid) {
            cJSON *saved = cJSON_Parse(json);
            assert(saved && widget_types_normalize_layout(saved));
            char *serialized = cJSON_PrintUnformatted(saved);
            assert(serialized && layout_validate_json(serialized, result));
            cJSON *restored = cJSON_Parse(serialized);
            assert(restored && cJSON_Compare(saved, restored, true));
            cJSON_Delete(saved); cJSON_Delete(restored); cJSON_free(serialized);
        }
    }
    free(result);
}

static int budget = -1, outstanding;
static void *tracked_malloc(size_t size)
{
    if (budget == 0) return NULL;
    if (budget > 0) --budget;
    void *ptr = malloc(size);
    if (ptr) ++outstanding;
    return ptr;
}
static void tracked_free(void *ptr)
{
    if (ptr) --outstanding;
    free(ptr);
}

static void allocation_failures(void)
{
    cJSON_Hooks hooks = {.malloc_fn = tracked_malloc, .free_fn = tracked_free};
    cJSON_InitHooks(&hooks);
    unsigned failures = 0, successes = 0;
    for (int limit = 0; limit < 6; ++limit) {
        budget = -1;
        cJSON *root = cJSON_Parse(legacy);
        assert(root);
        budget = limit;
        if (widget_types_normalize_layout(root)) {
            ++successes;
            assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(first_widget(root), "type")), "weather_tile"));
        } else {
            ++failures;
        }
        budget = -1;
        cJSON_Delete(root); /* Callers discard an incomplete tree; storage was never touched. */
        assert(outstanding == 0);
    }
    assert(failures > 0 && successes > 0);
    cJSON_InitHooks(NULL);
}

int main(void)
{
    normalization(); validation(); knx_bindings(); allocation_failures();
    puts("PASS: legacy weather and KNX layout validation, save/reload, retained roles and allocation cleanup");
    return 0;
}
