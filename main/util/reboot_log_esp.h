/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Device-side reboot history, backed by an RTC breadcrumb and an NVS ring.
 */
#pragma once

#include "cJSON.h"
#include "util/reboot_log.h"

/** @brief Record the previous run and start the new breadcrumb once per boot, after NVS is ready. */
void reboot_log_init(void);
/** @brief Mark the deliberate cause immediately before calling `esp_restart()`. */
void reboot_log_mark(reboot_cause_t cause);
/** @brief Add `boot_seq` and newest-first `entries` to the caller's JSON object. */
void reboot_log_add_json(cJSON *root);
/** @return A borrowed label for the last run, empty before initialization. */
const char *reboot_log_last_label(void);
/** @return The last deliberate restart cause, or REBOOT_CAUSE_NONE if not recorded. */
reboot_cause_t reboot_log_last_cause(void);
/** @return The last run's failure classification; false before initialization. */
bool reboot_log_last_run_failed(void);
/** @return True when the newest retained panic entry was copied for raw core dump import. */
bool reboot_log_latest_panic(reboot_entry_t *out);
