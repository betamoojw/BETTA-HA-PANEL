/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xiaozhi/xiaozhi.h"
#include "xiaozhi/xiaozhi_connection.h"
/* Expected negative controls must not open crash-report dialogs on Windows. */
#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(99); } } while (0)
#define portMAX_DELAY 0
#define pdMS_TO_TICKS(n) (n)
#define pdTRUE 1
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#define MAX_FRAME 2048
#define ESP_LOGI(...) ((void)0)
enum { WEBSOCKET_EVENT_CONNECTED, WEBSOCKET_EVENT_ERROR, WEBSOCKET_EVENT_DISCONNECTED,
       WEBSOCKET_EVENT_FINISH, WEBSOCKET_EVENT_CLOSED, WEBSOCKET_EVENT_DATA };
enum { WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT = 1, WEBSOCKET_ERROR_TYPE_HANDSHAKE };
typedef void *esp_event_base_t;
typedef struct {
    int error_type, esp_ws_handshake_status_code, esp_tls_stack_err, esp_transport_sock_errno;
} event_error_t;
typedef struct {
    const void *data_ptr;
    int data_len, payload_len, payload_offset, op_code, close_status_code;
    bool fin;
    event_error_t error_handle;
} esp_websocket_event_data_t;
typedef struct { int opcode; size_t len; uint8_t *data; } packet_t;
static int lock, packets, mutex_depth, send_calls, send_return, stop_calls, close_during_send;
static bool late_error, failed, ready;
static bool want_listen, auto_listen, idle_capture, audio_stopping, hello_sent;
static int64_t wake_retry;
static char session[128];
static int stream;
static void *encoder, *decoder, *pcm, *encoded, *duplex_frame;
static void *manual_vad;
static int64_t deadline;
static void *ws;
static uint8_t *rx;
static size_t rx_len;
static int rx_opcode;
static xiaozhi_connection_t connection;
static xiaozhi_status_t status;
static void ws_event(void *, esp_event_base_t, int32_t, void *);
static void event(int id) { ws_event(NULL, NULL, id, NULL); }
static void close_event(int code)
{
    unsigned char bytes[] = {(unsigned char)(code >> 8), (unsigned char)code};
    esp_websocket_event_data_t e = {.data_ptr = bytes, .data_len = code ? 2 : 0,
        .payload_len = code ? 2 : 0, .fin = true, .op_code = 8, .close_status_code = 0};
    /* The SDK's DATA callback precedes updating close_status_code. */
    ws_event(NULL, NULL, WEBSOCKET_EVENT_DATA, &e);
}
static void transport_error(void)
{
    esp_websocket_event_data_t e = {.error_handle = {
        .error_type = WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT, .esp_tls_stack_err = -0x7880,
        .esp_transport_sock_errno = 104}};
    ws_event(NULL, NULL, WEBSOCKET_EVENT_ERROR, &e);
    event(WEBSOCKET_EVENT_DISCONNECTED);
}
static void xSemaphoreTake(int handle, int ticks)
{ (void)handle; (void)ticks; assert(mutex_depth++ == 0); }
static void xSemaphoreGive(int handle)
{ (void)handle; assert(--mutex_depth == 0); }
static void *heap_caps_malloc(size_t n, int caps) { (void)caps; return malloc(n); }
static int xQueueSend(int queue, const packet_t *p, int wait)
{ (void)queue; (void)wait; free(p->data); return pdTRUE; }
static size_t strlcpy(char *dst, const char *src, size_t n)
{ size_t length = strlen(src); snprintf(dst, n, "%s", src); return length; }
static int send_mock(void *client, const void *data, int n, int wait)
{
    (void)data;
    assert(client == ws && n > 0 && wait == 500 && mutex_depth == 0);
    ++send_calls;
    /* Deterministic interleaving: CLOSE is delivered while the sender waits
     * on the SDK's shared lock, before its post-lock connected check. */
    if (close_during_send) close_event(close_during_send);
    return send_return;
}
#define esp_websocket_client_send_bin send_mock
#define esp_websocket_client_send_text send_mock
static int esp_websocket_client_stop(void *client)
{
    assert(mutex_depth == 0 && client == ws && connection.stopping);
    ++stop_calls;
    if (late_error) transport_error(); /* A final callback while stop joins. */
    event(WEBSOCKET_EVENT_FINISH);
    return ESP_OK;
}
static void esp_websocket_client_destroy(void *client) { assert(client == ws); }
static void conversation(bool visible) { assert(!visible); }
static void knx_mcp_session_reset(void) {}
static int64_t esp_timer_get_time(void) { return 10; }
static void xiaozhi_wake_reset(void) {}
static int xiaozhi_duplex_stop(void) { return ESP_OK; }
static void xiaozhi_audio_close(void) {}
static void xiaozhi_session_reset(int *s, bool realtime) { assert(s == &stream && !realtime); }
static void drain_packets(bool preserve) { assert(!preserve); }
static void update_audio_status(void) {}
static void vad_destroy(void *p) { assert(p); }
static void xiaozhi_activity_end(uint32_t generation) { assert(generation == status.dialog_seq); }
static void set_state(const char *s) { strlcpy(status.state, s, sizeof(status.state)); }
#include "xiaozhi_connection_functions.h"

