// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx_config.h"
extern "C" {
#include "util/flash_gate.h"
#include "util/atomic_file.h"
}
#include "esp_littlefs.h"
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

bool KnxLoadConfiguration(std::string& json, std::string& error) {
    if (!esp_littlefs_mounted("storage")) { error = "LittleFS is not mounted"; return false; }
    FILE* f = fopen(kKnxRuntimeConfigurationPath, "rb");
    if (!f) {
        if (errno == ENOENT) { json = kKnxDefaultConfiguration; return true; }
        error = "Cannot read knxConfig.json"; return false;
    }
    bool ok = fseek(f, 0, SEEK_END) == 0;
    long size = ok ? ftell(f) : -1;
    ok = size > 0 && size <= static_cast<long>(kKnxMaximumConfigurationLength) && fseek(f, 0, SEEK_SET) == 0;
    if (ok) { json.resize(size); ok = fread(json.data(), 1, size, f) == static_cast<size_t>(size); }
    fclose(f);
    if (!ok) { json.clear(); error = "Incomplete or oversized knxConfig.json"; }
    return ok;
}

bool KnxSaveConfiguration(const std::string& json, std::string& error) {
    if (!esp_littlefs_mounted("storage") || !flash_gate_enter(1000)) {
        error = "Storage is unavailable or busy"; return false;
    }
    bool ok = mkdir("/littlefs/interfaces", 0700) == 0 || errno == EEXIST;
    if (ok) ok = atomic_file_write_locked(kKnxRuntimeConfigurationPath, json.data(), json.size()) == ESP_OK;
    if (!ok) error = "Could not commit knxConfig.json";
    flash_gate_leave();
    return ok;
}
