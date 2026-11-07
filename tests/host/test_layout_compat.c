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
    normalization(); validation(); allocation_failures();
    puts("PASS: legacy weather reads, idempotence, retained fields, validation and allocation cleanup");
    return 0;
}
