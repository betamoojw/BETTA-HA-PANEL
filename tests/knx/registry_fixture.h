// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#pragma once
#include "knx_config.h"

// Each point has its own group address and enough metadata to exercise large files.
inline std::string registry_fixture(unsigned count, std::string json = kKnxDefaultConfiguration) {
    json.resize(json.find("[]") + 1);
    for (unsigned i = 0; i < count; ++i) {
        if (i) json += ',';
        json += R"({"id":"point-)" + std::to_string(i) +
            R"(","name":"Commissioning point )" + std::to_string(i) +
            R"(","description":"Building lighting command and independent commissioning feedback","group_address":"1/)" +
            std::to_string(i / 256) + "/" + std::to_string(i % 256) +
            R"(","datapoint_type":"DPT-1.001","readable":true,"writable":true})";
    }
    return json + "]}";
}
