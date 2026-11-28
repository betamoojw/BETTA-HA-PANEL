/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Persists wifi_mgr's connectivity diagnostics (last disconnect reason, reconnect attempts, C6
 * hard-recover attempts and their result) to NVS, so the record of the most recent real incident
 * survives a reset forced to end it -- the exact situation a 2026-09-24 SDIO/C6 wedge left no way
 * to inspect otherwise (HTTP unreachable, and opening the serial port to look would itself reset
 * the board and lose the same information). Kept in its own tiny file rather than inline in
 * wifi_mgr.c so the NVS namespace/key and the rate limiting are in one obvious place.
 */
/** @file
 * @brief Persistence contracts for Wi-Fi recovery and C6 incident diagnostics.
 */
#pragma once

#include <stdbool.h>

#include "net/wifi_mgr.h"
#include "net/c6_memory_state.h"

/** @brief Try to retain a diagnostic snapshot under the flash gate.
 * Routine disconnect bursts are rate-limited;
 * critical C6 hard-recover results bypass the limit so a reset immediately
 * after a recovery attempt can retain its result.
 */
void wifi_diag_store_save(const wifi_mgr_diag_t *diag, bool critical);

/** @brief Read the most recently retained Wi-Fi snapshot.
 * @return False for a missing, unreadable, or wrong-sized record; a supplied output is zeroed.
 */
bool wifi_diag_store_load(wifi_mgr_diag_t *out);

/** @brief Validate and commit an incident under its separate versioned key.
 * @pre Call from the diagnostics worker, not a Hosted callback.
 */
bool wifi_diag_store_save_c6(const c6_memory_incident_t *record);
/** @return True for a readable, valid incident. Failure zeroes a supplied output. */
bool wifi_diag_store_load_c6(c6_memory_incident_t *out);
