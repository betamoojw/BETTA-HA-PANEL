/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * The optional automatic firmware update ("Install updates automatically", off by default). Once a minute a task asks the pure decision
 * (util/auto_update.c) whether the panel may install the release the update check found, and if so starts the same install as the
 * editor's update button. It remembers the attempt in NVS (util/auto_update_state.c) so that after the restart it knows whether the
 * panel came up on the new firmware or the bootloader returned to the old one; a rolled-back version is not tried again and the panel
 * shows a notice.
 */
/** @file
 * @brief Automatic firmware update lifecycle and status reporting.
 */
#pragma once

#include <stdbool.h>

#include "cJSON.h"
#include "sdkconfig.h"

/** @brief Resolve any retained attempt and start the automatic-update worker.
 * @pre Call once at startup after the reboot log and release checker are initialized.
 */
void auto_update_task_start(bool enabled);
/** @brief Apply the new install preference and wake the worker without restarting firmware. */
void auto_update_task_configure(bool enabled);
#if CONFIG_APP_AUTO_UPDATE_TEST_HOOK
/** @brief Change test-only update policy or seed a retained attempt, then wake the worker.
 * @param force Negative leaves the override alone; zero clears it; positive enables it.
 * @param reset Clear retained test state.
 * @param attempt Optional tag to persist as an in-progress attempt for rollback tests.
 */
void auto_update_test_hook(int force, bool reset, const char *attempt);
#endif
/** @brief Append current policy, progress and retained result to caller-owned OTA status JSON. */
void auto_update_add_status_json(cJSON *root);
