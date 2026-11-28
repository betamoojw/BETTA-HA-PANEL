/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared kilometre conversion for range widgets and general distance readouts.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/** @brief Convert `m` or `mi` to kilometres in place.
 * @return True also for `km` or a missing unit, leaving the value unchanged.
 *         False for an unsupported unit or missing value pointer.
 */
bool distance_to_km(const char *unit, double *value);

/** @brief Format a finite metre reading of at least one kilometre in magnitude.
 *
 * Only applies to Home Assistant class `distance` with unit `m`. Uses at
 * most one decimal and drops a trailing `.0`.
 *
 * @return True when kilometre formatting applied. Otherwise keep the original readout.
 */
bool distance_readout_km(const char *device_class, const char *unit, const char *value_text, char *out, size_t out_len);
