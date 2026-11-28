/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Device core dump archive with an HTTP-owned store and a read-only UI metadata cache.
 */
#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "util/coredump_store.h"

/** @brief Import any raw dump after reboot history and LittleFS, before starting HTTP or the UI.
 *
 * A failed import leaves the raw partition intact. After boot, the HTTP
 * task owns archive access other than the count and cached snapshot.
 */
void coredump_archive_init(void);
/** @return The published report count, read atomically without filesystem access. */
unsigned coredump_archive_count(void);
/** @brief Add newest-first `coredump.items` and the compatible newest-report summary. */
void coredump_archive_add_json(cJSON *root);
/** @brief Copy metadata for an exact report ID, or select the newest for a NULL or empty ID.
 * @return False when no report matches; a missing explicit ID never selects another report.
 */
bool coredump_archive_get(const char *id, coredump_record_t *out);
/** @brief Read a bounded byte range of the raw dump, excluding archive metadata. */
bool coredump_archive_read(const char *id, size_t offset, void *buffer, size_t size);
/** @brief Erase only the selected report, including its raw partition copy if still retained.
 * @return ESP_ERR_NOT_FOUND when no report matches the selection.
 */
esp_err_t coredump_archive_delete(const char *id);

/** @brief Copy retained metadata for the UI without waiting or reading flash.
 * @param[out] out Caller-owned storage for up to COREDUMP_STORE_LIMIT records.
 * @param[out] count Number of records copied on success.
 * @return True when the snapshot was copied; false if unavailable, busy, or given invalid output buffers.
 */
bool coredump_archive_snapshot(coredump_record_t out[COREDUMP_STORE_LIMIT], size_t *count);
