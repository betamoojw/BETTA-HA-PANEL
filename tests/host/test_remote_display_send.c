/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "api/remote_display_send.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { FD = 5, BATCH = 7 * (30 + 32 * 32 * 2) };
static char wire[BATCH + 64], pixels[BATCH];
static size_t used, step, steps_left;
static int64_t now_us;
static unsigned warnings;

typedef struct {
    char op;           /* s = send byte count, p = poll event mask, negative = errno */
    int result, advance_ms, expected_wait_ms;
} action_t;
static const action_t *script;

static void start(const action_t *actions, size_t count)
{
    assert(steps_left == 0);
    script = actions;
    steps_left = count;
    step = used = now_us = warnings = 0;
}

static action_t next(char op)
{
    assert(steps_left && script[step].op == op);
    action_t a = script[step++];
    --steps_left;
    now_us += a.advance_ms * 1000;
    if (a.result < 0) errno = -a.result;
    return a;
}

int64_t esp_timer_get_time(void) { return now_us; }
void host_log(const char *level, const char *tag, const char *fmt, ...)
{
    (void)tag; (void)fmt;
    assert(strcmp(level, "W") == 0);
    ++warnings;
}

int lwip_send(int fd, const void *data, size_t len, int flags)
{
    assert(fd == FD && (flags & MSG_DONTWAIT));
    action_t a = next('s');
    if (a.result < 0) return -1;
    assert((size_t)a.result <= len && used + a.result <= sizeof(wire));
    memcpy(wire + used, data, a.result);
    used += a.result;
    return a.result;
}

int lwip_poll(struct pollfd *fds, size_t count, int timeout_ms)
{
    assert(count == 1 && fds->fd == FD && fds->events == POLLOUT);
    action_t a = next('p');
    assert(timeout_ms == a.expected_wait_ms);
    if (a.result <= 0) return a.result < 0 ? -1 : 0;
    fds->revents = a.result;
    return 1;
}

static void split_batch_and_pong(void)
{
    const char header[] = {(char)0x82, 126, BATCH >> 8, BATCH & 255};
    const char pong[] = "{\"type\":\"pong\",\"control\":false}";
    const char text[] = {(char)0x81, sizeof(pong) - 1};
    const action_t actions[] = {
        {'s', 2, 0, 0}, {'s', 2, 0, 0},
        {'s', 8192, 0, 0}, {'s', -EAGAIN, 0, 0}, {'p', -EINTR, 50, 750},
        {'s', -EWOULDBLOCK, 0, 0}, {'p', POLLOUT, 250, 700},
        {'s', -EINTR, 0, 0}, {'s', BATCH - 8192, 0, 0},
        {'s', 2, 0, 0}, {'s', sizeof(pong) - 1, 0, 0}
    };
    start(actions, sizeof(actions) / sizeof(*actions));
    assert(remote_display_send(NULL, FD, header, sizeof(header), 0) == sizeof(header));
    assert(remote_display_send(NULL, FD, pixels, BATCH, 0) == BATCH);
    assert(remote_display_send(NULL, FD, text, sizeof(text), 0) == sizeof(text));
    assert(remote_display_send(NULL, FD, pong, sizeof(pong) - 1, 0) == sizeof(pong) - 1);
    assert(!steps_left && !warnings && used == sizeof(header) + BATCH + sizeof(text) + sizeof(pong) - 1);
    assert(!memcmp(wire, header, sizeof(header)) && !memcmp(wire + sizeof(header), pixels, BATCH));
    assert(!memcmp(wire + sizeof(header) + BATCH, text, sizeof(text)));
    assert(!memcmp(wire + sizeof(header) + BATCH + sizeof(text), pong, sizeof(pong) - 1));
}

static void expect_failure(const action_t *actions, size_t count, size_t accepted)
{
    start(actions, count);
    assert(remote_display_send(NULL, FD, pixels, BATCH, 0) == HTTPD_SOCK_ERR_FAIL);
    assert(!steps_left && warnings == 1 && used == accepted && !memcmp(wire, pixels, used));
}

int main(void)
{
    for (size_t i = 0; i < sizeof(pixels); ++i) pixels[i] = (char)(i % 251);
    split_batch_and_pong();
    const action_t timeout[] = {
        {'s', 8192, 0, 0}, {'s', -EAGAIN, 0, 0}, {'p', POLLOUT, 500, 750},
        {'s', 1024, 0, 0}, {'s', -EAGAIN, 0, 0}, {'p', 0, 250, 250}
    };
    expect_failure(timeout, sizeof(timeout) / sizeof(*timeout), 9216);
    assert(now_us == 750000);
    const action_t ready_at_deadline[] = {
        {'s', 8192, 5, 0}, {'s', -EAGAIN, 0, 0}, {'p', POLLOUT, 745, 745}
    };
    expect_failure(ready_at_deadline, sizeof(ready_at_deadline) / sizeof(*ready_at_deadline), 8192);
    assert(now_us == 750000);
    const action_t interrupted[] = {{'s', -EINTR, 750, 0}};
    expect_failure(interrupted, 1, 0);
    const action_t zero[] = {{'s', 0, 0, 0}};
    expect_failure(zero, 1, 0);
    const action_t reset[] = {{'s', 16, 0, 0}, {'s', -ECONNRESET, 0, 0}};
    expect_failure(reset, 2, 16);
    const action_t hangup[] = {{'s', -EAGAIN, 0, 0}, {'p', POLLOUT | POLLHUP, 0, 750}};
    expect_failure(hangup, 2, 0);
    const action_t poll_error[] = {{'s', -EAGAIN, 0, 0}, {'p', -EBADF, 0, 750}};
    expect_failure(poll_error, 2, 0);
    assert(remote_display_send(NULL, FD, NULL, 0, 0) == 0);
    assert(remote_display_send(NULL, FD, NULL, 1, 0) == HTTPD_SOCK_ERR_INVALID);
    assert(remote_display_send(NULL, FD, pixels, (size_t)INT_MAX + 1, 0) == HTTPD_SOCK_ERR_INVALID);
    puts("Live Display bounded writes, frame boundaries and peer failures passed.");
}
