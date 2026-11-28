// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723

/** @file
 * @brief Registry parsing, routing parameters and LittleFS persistence.
 */
#pragma once
#include "knx_limits.h"
#include "knx_object.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/** @brief Multicast endpoint and local individual address for KNXnet/IP Routing. */
struct KnxRoutingParameters
{
    std::string multicast_address;
    uint16_t port = 0;
    knx_address_t physical_address = 0;
};

/** @brief Path shared by LittleFS load and atomic save. */
constexpr char kKnxRuntimeConfigurationPath[] = "/littlefs/interfaces/knxConfig.json";

/** @brief Empty registry used when no saved configuration exists. */
constexpr char kKnxDefaultConfiguration[] =
    R"({"media_type":"knx_ip","media_parameters":{"multicast_address":"224.0.23.12","udp_port":3671,"transport_mode":"routing","interface_identifier":"KNX-IP-Interface","nat":false},"physical_address":"15.15.199","comObjects":[]})";

/**
 * @brief Validate a bounded registry without changing the running service.
 * @param json_text Registry JSON to parse.
 * @param[out] objects Parsed objects in input order; cleared on failure.
 * @param[out] canonical_json Compact JSON for persistence; cleared on failure.
 * @param[out] error Validation diagnostic on failure.
 * @param[out] routing Optional routing parameters, written only on success.
 * @return True if the full configuration is valid and serializable.
 */
bool KnxParseConfiguration(std::string_view json_text, std::vector<KnxCommunicationObject> &objects, std::string &canonical_json,
                           std::string &error, KnxRoutingParameters *routing = nullptr);

/** @brief Read the saved registry, using the default only if the file is absent. */
bool KnxLoadConfiguration(std::string &json, std::string &error);

/** @brief Atomically save the registry under the flash gate; report failure in error. */
bool KnxSaveConfiguration(const std::string &json, std::string &error);
