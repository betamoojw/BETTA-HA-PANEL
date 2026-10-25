// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#pragma once
#include "knx_object.h"
#include <cstddef>
#include <string>
#include <vector>
constexpr size_t kKnxMaximumConfigurationLength = 65535;
constexpr size_t kKnxMaximumObjects = 128;
constexpr char kKnxRuntimeConfigurationPath[] = "/littlefs/interfaces/knxConfig.json";
constexpr char kKnxDefaultConfiguration[] = R"({"media_type":"knx_ip","media_parameters":{"multicast_address":"224.0.23.12","udp_port":3671,"transport_mode":"routing","interface_identifier":"KNX-IP-Interface","nat":false},"physical_address":"15.15.199","comObjects":[]})";
bool KnxParseConfiguration(const std::string& json_text, size_t maximum_objects,
                          size_t maximum_group_addresses,
                          std::vector<KnxCommunicationObject>& objects,
                          std::string& canonical_json, std::string& error,
                          std::string* physical_address = nullptr);
bool KnxLoadConfiguration(std::string& json, std::string& error);
bool KnxSaveConfiguration(const std::string& json, std::string& error);
