/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Pure restart policy for sustained network or loopback HTTP failures.
 *
 * The loopback server check also applies in setup mode. Transport recovery
 * requires evidence that the gateway used to answer and that Home Assistant
 * is not connected. The policy has a boot grace period and a restart cap.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NET_WD_BOOT_GRACE_S  300
#define NET_WD_LINK_DOWN_S   600
#define NET_WD_PING_DEAD_S   600
#define NET_WD_CLEAR_AFTER_S 600
#define NET_WD_HTTP_DEAD_S   300
#define NET_WD_MAX_RESTARTS  3

/** @brief Start times of sustained conditions, in uptime seconds or -1 when inactive. */
typedef struct
{
    int64_t link_down_since; /**< Expected network link went down. */
    int64_t ping_fail_since; /**< A previously responsive gateway stopped answering. */
    int64_t alive_since;     /**< The link became healthy and the gateway was not failing. */
    int64_t http_fail_since; /**< The HTTP loopback probe stopped answering. */
} net_wd_state_t;

/** @brief Current link and probe observations plus the retained restart count. */
typedef struct
{
    bool wifi_expected; /**< Monitor the configured network link, not a setup-AP-only connection. */
    bool link_up;       /**< A usable connection has an IP address. */
    bool ha_connected;  /**< A live Home Assistant connection proves traffic is flowing. */
    int ping;           /**< Gateway probe: 1 answered, 0 failed, -1 unknown. */
    bool ping_ever_ok;  /**< The current gateway answered at least one probe. */
    int http;           /**< HTTP loopback probe: 1 answered, 0 failed, -1 unknown. */
    uint32_t restarts_in_a_row;
} net_wd_input_t;

/** @brief Outcome of one watchdog evaluation. */
typedef enum
{
    NET_WD_OK = 0,            /**< No restart is due. */
    NET_WD_RESTART_LINK,      /**< Sustained loss of an expected link. */
    NET_WD_RESTART_TRANSPORT, /**< Link up but no gateway reply or live Home Assistant connection. */
    NET_WD_RESTART_HTTP,      /**< Sustained failure of the local HTTP server. */
    NET_WD_SUPPRESSED,        /**< Restart due, but the consecutive restart limit was reached. */
} net_wd_decision_t;

/** @brief Reset all condition timers to inactive. */
void net_wd_init(net_wd_state_t *state);
/** @brief Update condition timers and evaluate recovery at now_s seconds since boot. */
net_wd_decision_t net_wd_step(net_wd_state_t *state, const net_wd_input_t *in, int64_t now_s);
/** @return True when sustained network health lets the caller clear the restart count. */
bool net_wd_may_clear_restarts(const net_wd_state_t *state, int64_t now_s);
