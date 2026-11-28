/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Bounded socket writes for Live Display WebSocket frames.
 */
#include "api/remote_display_send.h"
#include <errno.h>
#include <limits.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"

/** @brief Monotonic time budget for one WebSocket header or payload write, in microseconds. */
#define SEND_BUDGET_US 750000

int remote_display_send(httpd_handle_t server, int fd, const char *data, size_t len, int flags)
{
    (void) server;
    if (len > INT_MAX || (len && !data))
        return HTTPD_SOCK_ERR_INVALID;
    const int64_t deadline = esp_timer_get_time() + SEND_BUDGET_US;
    size_t sent = 0;
    int cause = ETIMEDOUT;
    while (sent < len)
    {
        if (esp_timer_get_time() >= deadline)
            break;
        int n = lwip_send(fd, data + sent, len - sent, flags | MSG_DONTWAIT);
        if (n > 0)
        {
            sent += (size_t) n;
            continue;
        }
        if (n == 0)
        {
            cause = EPIPE;
            break;
        }
        if (errno == EINTR)
            continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            cause = errno;
            break;
        }

        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0)
            break;
        struct pollfd socket = {.fd = fd, .events = POLLOUT};
        int ready = lwip_poll(&socket, 1, (int) ((remaining + 999) / 1000));
        if (ready == 0)
            break;
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            cause = errno;
            break;
        }
        if (!(socket.revents & POLLOUT) || (socket.revents & (POLLERR | POLLHUP | POLLNVAL)))
        {
            cause = EPIPE;
            break;
        }
    }
    if (sent == len)
        return (int) len;
    ESP_LOGW("live_display", "Closing slow or failed send at %u/%u bytes (errno %d)", (unsigned) sent, (unsigned) len, cause);
    return HTTPD_SOCK_ERR_FAIL;
}
