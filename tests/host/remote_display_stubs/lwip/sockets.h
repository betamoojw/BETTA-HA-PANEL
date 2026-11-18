/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#define MSG_DONTWAIT 8
enum { POLLOUT = 4, POLLERR = 8, POLLHUP = 16, POLLNVAL = 32 };
struct pollfd { int fd; short events, revents; };
int lwip_send(int fd, const void *data, size_t len, int flags);
int lwip_poll(struct pollfd *fds, size_t count, int timeout_ms);
