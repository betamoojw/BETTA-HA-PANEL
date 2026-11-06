/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The caller serializes access between the WebSocket callback and worker.
 * A real error is sticky: a later normal CLOSE cannot erase its diagnosis. */
typedef enum {
    XZ_END_NONE, XZ_END_NORMAL, XZ_END_NO_STATUS,
    XZ_END_ABNORMAL, XZ_END_INVALID, XZ_END_TRANSPORT, XZ_END_DISCONNECTED,
    XZ_END_SEND, XZ_END_HANDSHAKE_TIMEOUT, XZ_END_LOCAL
} xiaozhi_end_t;
typedef struct {
    bool connected, peer_close, stopping;
    xiaozhi_end_t end;
    int close_code, error_type, tls_error, socket_errno, http_status;
} xiaozhi_connection_t;

static inline bool xiaozhi_connection_error(const xiaozhi_connection_t *c)
{ return c->end >= XZ_END_ABNORMAL; }
static inline bool xiaozhi_connection_can_send(const xiaozhi_connection_t *c)
{ return c->connected && !c->stopping && c->end == XZ_END_NONE; }
static inline void xiaozhi_connection_fail(xiaozhi_connection_t *c, xiaozhi_end_t end)
{
    if (!xiaozhi_connection_error(c)) c->end = end;
}
static inline const char *xiaozhi_connection_reason(const xiaozhi_connection_t *c)
{
    switch (c->end) {
    case XZ_END_NORMAL: return "normal_close";
    case XZ_END_NO_STATUS: return "close_without_status";
    case XZ_END_ABNORMAL: return "abnormal_close";
    case XZ_END_INVALID: return "invalid_close";
    case XZ_END_TRANSPORT: return "transport_error";
    case XZ_END_DISCONNECTED: return "unexpected_disconnect";
    case XZ_END_SEND: return "send_error";
    case XZ_END_HANDSHAKE_TIMEOUT: return "handshake_timeout";
    case XZ_END_LOCAL: return "local_error";
    default: return "none";
    }
}

/* RFC 6455 CLOSE reasons are UTF-8; reject malformed text without logging it. */
static inline bool xiaozhi_close_utf8(const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        uint32_t cp = s[i++];
        unsigned extra;
        uint32_t minimum;
        if (cp < 0x80) continue;
        if (cp >= 0xc2 && cp <= 0xdf) { extra = 1; minimum = 0x80; cp &= 0x1f; }
        else if (cp >= 0xe0 && cp <= 0xef) { extra = 2; minimum = 0x800; cp &= 0x0f; }
        else if (cp >= 0xf0 && cp <= 0xf4) { extra = 3; minimum = 0x10000; cp &= 7; }
        else return false;
        if (n - i < extra) return false;
        while (extra--) {
            if ((s[i] & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (s[i++] & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}

static inline void xiaozhi_connection_close_code(xiaozhi_connection_t *c, int code)
{
    if (c->peer_close) return; /* Keep the first CLOSE, including malformed input. */
    c->peer_close = true;
    c->close_code = code;
    if (xiaozhi_connection_error(c)) return;
    c->end = code == 0 ? XZ_END_NO_STATUS :
        (code == 1000 || code == 1001) ? XZ_END_NORMAL : XZ_END_ABNORMAL;
}

static inline void xiaozhi_connection_close_frame(xiaozhi_connection_t *c,
    const void *data, int length, int payload_length, int offset, bool fin)
{
    if (c->peer_close) return;
    const uint8_t *bytes = data;
    if (!fin || offset != 0 || length < 0 || length != payload_length || length > 125 ||
        length == 1 || (length && !bytes) ||
        (length >= 2 && !xiaozhi_close_utf8(bytes + 2, (size_t)length - 2))) {
        c->peer_close = true;
        xiaozhi_connection_fail(c, XZ_END_INVALID);
        return;
    }
    int code = length ? ((int)bytes[0] << 8) | bytes[1] : 0;
    if (length && (code < 1000 || code >= 5000 || code == 1004 || code == 1005 || code == 1006 || code == 1015)) {
        c->peer_close = true; c->close_code = code;
        xiaozhi_connection_fail(c, XZ_END_INVALID);
        return;
    }
    xiaozhi_connection_close_code(c, code);
}

static inline void xiaozhi_connection_send_result(xiaozhi_connection_t *c, int sent, int wanted)
{
    /* CLOSE is delivered under the same transport lock before a queued send
     * can be rejected. Only that observed close suppresses a send failure. */
    if (sent != wanted && c->end == XZ_END_NONE) xiaozhi_connection_fail(c, XZ_END_SEND);
}

static inline void xiaozhi_connection_finished(xiaozhi_connection_t *c)
{
    if (!c->stopping && c->end == XZ_END_NONE)
        xiaozhi_connection_fail(c, XZ_END_DISCONNECTED);
}
