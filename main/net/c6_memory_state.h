/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- added in the BETTA-HA-PANEL fork
 */
/** @file
 * @brief Coprocessor heap samples, generation-scoped monitoring and retained incidents.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"

#define C6_MEMORY_INTERVAL_SEC 60     /**< Requested reporting interval in seconds. */
#define C6_MEMORY_STALE_MS     180000 /**< Maximum sample age in milliseconds for fresh status. */
#define C6_MEMORY_MAX_ATTEMPTS 3      /**< Configuration RPC attempts per transport generation; busy is not an attempt. */

/** @brief Reported heap quantities in bytes; capability heaps include their largest free block. */
typedef struct
{
    uint32_t free_heap, min_free_heap;
    uint32_t internal_free, internal_largest;
    uint32_t dma_free, dma_largest;
    bool has_minimum; /**< Whether the coprocessor supplied the lifetime minimum. */
} c6_memory_values_t;

/** @brief Caller-owned diagnostic state. Timestamps use host monotonic milliseconds. */
typedef struct
{
    c6_memory_values_t values;
    int64_t sampled_ms, retry_at_ms;
    uint32_t generation, sample_generation;
    int32_t last_error;
    uint8_t attempts;
    bool available, allowed, link_up, configured, unsupported;
} c6_memory_state_t;

#define C6_MEMORY_RECORD_MAGIC   0x43364d31U /**< Identifier for C6 incident records, separate from legacy Wi-Fi diagnostics. */
#define C6_MEMORY_RECORD_VERSION 2           /**< Current layout includes restart_cause; older versions are rejected. */

/** @brief Versioned NVS record containing one incident and its associated heap sample. */
typedef struct
{
    uint32_t magic, version, size, boot_id, generation;
    int64_t sampled_ms, incident_ms;
    c6_memory_values_t values;
    uint8_t restart_cause; /**< Watchdog reboot cause, or zero for a connection incident. */
    uint8_t reserved[7];
} c6_memory_incident_t;

/** @brief Clear the state and start the first generation, optionally marking it unsupported. */
void c6_memory_state_init(c6_memory_state_t *state, bool supported);
/** @brief Advance the generation and reset retries, retaining the preceding sample as historical evidence. */
void c6_memory_state_reset(c6_memory_state_t *state, int64_t now_ms);
/** @return True when the current transport may attempt configuration and its retry deadline has elapsed. */
bool c6_memory_state_due(const c6_memory_state_t *state, int64_t now_ms);
/** @return True when the block sizes and optional minimum agree with the reported totals. */
bool c6_memory_values_valid(const c6_memory_values_t *values);
/** @brief Apply an RPC result only to the same usable transport generation.
 * A busy result reschedules without charging the attempt budget.
 */
void c6_memory_state_result(c6_memory_state_t *state, uint32_t generation, int64_t now_ms, int32_t error, bool busy, bool unsupported,
                            const c6_memory_values_t *values);
/** @brief Accept periodic reports only while the current transport is allowed and configured. */
void c6_memory_state_report(c6_memory_state_t *state, int64_t now_ms, const c6_memory_values_t *values);
/** @return Diagnostic label distinguishing current samples from historical ones. */
const char *c6_memory_state_name(const c6_memory_state_t *state, int64_t now_ms);
/** @brief Copy a matching-generation sample into a versioned incident record.
 * @return False when no sample can be attributed to this incident; out is left unchanged.
 */
bool c6_memory_incident_capture(const c6_memory_state_t *state, uint32_t boot_id, int64_t now_ms, c6_memory_incident_t *out);
/** @return True only for a complete record of the current version with consistent fields. */
bool c6_memory_incident_valid(const c6_memory_incident_t *record, size_t size);
/** @brief Add live diagnostic state, including the sample's generation and age when available. */
void c6_memory_state_json(cJSON *root, const c6_memory_state_t *state, int64_t now_ms, uint32_t boot_id);
/** @brief Append a valid retained incident and indicate whether it is from the current host boot. */
void c6_memory_incident_json(cJSON *parent, const c6_memory_incident_t *record, uint32_t boot_id);
