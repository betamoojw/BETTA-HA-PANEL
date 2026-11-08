/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "layout/alarm_panel_config.h"
#include "app_config.h"
#include "util/entity_id.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool domain(const char *id, const char *name)
{
    return entity_id_in_domain(id, name);
}
static bool slots(const cJSON *array, bool zones)
{
    if (!array) return true;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) > (zones ? 16 : 8)) return false;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "entity_id"));
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        if (!cJSON_IsObject(item) || (name && (!cJSON_IsString(name) || strlen(name->valuestring) >= 48))) return false;
        if (zones ? !domain(id,"binary_sensor") :
            !(domain(id,"switch") || domain(id,"input_boolean") || domain(id,"button") || domain(id,"input_button"))) return false;
        for (const char *p = id; *p; ++p)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.')) return false;
        for (const cJSON *earlier = array->child; earlier != item; earlier = earlier->next) {
            const char *other = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(earlier,"entity_id"));
            if (other && !strcmp(id,other)) return false;
        }
    }
    return true;
}
bool alarm_panel_validate(const cJSON *widget, char *error, size_t size)
{
    const cJSON *hours = cJSON_GetObjectItemCaseSensitive(widget,"alarm_log_hours");
    const cJSON *secondary = cJSON_GetObjectItemCaseSensitive(widget,"secondary_entity_id");
    const char *reason = NULL;
    if (!slots(cJSON_GetObjectItemCaseSensitive(widget,"alarm_zones"),true)) reason = "alarm_zones requires up to 16 unique binary_sensor entries";
    else if (!slots(cJSON_GetObjectItemCaseSensitive(widget,"alarm_actions"),false)) reason = "alarm_actions requires up to 8 unique switch or button entries";
    else if (hours && (!cJSON_IsNumber(hours) || !isfinite(hours->valuedouble) || hours->valuedouble < 1 || hours->valuedouble > 168 || hours->valuedouble != hours->valueint)) reason = "alarm_log_hours must be an integer from 1 to 168";
    else if (secondary && (!cJSON_IsString(secondary) || (secondary->valuestring[0] && !domain(secondary->valuestring,"sensor")))) reason = "alarm keypad text must be a sensor";
    else if (cJSON_GetObjectItemCaseSensitive(widget,"code") || cJSON_GetObjectItemCaseSensitive(widget,"alarm_code") || cJSON_GetObjectItemCaseSensitive(widget,"pin")) reason = "alarm codes must not be stored in a layout";
    if (reason && error && size) snprintf(error,size,"%s",reason);
    return !reason;
}
