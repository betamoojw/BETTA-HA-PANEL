// Adapted from betamoojw/xiaozhi-esp32 at 61fc9d0b. See README.md and LICENSE.xiaozhi.
#include "knx_object.h"
#include "cJSON.h"
#include <type_traits>
void AddValue(cJSON* json, const KnxValue& value) {
    std::visit(
        [json](const auto& item) {
            using Type = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Type, std::monostate>) {
                cJSON_AddNullToObject(json, "value");
            } else if constexpr (std::is_same_v<Type, bool>) {
                cJSON_AddBoolToObject(json, "value", item);
            } else if constexpr (std::is_same_v<Type, int64_t>) {
                cJSON_AddStringToObject(json, "value", std::to_string(item).c_str());
            } else if constexpr (std::is_arithmetic_v<Type>) {
                cJSON_AddNumberToObject(json, "value", item);
            } else if constexpr (std::is_same_v<Type, std::string>) {
                cJSON_AddStringToObject(json, "value", item.c_str());
            } else {
                cJSON* object = cJSON_AddObjectToObject(json, "value");
                if constexpr (std::is_same_v<Type, knx_dpt2_control_t>) {
                    cJSON_AddBoolToObject(object, "control", item.control);
                    cJSON_AddBoolToObject(object, "value", item.value);
                } else if constexpr (std::is_same_v<Type, knx_dpt3_control_t>) {
                    cJSON_AddBoolToObject(object, "control", item.control);
                    cJSON_AddNumberToObject(object, "step_code", item.step_code);
                } else if constexpr (std::is_same_v<Type, knx_dpt10_time_t>) {
                    cJSON_AddNumberToObject(object, "weekday", item.weekday);
                    cJSON_AddNumberToObject(object, "hour", item.hour);
                    cJSON_AddNumberToObject(object, "minute", item.minute);
                    cJSON_AddNumberToObject(object, "second", item.second);
                } else if constexpr (std::is_same_v<Type, knx_dpt11_date_t>) {
                    cJSON_AddNumberToObject(object, "day", item.day);
                    cJSON_AddNumberToObject(object, "month", item.month);
                    cJSON_AddNumberToObject(object, "year", item.year);
                } else if constexpr (std::is_same_v<Type, knx_dpt18_scene_control_t>) {
                    cJSON_AddBoolToObject(object, "learn", item.learn);
                    cJSON_AddNumberToObject(object, "scene_number", item.scene_number);
                } else if constexpr (std::is_same_v<Type, KnxDateTime>) {
                    cJSON_AddNumberToObject(object, "year", item.year);
                    cJSON_AddNumberToObject(object, "month", item.month);
                    cJSON_AddNumberToObject(object, "day", item.day);
                    cJSON_AddNumberToObject(object, "weekday", item.weekday);
                    cJSON_AddNumberToObject(object, "hour", item.hour);
                    cJSON_AddNumberToObject(object, "minute", item.minute);
                    cJSON_AddNumberToObject(object, "second", item.second);
                    cJSON_AddBoolToObject(object, "fault", item.fault);
                    cJSON_AddBoolToObject(object, "working_day", item.working_day);
                    cJSON_AddBoolToObject(object, "working_day_valid", item.working_day_valid);
                    cJSON_AddBoolToObject(object, "date_valid", item.date_valid);
                    cJSON_AddBoolToObject(object, "weekday_valid", item.weekday_valid);
                    cJSON_AddBoolToObject(object, "time_valid", item.time_valid);
                    cJSON_AddBoolToObject(object, "daylight_saving_time",
                                          item.daylight_saving_time);
                    cJSON_AddBoolToObject(object, "clock_quality", item.clock_quality);
                    cJSON_AddBoolToObject(object, "year_valid", item.year_valid);
                } else if constexpr (std::is_same_v<Type, knx_dpt26_scene_info_t>) {
                    cJSON_AddBoolToObject(object, "active", item.active);
                    cJSON_AddNumberToObject(object, "scene_number", item.scene_number);
                } else if constexpr (std::is_same_v<Type, knx_dpt27_combined_status_t>) {
                    cJSON_AddNumberToObject(object, "value", item.value);
                    cJSON_AddNumberToObject(object, "mask", item.mask);
                } else if constexpr (std::is_same_v<Type, knx_dpt232_color_t>) {
                    cJSON_AddNumberToObject(object, "red", item.red);
                    cJSON_AddNumberToObject(object, "green", item.green);
                    cJSON_AddNumberToObject(object, "blue", item.blue);
                } else if constexpr (std::is_same_v<Type, knx_dpt251_color_t>) {
                    cJSON_AddNumberToObject(object, "red", item.red);
                    cJSON_AddNumberToObject(object, "green", item.green);
                    cJSON_AddNumberToObject(object, "blue", item.blue);
                    cJSON_AddNumberToObject(object, "white", item.white);
                    cJSON_AddNumberToObject(object, "valid_channels", item.valid_channels);
                }
            }
        },
        value);
}

cJSON* KnxObjectJson(const KnxCommunicationObject& object) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", object.id.c_str());
    cJSON_AddStringToObject(json, "name", object.name.c_str());
    if (!object.description.empty()) {
        cJSON_AddStringToObject(json, "description", object.description.c_str());
    }
    cJSON_AddStringToObject(json, "group_address", object.group_address.c_str());
    cJSON_AddStringToObject(json, "datapoint_type", KnxDptName(object.datapoint_type).c_str());
    cJSON_AddStringToObject(json, "unit", object.unit.c_str());
    const char* format = "decimal integer";
    switch (object.datapoint_type.main) {
        case 1: format = "on/off or true/false"; break;
        case 2: format = "control,value (booleans)"; break;
        case 3: format = "control,step_code (boolean,0..7)"; break;
        case 4: format = object.datapoint_type.subtype == 1 ? "one ASCII character" : "decimal integer"; break;
        case 5: format = object.datapoint_type.subtype == 1 ? "percent 0..100" :
                         object.datapoint_type.subtype == 3 ? "angle 0..360 degrees" : "decimal integer"; break;
        case 9: case 14: format = "finite decimal number in the configured unit"; break;
        case 10: format = "weekday,hour,minute,second (0..7,0..23,0..59,0..59)"; break;
        case 11: format = "day,month,year (two-digit year)"; break;
        case 16: format = "text, at most 14 encoded bytes"; break;
        case 18: format = "learn,scene_number (boolean,0..63)"; break;
        case 19: format = "year,month,day,weekday,hour,minute,second,fault,working_day,working_day_valid,date_valid,weekday_valid,time_valid,daylight_saving_time,clock_quality (year since 1900; flags boolean)"; break;
        case 24: case 28: format = "text"; break;
        case 26: format = "active,scene_number (boolean,0..63)"; break;
        case 27: format = "value,mask (integers)"; break;
        case 29: format = "signed 64-bit decimal string"; break;
        case 232: format = "red,green,blue (0..255)"; break;
        case 234: format = "two-letter language/region code, per subtype"; break;
        case 251: format = "red,green,blue,white,valid_channels (channels 0..255, validity mask 0..15)"; break;
    }
    cJSON_AddStringToObject(json, "value_format", format);
    cJSON_AddBoolToObject(json, "readable", object.readable);
    cJSON_AddBoolToObject(json, "writable", object.writable);
    cJSON_AddBoolToObject(json, "valid", object.valid);
    if (object.valid) {
        AddValue(json, object.current_value);
        cJSON_AddNumberToObject(json, "last_update_ms", static_cast<double>(object.last_update_ms));
    }
    return json;
}
