// Adapted from betamoojw/xiaozhi-esp32, feat/knx_ip at 61fc9d0b619f39ed20356b8de2f9c0edd954472d.
// See README.md for provenance and third-party terms.
#include "knx_config.h"
#include <cJSON.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <set>
#include <utility>

namespace {

bool Fields(const cJSON* object, const std::set<std::string>& allowed) {
    std::set<std::string> seen;
    for (auto* p = object->child; p; p = p->next)
        if (!p->string || !allowed.count(p->string) || !seen.insert(p->string).second) return false;
    return true;
}

bool Multicast(const std::string& text) {
    unsigned a, b, c, d; int used = 0;
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u%n", &a, &b, &c, &d, &used) != 4 ||
        static_cast<size_t>(used) != text.size()) return false;
    return a >= 224 && a <= 239 && b <= 255 && c <= 255 && d <= 255 &&
        text == std::to_string(a)+"."+std::to_string(b)+"."+std::to_string(c)+"."+std::to_string(d);
}

bool ShallowJson(const std::string& text) {
    // Bound cJSON's recursive parser on the HTTP task's finite stack.
    unsigned depth=0; bool quoted=false, escaped=false;
    for (char c : text) {
        if (quoted) {
            if (escaped) escaped=false;
            else if (c=='\\') escaped=true;
            else if (c=='"') quoted=false;
        } else if (c=='"') quoted=true;
        else if (c=='{' || c=='[') { if (++depth>12) return false; }
        else if (c=='}' || c==']') { if (!depth) return false; --depth; }
    }
    return !quoted && depth==0;
}

constexpr size_t kMaximumIdLength = 48;
constexpr size_t kMaximumNameLength = 80;
constexpr size_t kMaximumDescriptionLength = 192;


bool IsKnownField(const char* name) {
    constexpr const char* kFields[] = {
        "id", "name", "description", "group_address", "datapoint_type", "readable", "writable",
        "unit",
    };
    return name != nullptr &&
           std::any_of(std::begin(kFields), std::end(kFields),
                       [name](const char* field) { return std::strcmp(name, field) == 0; });
}

std::string ObjectFieldError(size_t index, const char* field) {
    return "Invalid KNX communication object " + std::to_string(index) + " field '" + field +
           "'";
}

bool JsonString(const cJSON* object, const char* name, std::string& value, bool required,
                size_t maximum_length) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (item == nullptr && !required) {
        value.clear();
        return true;
    }
    if (!required && cJSON_IsNull(item)) {
        value.clear();
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == nullptr) {
        return false;
    }
    value = item->valuestring;
    return (!required || !value.empty()) && value.size() <= maximum_length;
}

bool JsonBoolean(const cJSON* object, const char* name, bool& value) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsBool(item)) {
        return false;
    }
    value = cJSON_IsTrue(item);
    return true;
}

bool ValidateObject(const KnxCommunicationObject& object,
                    const std::vector<KnxCommunicationObject>& objects, size_t index,
                    std::string& error) {
    if (!object.readable && !object.writable) {
        error = ObjectFieldError(index, "readable/writable") + ": at least one must be true";
        return false;
    }
    if (std::any_of(objects.begin(), objects.end(),
                    [&object](const auto& existing) { return existing.id == object.id; })) {
        error = ObjectFieldError(index, "id") + ": duplicate ID '" + object.id + "'";
        return false;
    }
    if (std::any_of(objects.begin(), objects.end(), [&object](const auto& existing) {
            return existing.parsed_group_address == object.parsed_group_address;
        })) {
        error = ObjectFieldError(index, "group_address") + ": duplicate address '" +
            object.group_address + "'";
        return false;
    }
    return true;
}

}  // namespace



