/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- added in the BETTA-HA-PANEL fork
 */
/** @file
 * @brief Worker, transport and callback contracts for C6 memory diagnostics.
 */
#pragma once
#include "net/c6_memory_state.h"
#include "esp_err.h"

/** @brief Configure reporting on the diagnostics worker. A busy result means no RPC was started. */
typedef esp_err_t (*c6_memory_configure_fn)(c6_memory_values_t *out, bool *busy);
/** @brief Initialize diagnostics once; a NULL callback indicates unsupported reporting. */
void c6_memory_init(c6_memory_configure_fn configure);
/** @brief Allow or pause configuration without performing RPC or flash I/O in the caller. */
void c6_memory_allow(bool allow);
/** @return True when configuration is permitted and the transport is up. */
bool c6_memory_can_configure(void);
/** @brief Track transport readiness, retaining incident evidence before resetting on link loss. */
void c6_memory_transport(bool up);
/** @brief Start a new configuration generation after coprocessor initialization. */
void c6_memory_cp_init(void);
/** @brief Copy a report from the Hosted callback; do not write flash from that context. */
void c6_memory_report(const c6_memory_values_t *values);
/** @brief Stage an incident for the diagnostics worker using the last attributable sample. */
void c6_memory_note_incident(void);
/** @brief Ask the internal-stack worker to retain an incident before a watchdog restart.
 * @param cause Recorded reboot cause if a current sample can be captured.
 * @param timeout_ms Wait budget in milliseconds; the caller can be on a PSRAM stack.
 * @return True if the requested incident revision was observed as saved before the wait ended.
 */
bool c6_memory_retain_for_restart(uint8_t cause, uint32_t timeout_ms);
/** @brief Serialize cached state and incident copies without accessing flash or the coprocessor. */
void c6_memory_add_json(cJSON *root, cJSON *wifi_diag);
