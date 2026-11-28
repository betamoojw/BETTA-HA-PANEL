// Adapted from betamoojw/xiaozhi-esp32, feat/knx_ip at 61fc9d0b619f39ed20356b8de2f9c0edd954472d.
// See README.md for provenance and third-party terms.

/** @file
 * @brief KNX address, datapoint type (DPT) and communication object codecs.
 */
#pragma once

#include <esp_knx_ip/knx_dpt.h>
#include <esp_knx_ip/knx_types.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

/** @brief Datapoint identifier that distinguishes a missing subtype from subtype zero. */
struct KnxDpt
{
    uint16_t main = 1;
    uint16_t subtype = 0;
    bool has_subtype = false;

    bool operator==(const KnxDpt &other) const
    {
        return main == other.main && subtype == other.subtype && has_subtype == other.has_subtype;
    }
};

/**
 * @brief DPT 19 date and time with independent year and date validity.
 * The transport component combines those flags.
 * @note Keep the original 15 fields in order for source compatibility.
 */
struct KnxDateTime
{
    uint8_t year, month, day, weekday, hour, minute, second;
    bool fault, working_day, working_day_valid, date_valid, weekday_valid, time_valid;
    bool daylight_saving_time, clock_quality;
    bool year_valid = true;
};

/** @brief Typed datapoint value; std::monostate represents an invalid reading where the DPT permits it. */
using KnxValue = std::variant<bool, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, int64_t, float, std::string, knx_dpt2_control_t,
                              knx_dpt3_control_t, knx_dpt10_time_t, knx_dpt11_date_t, knx_dpt18_scene_control_t, KnxDateTime,
                              knx_dpt26_scene_info_t, knx_dpt27_combined_status_t, knx_dpt232_color_t, knx_dpt251_color_t, std::monostate>;

/** @brief Configured group alias with separate bus feedback and last-command caches. */
struct KnxCommunicationObject
{
    std::string id;
    std::string name;
    std::string description;
    std::string group_address;
    knx_address_t parsed_group_address = 0;
    KnxDpt datapoint_type;
    bool readable = false;
    bool writable = false;
    int display_precision = -1;     /**< Optional scaling Sensor display override. */
    std::string unit;               /**< Optional unit of measurement (e.g., "%", "°C"). */
    KnxValue current_value = false; /**< Cached bus value; inspect valid or live_feedback before use. */
    bool valid = false;             /**< Cached value is valid, even if it is from an earlier connection. */
    uint64_t last_update_ms = 0;    /**< Monotonic time of the last received sample in milliseconds. */
    uint32_t widget_revision = 0;   /**< Revision of the object's view, including invalidation. */
    bool live_feedback = false;     /**< Whether feedback is valid for the current routing connection. */
    float last_command = 0;         /**< Scalar command for paired controls; zero is a valid command. */
    uint32_t command_revision = 0;  /**< Zero when no command is cached. */
};

/** @brief Parse a three-level group address in main/middle/sub form. */
bool KnxParseGroupAddress(const std::string &text, knx_address_t &address);
/** @brief Render a group address in canonical main/middle/sub form. */
std::string KnxFormatGroupAddress(knx_address_t address);
/** @brief Parse an individual address in area.line.device form. */
bool KnxParsePhysicalAddress(const std::string &text, knx_address_t &address);
/** @brief Render an individual address in canonical area.line.device form. */
std::string KnxFormatPhysicalAddress(knx_address_t address);
/** @brief Parse and validate a main type and optional subtype, with or without the DPT prefix. */
bool KnxParseDpt(const std::string &text, KnxDpt &datapoint_type);
/** @brief Check whether the DPT is supported by this firmware. */
bool KnxValidateDpt(const KnxDpt &datapoint_type);
/** @brief Check the representation and subtype constraints; optionally return a diagnostic. */
bool KnxValidateValue(const KnxDpt &datapoint_type, const KnxValue &value, std::string *error = nullptr);
/** @brief Return the canonical DPT name used in registry JSON. */
std::string KnxDptName(const KnxDpt &datapoint_type);
/** @brief Parse text only if the resulting value can be encoded for the configured DPT. */
bool KnxParseValue(const KnxDpt &datapoint_type, const std::string &text, KnxValue &value);
/** @brief Render a typed value for host diagnostics; the implementation is host-only. */
std::string KnxValueToString(const KnxValue &value);
/** @brief Decode telegram data using the configured DPT and reject malformed or invalid values. */
bool KnxDecodeValue(const KnxDpt &datapoint_type, const uint8_t *data, size_t length, KnxValue &value);
/** @brief Encode a typed value into telegram data; clear the output on failure. */
bool KnxEncodeValue(const KnxDpt &datapoint_type, const KnxValue &value, std::vector<uint8_t> &data);
