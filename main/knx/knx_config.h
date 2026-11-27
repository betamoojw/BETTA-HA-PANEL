// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#pragma once
#include "knx_limits.h"
#include "knx_object.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
struct KnxRoutingParameters {
    std::string multicast_address;
    uint16_t port = 0;
    knx_address_t physical_address = 0;
};
constexpr char kKnxRuntimeConfigurationPath[] = "/littlefs/interfaces/knxConfig.json";
constexpr char kKnxDefaultConfiguration[] = R"({"media_type":"knx_ip","media_parameters":{"multicast_address":"224.0.23.12","udp_port":3671,"transport_mode":"routing","interface_identifier":"KNX-IP-Interface","nat":false},"physical_address":"15.15.199","comObjects":[]})";
bool KnxParseConfiguration(std::string_view json_text,
                          std::vector<KnxCommunicationObject>& objects,
                          std::string& canonical_json, std::string& error,
                          KnxRoutingParameters* routing = nullptr);
bool KnxLoadConfiguration(std::string& json, std::string& error);
bool KnxSaveConfiguration(const std::string& json, std::string& error);
