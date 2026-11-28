/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief In-memory transitions for the automatic update record kept across restarts.
 *
 * The scheduler owns NVS persistence; these functions do not write flash.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define AUTO_UPDATE_TAG_LEN 32

/** @brief Persisted result codes; keep their numeric values stable. */
typedef enum
{
    AUTO_UPDATE_RESULT_NONE = 0,    /**< No result recorded. */
    AUTO_UPDATE_RESULT_INSTALLED,   /**< The panel booted the attempted version. */
    AUTO_UPDATE_RESULT_ROLLED_BACK, /**< The panel returned to a different version after a failure. */
    AUTO_UPDATE_RESULT_FAILED       /**< The attempt did not finish; no rollback block is set by this result. */
} auto_update_result_t;

/** @brief Versioned attempt record stored as an NVS blob by the scheduler. */
typedef struct
{
    uint32_t version;                      /**< Persistent record layout version. */
    char attempt_tag[AUTO_UPDATE_TAG_LEN]; /**< Empty when no attempt is pending. */
    int64_t attempt_epoch;                 /**< Wall-clock seconds when the attempt started, or zero. */
    int32_t last_attempt_day;              /**< Local day number used to allow at most one attempt per day. */
    char blocked_tag[AUTO_UPDATE_TAG_LEN]; /**< The rolled-back release to exclude from automatic attempts. */
    uint8_t result;                        /**< Value from auto_update_result_t. */
    char result_tag[AUTO_UPDATE_TAG_LEN];
    int64_t result_epoch;   /**< Wall-clock seconds when the result was recorded, or zero. */
    uint8_t notice_pending; /**< The rollback notice has not yet been shown on the panel. */
} auto_update_state_t;

#define AUTO_UPDATE_STATE_VERSION 1u

/** @brief Initialize an empty record with the current layout version. */
void auto_update_state_init(auto_update_state_t *state);
/** @brief Record the start of an install for the local day and wall time (zero if unknown). */
void auto_update_state_begin(auto_update_state_t *state, const char *tag, int64_t epoch, int32_t day);
/** @brief Finish an attempt that failed before restart, without blocking the release. */
void auto_update_state_failed(auto_update_state_t *state, int64_t epoch);
/** @brief Resolve a pending attempt using the running version and previous reset classification.
 *
 * The attempted version counts as installed. A different version is a
 * rollback only when the previous run ended in failure; otherwise the
 * attempt failed without a rollback block.
 *
 * @return True when the record changed and the caller should persist it.
 */
bool auto_update_state_resolve_boot(auto_update_state_t *state, const char *running_version, bool previous_run_failed, int64_t epoch);
/** @brief Acknowledge the displayed rollback notice without clearing the blocked release. */
void auto_update_state_ack_notice(auto_update_state_t *state);

/** @return A static result name, or an empty string when no result is recognized. */
const char *auto_update_result_name(auto_update_result_t result);