bool KnxParseConfiguration(const std::string& json_text, size_t maximum_objects,
                           size_t maximum_group_addresses,
                           std::vector<KnxCommunicationObject>& objects,
                           std::string& canonical_json, std::string& error,
                           std::string* physical_address) {
    objects.clear();
    canonical_json.clear();
    error.clear();
    if (physical_address != nullptr) {
        physical_address->clear();
    }
    if (json_text.empty() || json_text.size() > kKnxMaximumConfigurationLength) {
        error = "KNX configuration is empty or exceeds the size limit";
        return false;
    }
    if (!ShallowJson(json_text)) { error = "Invalid or excessively nested JSON"; return false; }
    // cJSON strings use NUL termination: reject embedded or escaped NULs rather
    // than silently authorizing an ID truncated at that byte.
    if (json_text.find('\0') != std::string::npos ||
        json_text.find("\\u0000") != std::string::npos) {
        error = "NUL characters are not allowed"; return false;
    }

    cJSON* root = cJSON_ParseWithLengthOpts(json_text.c_str(), json_text.size() + 1, nullptr, true);
    if (root == nullptr) {
        error = "Invalid JSON in KNX configuration";
        return false;
    }

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        error = "KNX configuration root must be a JSON object";
        return false;
    }
    if (!Fields(root, {"media_type", "media_parameters", "physical_address", "comObjects"})) {
        cJSON_Delete(root); error = "Unknown or duplicate KNX root field"; return false;
    }
    std::string media_type;
    if (!JsonString(root, "media_type", media_type, true, 16) || media_type != "knx_ip") {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'media_type': expected 'knx_ip'";
        return false;
    }
    const cJSON* media_parameters = cJSON_GetObjectItemCaseSensitive(root, "media_parameters");
    if (!cJSON_IsObject(media_parameters)) {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'media_parameters': expected an object";
        return false;
    }
    if (!Fields(media_parameters, {"multicast_address", "udp_port", "transport_mode", "interface_identifier", "nat"})) {
        cJSON_Delete(root); error = "Unknown or duplicate KNX media field"; return false;
    }
    std::string multicast_address;
    std::string transport_mode;
    std::string interface_identifier;
    const cJSON* udp_port = cJSON_GetObjectItemCaseSensitive(media_parameters, "udp_port");
    if (!JsonString(media_parameters, "multicast_address", multicast_address, true, 15) || !Multicast(multicast_address)) {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'media_parameters.multicast_address'";
        return false;
    }
    if (!cJSON_IsNumber(udp_port) || udp_port->valuedouble < 1 || udp_port->valuedouble > 65535 ||
        udp_port->valuedouble != udp_port->valueint) {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'media_parameters.udp_port'";
        return false;
    }
    if (!JsonString(media_parameters, "transport_mode", transport_mode, true, 16) ||
        transport_mode != "routing") {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'media_parameters.transport_mode': expected 'routing'";
        return false;
    }
    if (!JsonString(media_parameters, "interface_identifier", interface_identifier, true, 80)) {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'media_parameters.interface_identifier'";
        return false;
    }
    bool nat = false;
    if (!JsonBoolean(media_parameters, "nat", nat) || nat) {
        cJSON_Delete(root);
        error = "Routing requires media_parameters.nat=false";
        return false;
    }
    std::string parsed_physical_address;
    knx_address_t parsed_physical_address_value = 0;
    if (!JsonString(root, "physical_address", parsed_physical_address, true, 10) ||
        !KnxParsePhysicalAddress(parsed_physical_address, parsed_physical_address_value)) {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'physical_address'";
        return false;
    }
    cJSON* objects_array = cJSON_GetObjectItemCaseSensitive(root, "comObjects");
    if (!cJSON_IsArray(objects_array)) {
        cJSON_Delete(root);
        error = "Invalid KNX root field 'comObjects': expected an array";
        return false;
    }

    bool valid = true;
    const cJSON* item = nullptr;
    size_t index = 0;
    cJSON_ArrayForEach (item, objects_array) {
        if (objects.size() >= maximum_objects || !cJSON_IsObject(item)) {
            error = ObjectFieldError(index, "object") +
                    ": exceeds the configured limit or is not an object";
            valid = false;
            break;
        }
        std::set<std::string> fields;
        for (const cJSON* field = item->child; field != nullptr; field = field->next) {
            if (!IsKnownField(field->string)) {
                error = ObjectFieldError(index,
                                         field->string == nullptr ? "<unnamed>" : field->string) +
                        ": unknown field";
                valid = false;
                break;
            }
            if (!fields.insert(field->string).second) {
                error = ObjectFieldError(index, field->string) + ": duplicate field";
                valid = false;
                break;
            }
        }
        if (!valid) {
            break;
        }

        KnxCommunicationObject object;
        std::string datapoint_type;
        if (!JsonString(item, "id", object.id, true, kMaximumIdLength)) {
            error = ObjectFieldError(index, "id");
        } else if (!JsonString(item, "name", object.name, true, kMaximumNameLength)) {
            error = ObjectFieldError(index, "name");
        } else if (!JsonString(item, "description", object.description, false,
                               kMaximumDescriptionLength)) {
            error = ObjectFieldError(index, "description");
        } else if (!JsonString(item, "group_address", object.group_address, true, 10) ||
                   !KnxParseGroupAddress(object.group_address, object.parsed_group_address)) {
            error = ObjectFieldError(index, "group_address");
        } else if (!JsonString(item, "datapoint_type", datapoint_type, true, 16) ||
                   !KnxParseDpt(datapoint_type, object.datapoint_type) ||
                   KnxDptName(object.datapoint_type) != datapoint_type) {
            error = ObjectFieldError(index, "datapoint_type") + ": object '" + object.id +
                    "', '" + datapoint_type + "': expected a canonical supported DPT";
        } else if (!JsonBoolean(item, "readable", object.readable)) {
            error = ObjectFieldError(index, "readable");
        } else if (!JsonBoolean(item, "writable", object.writable)) {
            error = ObjectFieldError(index, "writable");
        } else if (!JsonString(item, "unit", object.unit, false, 32)) {
            error = ObjectFieldError(index, "unit");
        } else {
            object.group_address = KnxFormatGroupAddress(object.parsed_group_address);
            valid = ValidateObject(object, objects, index, error);
        }
        if (!error.empty()) {
            valid = false;
            break;
        }
        objects.push_back(std::move(object));
        ++index;
    }

    if (valid && objects.size() > maximum_group_addresses) {
        error = "KNX configuration exceeds component callback capacity";
        valid = false;
    }
    if (valid) {
        char* printed = cJSON_PrintUnformatted(root);
        if (printed == nullptr) {
            error = "Could not serialize KNX configuration";
            valid = false;
        } else {
            canonical_json = printed;
            cJSON_free(printed);
            if (canonical_json.size() > kKnxMaximumConfigurationLength) {
                error = "Canonical KNX configuration exceeds the size limit";
                valid = false;
            }
        }
    }
    cJSON_Delete(root);
    if (!valid) {
        objects.clear();
        canonical_json.clear();
    } else if (physical_address != nullptr) {
        *physical_address = KnxFormatPhysicalAddress(parsed_physical_address_value);
    }
    return valid;
}
