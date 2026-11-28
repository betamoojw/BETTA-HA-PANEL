/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Schedule Recorder history fetching for visible graph widgets.
 *
 * The shared history queue can replace a pending request, so the widget
 * feed allows one outstanding request and times it out if no answer arrives.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HISTORY_FEED_MAX        8
#define HISTORY_FEED_REFRESH_MS (15LL * 60LL * 1000LL) /* a shown history is refreshed every 15 minutes */
#define HISTORY_FEED_RETRY_MS   (30LL * 1000LL)        /* after a failed or unanswered request */
#define HISTORY_FEED_TIMEOUT_MS (45LL * 1000LL)        /* an unanswered request is forgotten after this */

/** @brief Visibility and request timestamps for one graph slot, in monotonic milliseconds. */
typedef struct
{
    bool used;
    bool visible;        /* the widget's page is on screen */
    int64_t last_ok_ms;  /**< Last successful fetch, or zero if none. */
    int64_t last_try_ms; /**< Last attempt, or zero if none. */
} history_feed_slot_t;

/** @brief One outstanding request and the time it started. */
typedef struct
{
    int inflight; /**< Slot index, or -1 when no request is pending. */
    int64_t inflight_since_ms;
} history_feed_state_t;

/** @brief Slot indices for the caller's timeout and scheduling actions. */
typedef struct
{
    int timed_out; /**< Slot whose timeout the caller must record, or -1 for none. */
    int next;      /**< Slot to request now, or -1 if none is due. */
} history_feed_decision_t;

/** @brief Start with no outstanding request. */
void history_feed_state_init(history_feed_state_t *state);
/** @brief Evaluate timeouts and choose the least recently fetched eligible visible slot.
 *
 * Uses monotonic now_ms and does not modify the slots or state. The caller
 * records any timeout and starts the returned request.
 */
history_feed_decision_t history_feed_decide(const history_feed_slot_t *slots, int count, const history_feed_state_t *state, int64_t now_ms);