static void start(void)
{
    status.enabled = true;
    connection_reset();
    ready = true; deadline = 1000; ws = &connection;
    rx = malloc(MAX_FRAME); assert(rx);
    send_calls = stop_calls = close_during_send = 0;
    send_return = 4; late_error = false;
    event(WEBSOCKET_EVENT_CONNECTED);
    status.listening = status.speaking = status.duplex_active = true;
    status.dialog_active = true; ++status.dialog_seq;
    set_state("speaking_listening");
    assert(connection_can_send());
}
static void end(bool error, const char *reason)
{
    finish_connection(10);
    assert(!ws && stop_calls == 1 && !connection_can_send());
    assert(!status.speaking && !status.listening && !status.duplex_active);
    assert(!status.dialog_active && status.dialog_end_seq == status.dialog_seq);
    assert(!strcmp(status.dialog_end_reason, error ? "error" : "peer_close"));
    assert(!strcmp(status.state, error ? "connection_error" : "ready"));
    assert(!strcmp(status.connection_end, reason));
    uint32_t errors = status.connection_errors, normal = status.normal_closes;
    finish_connection(20);
    assert(status.connection_errors == errors && status.normal_closes == normal && stop_calls == 1);
}
static void normal(void)
{
    int codes[] = {1000, 1001, 0};
    for (unsigned i = 0; i < 3; ++i) {
        start();
        close_event(codes[i]);
        assert(!connection_can_send() && !send_packet("test", 4, true) && send_calls == 0);
        esp_websocket_event_data_t e = {.close_status_code = codes[i]};
        ws_event(NULL, NULL, WEBSOCKET_EVENT_CLOSED, &e);
        event(WEBSOCKET_EVENT_FINISH);
        end(false, codes[i] ? "normal_close" : "close_without_status");
        assert(status.connection_close_code == codes[i]);
        assert(status.normal_closes == i + 1 && status.connection_errors == 0);
    }
    start(); /* CLOSED-only fallback still records the SDK's final code. */
    esp_websocket_event_data_t e = {.close_status_code = 1000};
    ws_event(NULL, NULL, WEBSOCKET_EVENT_CLOSED, &e);
    end(false, "normal_close");
}
static void queued_close(void)
{
    for (int binary = 0; binary < 2; ++binary) {
        start(); close_during_send = 1000; send_return = -1;
        assert(!send_packet("test", 4, binary));
        assert(send_calls == 1 && !failed);
        end(false, "normal_close");
    }
    assert(status.normal_closes == 2 && status.connection_errors == 0);
}
static void abnormal(void)
{
    int codes[] = {1002, 1008, 1011, 1012, 1013, 4001};
    for (unsigned i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i) {
        start(); close_event(codes[i]); close_event(1000);
        end(true, "abnormal_close");
        assert(status.connection_close_code == codes[i]);
    }
    assert(status.normal_closes == 0 && status.connection_errors == 6);
}
static void malformed(void)
{
    unsigned char bytes[] = {3, 232, 0xed, 0xa0, 0x80}; /* UTF-8 surrogate */
    for (int i = 0; i < 8; ++i) {
        start();
        esp_websocket_event_data_t e = {.op_code = 8, .data_ptr = bytes,
            .data_len = 2, .payload_len = 2, .fin = true};
        switch (i) {
        case 0: e.data_len = e.payload_len = 1; break;
        case 1: e.fin = false; break;
        case 2: e.payload_offset = 1; break;
        case 3: e.payload_len = 3; break;
        case 4: e.data_len = e.payload_len = 126; break;
        case 5: e.data_ptr = NULL; break;
        case 6: e.data_len = -1; break;
        case 7: e.data_len = e.payload_len = sizeof(bytes); break;
        }
        ws_event(NULL, NULL, WEBSOCKET_EVENT_DATA, &e);
        close_event(1000);
        end(true, "invalid_close");
    }
    int invalid[] = {1, 999, 1004, 1005, 1006, 1015, 5000};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        start(); close_event(invalid[i]); end(true, "invalid_close");
    }
    start(); /* Two zero status bytes differ from an empty CLOSE body. */
    bytes[0] = bytes[1] = 0;
    esp_websocket_event_data_t e = {.op_code = 8, .data_ptr = bytes, .data_len = 2, .payload_len = 2, .fin = true};
    ws_event(NULL, NULL, WEBSOCKET_EVENT_DATA, &e);
    end(true, "invalid_close");
}
static void transport(void)
{
    for (int first = 0; first < 2; ++first) {
        start();
        if (first) transport_error();
        close_event(1000);
        if (!first) transport_error();
        end(true, "transport_error");
        assert(status.connection_tls_error == -0x7880 && status.connection_socket_errno == 104);
        assert(status.connection_error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT && status.connection_close_code == 1000);
    }
    start();
    esp_websocket_event_data_t e = {.error_handle = {.error_type = WEBSOCKET_ERROR_TYPE_HANDSHAKE,
        .esp_ws_handshake_status_code = 401, .esp_tls_stack_err = 1234, .esp_transport_sock_errno = 1234}};
    ws_event(NULL, NULL, WEBSOCKET_EVENT_ERROR, &e);
    end(true, "transport_error");
    assert(status.connection_http_status == 401 && !status.connection_tls_error && !status.connection_socket_errno);
}
static void timeout(void)
{
    start(); send_return = -1;
    assert(!send_packet("test", 4, false));
    close_event(1000); /* Later normal CLOSE cannot erase a send timeout. */
    end(true, "send_error");
    start(); send_return = 2;
    assert(!send_packet("test", 4, true)); end(true, "send_error");
    start(); ready = false; deadline = 5;
    end(true, "handshake_timeout");
    start(); failed = true; close_event(1000); end(true, "local_error");
}
static void rewake(void)
{
    start(); transport_error(); end(true, "transport_error");
    start();
    assert(status.connection_errors == 1 && status.connection_tls_error == -0x7880);
    assert(!strcmp(status.connection_end, "transport_error"));
    assert(send_packet("test", 4, true));
    finish_connection(100); assert(ws && stop_calls == 0 && !failed);
    close_event(1000); end(false, "transport_error");
    start();
    assert(status.normal_closes == 1 && status.connection_errors == 1);
    assert(send_packet("test", 4, false) && !failed && status.duplex_active);
    assert(!strcmp(status.connection_end, "transport_error"));
}
static void local_stop(void)
{
    start(); late_error = true;
    disconnect_session();
    assert(!ws && stop_calls == 1 && status.connection_errors == 1 && !status.normal_closes);
    assert(!strcmp(status.connection_end, "transport_error") && status.connection_tls_error == -0x7880);
    disconnect_session(); /* Cleanup without a live client cannot count twice. */
    assert(stop_calls == 1 && status.connection_errors == 1);
    start(); disconnect_session();
    assert(status.connection_errors == 1 && !status.normal_closes);
    assert(!strcmp(status.connection_end, "transport_error"));
    start(); close_event(1000); disconnect_session();
    assert(status.connection_errors == 1 && status.normal_closes == 1);
    assert(!strcmp(status.connection_end, "transport_error"));
}
static void dialog_close(void)
{
    start(); close_dialog("farewell");
    assert(!ws && !status.dialog_active && !status.connection_errors);
    assert(!strcmp(status.dialog_end_reason, "farewell") && !strcmp(status.state, "ready"));
    start(); late_error = true; close_dialog("silence");
    assert(!ws && !status.dialog_active && status.connection_errors == 1);
    assert(!strcmp(status.dialog_end_reason, "error") && !strcmp(status.state, "connection_error"));
    start(); close_dialog("manual");
    assert(!strcmp(status.dialog_end_reason, "manual") && !strcmp(status.state, "ready"));
    assert(status.connection_errors == 1 && !strcmp(status.connection_end, "transport_error"));
    start(); failed = true; close_dialog("farewell");
    assert(!strcmp(status.dialog_end_reason, "error") && !strcmp(status.state, "connection_error"));
    assert(status.connection_errors == 2 && !strcmp(status.connection_end, "local_error"));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "normal")) normal();
    else if (!strcmp(argv[1], "queued-close")) queued_close();
    else if (!strcmp(argv[1], "abnormal")) abnormal();
    else if (!strcmp(argv[1], "malformed")) malformed();
    else if (!strcmp(argv[1], "transport")) transport();
    else if (!strcmp(argv[1], "late-error")) { start(); close_event(1000); late_error = true; end(true, "transport_error"); }
    else if (!strcmp(argv[1], "timeout")) timeout();
    else if (!strcmp(argv[1], "unexpected-finish")) {
        start(); event(WEBSOCKET_EVENT_FINISH); end(true, "unexpected_disconnect");
        start(); event(WEBSOCKET_EVENT_DISCONNECTED); close_event(1000); end(true, "unexpected_disconnect");
    }
    else if (!strcmp(argv[1], "rewake")) rewake();
    else if (!strcmp(argv[1], "local-stop")) local_stop();
    else if (!strcmp(argv[1], "dialog-close")) dialog_close();
    else assert(false);
    assert(mutex_depth == 0);
    puts(argv[1]);
    return 0;
}
