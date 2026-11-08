/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include "cJSON.h"
#include "layout/alarm_panel_config.h"
static void check(const char *json, bool valid)
{
    cJSON *value = cJSON_Parse(json);
    char error[160] = {0};
    assert(value);
    assert(alarm_panel_validate(value, error, sizeof(error)) == valid);
    if (!valid) assert(error[0]);
    cJSON_Delete(value);
}
int main(void)
{
    check("{}", true);
    check("{\"alarm_zones\":[{\"entity_id\":\"binary_sensor.door\",\"name\":\"Door\"}],\"alarm_actions\":[{\"entity_id\":\"input_button.panic\"}],\"alarm_log_hours\":168}",true);
    check("{\"alarm_zones\":[{\"entity_id\":\"binary_sensor.a.b\"}]}",false);
    check("{\"secondary_entity_id\":\"sensor.a.b\"}",false);
    check("{\"alarm_zones\":[{\"entity_id\":\"switch.door\"}]}",false);
    check("{\"alarm_zones\":[{\"entity_id\":\"binary_sensor.door\"},{\"entity_id\":\"binary_sensor.door\"}]}",false);
    check("{\"alarm_actions\":[{\"entity_id\":\"script.panic\"}]}",false);
    check("{\"alarm_actions\":[\"arm_home\"]}",false);
    check("{\"alarm_log_hours\":1.5}",false);
    check("{\"alarm_log_hours\":0}",false);
    check("{\"alarm_log_hours\":169}",false);
    check("{\"code\":\"1234\"}",false);
    check("{\"alarm_code\":null}",false);
    check("{\"pin\":1234}",false);
    puts("alarm panel configuration: 14 checks passed");
}
