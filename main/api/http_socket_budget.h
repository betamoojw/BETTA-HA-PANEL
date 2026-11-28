/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief HTTP client capacity within the shared lwIP socket budget.
 */
#pragma once

/** @brief Calculate the available client sessions after shared reservations.
 * HTTPD owns three control/listener sockets in addition to its client sessions.
 * Reserve nine more for HA, MQTT, Xiaozhi, DNS/SNTP, ping and the HTTP watchdog.
 * A server-only LRU cannot run when lwIP runs out before the HTTP session limit.
 * Old sdkconfig files with 16 sockets must remain safe until reconfigured.
 * @param total Configured total lwIP sockets, including the reserved sockets.
 * @return Client slots from 0 through 12; startup rejects fewer than four.
 */
static inline int http_socket_client_budget(int total)
{
    int clients = total - 12;
    return clients > 12 ? 12 : (clients > 0 ? clients : 0);
}
