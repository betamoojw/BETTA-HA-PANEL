/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Boot initialization and cached P4 die temperature diagnostics.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/** @brief A cached sample paired with the latest status. */
typedef struct
{
    bool available;     /**< The latest read produced a finite value. */
    float celsius;      /**< Most recent successful die temperature, in degrees Celsius. */
    int64_t sampled_us; /**< Timestamp of that reading in monotonic microseconds. */
    esp_err_t error;    /**< Latest sensor read or initialization status. */
} chip_temperature_snapshot_t;

/** @brief Initialize sampling at boot; the caller must not block startup on diagnostic failure. */
esp_err_t chip_temperature_init(void);
/** @return The cached status and value without reading the hardware. */
chip_temperature_snapshot_t chip_temperature_snapshot(void);
