/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Internal settings-store credential staging and legacy migration.
 *
 * The runtime settings store serializes these operations and owns flash access.
 */
#pragma once

#include <stdint.h>
#include "settings/runtime_settings.h"

/** @brief Read credentials consistent with the parsed public settings.
 *
 * Generation zero supports the legacy keys and their staged migration
 * snapshot. Later generations require an exact matching record and scope.
 */
esp_err_t runtime_secrets_load(uint32_t generation, runtime_settings_t *settings);
/** @brief Commit the secret record before publishing its generation in the public file. */
esp_err_t runtime_secrets_stage(uint32_t generation, const runtime_settings_t *settings);
/** @brief Clear legacy credentials for connections whose scope or secret changed.
 *
 * Write empty values rather than removing keys so older firmware cannot use
 * compiled-in credential defaults for those connections after rollback.
 */
esp_err_t runtime_secrets_retire_legacy(const runtime_settings_t *previous, const runtime_settings_t *next);
