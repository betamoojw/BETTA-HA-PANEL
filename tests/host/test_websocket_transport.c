/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Executes the fetched component's patched C functions with deterministic
 * transport/scheduler faults. The generated header is not a copy of the code. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
/* Expected negative controls must not open Windows crash-report dialogs. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
    exit(99); \
} } while (0)
static uint16_t ntohs(uint16_t value)
{
    const unsigned char *bytes = (const unsigned char *)&value;
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

#define ESP_OK 0
#define ESP_FAIL -1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGV(...) ((void)0)
#define WEBSOCKET_STATE_CONNECTED 1
#define WEBSOCKET_STATE_CLOSING 2
#define WEBSOCKET_STATE_UNKNOW 3
#define WEBSOCKET_STATE_WAIT_TIMEOUT 4
#define WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT 1
#define WEBSOCKET_EVENT_DATA 1
#define WS_TRANSPORT_OPCODES_NONE 0
#define WS_TRANSPORT_OPCODES_BINARY 2
#define WS_TRANSPORT_OPCODES_CLOSE 8
#define WS_TRANSPORT_OPCODES_PING 9
#define WS_TRANSPORT_OPCODES_PONG 10
#define WS_TRANSPORT_OPCODES_FIN 128
#define WEBSOCKET_TX_LOCK_TIMEOUT_MS 2000
typedef int esp_err_t;
typedef int ws_transport_opcodes_t;
typedef uint32_t TickType_t;
typedef struct { int owner, depth; } test_mutex;
typedef struct { int network_timeout_ms; } test_config;
typedef struct { int last_error, esp_tls_error_code, esp_tls_flags; } test_error;
typedef test_error *esp_tls_error_handle_t;
typedef struct client {
    test_mutex *lock, *tx_lock;
    test_config *config;
    void *transport;
    char tx_buffer[16], rx_buffer[16];
    int state, buffer_size, payload_offset, payload_len, last_opcode, close_status_code;
    bool last_fin, wait_for_pong_resp, run;
} *esp_websocket_client_handle_t;

static struct client client;
static test_mutex rx_mutex, tx_mutex;
static test_config config;
static int actor, close_on_lock, blocked_lock, read_count, write_count, events, aborts, yields;
static int readable, opcode, payload, read_values[8], read_times[8], read_items, send_result;
static bool tls_busy, inject_sender, fail_write;
static uint64_t now_ms;
static int esp_websocket_client_send_with_exact_opcode(esp_websocket_client_handle_t,
    ws_transport_opcodes_t, const uint8_t *, int, TickType_t);

static int xSemaphoreTakeRecursive(test_mutex *mutex, TickType_t timeout)
{
    if (blocked_lock || (mutex->depth && mutex->owner != actor)) {
        assert(timeout != portMAX_DELAY);
        now_ms += timeout;
        return 0;
    }
    mutex->owner = actor;
    ++mutex->depth;
    if (close_on_lock) {
        client.state = WEBSOCKET_STATE_UNKNOW;
        close_on_lock = 0;
    }
    return pdPASS;
}
static void xSemaphoreGiveRecursive(test_mutex *mutex)
{
    assert(mutex->owner == actor && mutex->depth > 0);
    if (!--mutex->depth) mutex->owner = 0;
}
static void vTaskDelay(TickType_t ticks)
{
    assert(rx_mutex.depth == 0 && tx_mutex.depth == 0 && ticks == 1);
    ++yields;
    now_ms += ticks;
}
static uint64_t _tick_get_ms(void) { return now_ms; }
static int esp_websocket_client_is_connected(esp_websocket_client_handle_t c)
{ return c->state == WEBSOCKET_STATE_CONNECTED; }
static int esp_websocket_new_buf(esp_websocket_client_handle_t c, bool tx)
{ (void)c; (void)tx; return ESP_OK; }
static void esp_websocket_free_buf(esp_websocket_client_handle_t c, bool tx)
{ (void)c; (void)tx; }
static const char *esp_err_to_name(int error) { (void)error; return "test"; }
static esp_tls_error_handle_t esp_transport_get_error_handle(void *t) { (void)t; return NULL; }
static int esp_websocket_client_error(esp_websocket_client_handle_t c, const char *fmt, ...)
{ (void)c; (void)fmt; return ESP_OK; }
static int esp_websocket_client_abort_connection(esp_websocket_client_handle_t c, int error)
{
    (void)error;
    assert(c->lock->owner == actor && c->lock->depth > 0);
    assert(!tls_busy);
    c->state = WEBSOCKET_STATE_UNKNOW;
    c->run = false;
    ++aborts;
    return ESP_OK;
}
static int esp_transport_ws_send_raw(void *t, int op, const void *data, int len, int timeout)
{
    (void)op; (void)data;
    assert(t && timeout > 0 && client.state == WEBSOCKET_STATE_CONNECTED);
    assert(!tls_busy); /* Regression: RX temporarily has no TLS counter/buffer. */
    ++write_count;
    return fail_write ? -1 : len;
}
static int esp_transport_poll_read(void *t, int timeout)
{
    assert(t && rx_mutex.owner == actor && rx_mutex.depth > 0);
    assert(timeout > 0 && timeout <= 20);
    if (readable == 0) now_ms += timeout;
    return readable;
}
static int esp_transport_read(void *t, void *data, int len, int timeout)
{
    (void)data; (void)len;
    assert(t && rx_mutex.owner == actor && rx_mutex.depth > 0);
    assert(timeout > 0 && timeout <= config.network_timeout_ms);
    assert(read_count < read_items); /* Detect infinite zero-progress retries. */
    tls_busy = true;
    if (inject_sender) {
        actor = 2;
        send_result = esp_websocket_client_send_with_exact_opcode(&client,
            WS_TRANSPORT_OPCODES_BINARY | WS_TRANSPORT_OPCODES_FIN,
            (const uint8_t *)"voice", 5, 500);
        actor = 1;
        inject_sender = false;
    }
    now_ms += read_times[read_count];
    tls_busy = false;
    return read_values[read_count++];
}
static int esp_transport_ws_get_read_payload_len(void *t) { (void)t; return payload; }
static int esp_transport_ws_get_fin_flag(void *t) { (void)t; return 1; }
static int esp_transport_ws_get_read_opcode(void *t) { (void)t; return opcode; }
static void esp_websocket_client_dispatch_event(esp_websocket_client_handle_t c, int event,
                                               const void *data, int size)
{ (void)c; (void)event; (void)data; (void)size; ++events; }

#include "websocket_functions.h"

static void reset(void)
{
    memset(&client, 0, sizeof(client));
    memset(&rx_mutex, 0, sizeof(rx_mutex));
    memset(&tx_mutex, 0, sizeof(tx_mutex));
    memset(read_values, 0, sizeof(read_values));
    memset(read_times, 0, sizeof(read_times));
    config.network_timeout_ms = 5000;
    client.lock = &rx_mutex; client.tx_lock = &tx_mutex; client.config = &config;
    client.transport = &client; client.state = WEBSOCKET_STATE_CONNECTED;
    client.buffer_size = 16; client.run = true;
    actor = 1; close_on_lock = blocked_lock = read_count = write_count = events = aborts = yields = 0;
    readable = 1; opcode = WS_TRANSPORT_OPCODES_BINARY; payload = 5;
    read_items = 1; read_values[0] = 5;
    now_ms = 0; tls_busy = inject_sender = fail_write = false;
}
static int send_voice(void)
{
    return esp_websocket_client_send_with_exact_opcode(&client, 130,
        (const uint8_t *)"voice", 5, 500);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    reset();
    if (!strcmp(argv[1], "concurrent")) {
        inject_sender = true;
        esp_websocket_client_poll_connection(&client);
        assert(send_result == -1 && write_count == 0 && aborts == 0);
        assert(send_voice() == 5 && write_count == 1);
    } else if (!strcmp(argv[1], "closed_while_waiting")) {
        close_on_lock = 1;
        assert(send_voice() == -1 && write_count == 0);
    } else if (!strcmp(argv[1], "send_timeout")) {
        blocked_lock = 1;
        assert(send_voice() == -1 && write_count == 0 && now_ms == 500);
        blocked_lock = 0;
        assert(send_voice() == 5);
    } else if (!strcmp(argv[1], "write_failure")) {
        fail_write = true;
        assert(send_voice() == -1 && aborts == 1);
        assert(send_voice() == -1 && write_count == 1);
    } else if (!strcmp(argv[1], "partial_timeout")) {
        read_items = 2; read_values[0] = 2; read_values[1] = 0;
        read_times[1] = 5000;
        esp_websocket_client_poll_connection(&client);
        assert(read_count == 2 && events == 1 && aborts == 1);
    } else if (!strcmp(argv[1], "trickle_deadline")) {
        payload = 100; read_items = 5;
        for (int i = 0; i < 5; ++i) { read_values[i] = 1; read_times[i] = 1000; }
        esp_websocket_client_poll_connection(&client);
        assert(read_count == 5 && aborts == 1);
    } else if (!strcmp(argv[1], "stopped_receive")) {
        client.run = false;
        esp_websocket_client_poll_connection(&client);
        assert(read_count == 0 && aborts == 1);
    } else if (!strcmp(argv[1], "empty_control")) {
        payload = 0; opcode = WS_TRANSPORT_OPCODES_CLOSE; read_values[0] = 0;
        esp_websocket_client_poll_connection(&client);
        assert(events == 1 && aborts == 0 && client.state == WEBSOCKET_STATE_CLOSING);
        assert(send_voice() == -1 && write_count == 0);
    } else if (!strcmp(argv[1], "poll_recovery")) {
        readable = -1;
        esp_websocket_client_poll_connection(&client);
        assert(aborts == 1 && read_count == 0);
        reset(); /* A newly initialized connection can send and receive again. */
        esp_websocket_client_poll_connection(&client);
        assert(events == 1 && send_voice() == 5);
    } else if (!strcmp(argv[1], "idle_and_duplex")) {
        readable = 0;
        esp_websocket_client_poll_connection(&client);
        assert(now_ms <= 21 && send_voice() == 5);
        readable = 1;
        for (int i = 0; i < 20; ++i) {
            read_count = 0;
            esp_websocket_client_poll_connection(&client);
            assert(send_voice() == 5);
        }
        assert(events == 20 && write_count == 21 && yields == 21 && aborts == 0);
    } else if (!strcmp(argv[1], "fragmented")) {
        read_items = 3; read_values[0] = 2; read_values[1] = 2; read_values[2] = 1;
        read_times[0] = read_times[1] = read_times[2] = 100;
        esp_websocket_client_poll_connection(&client);
        assert(read_count == 3 && events == 3 && aborts == 0 && send_voice() == 5);
    } else {
        return 2;
    }
    assert(rx_mutex.depth == 0 && tx_mutex.depth == 0 && !tls_busy);
    assert(_tick_get_ms() == now_ms);
    puts(argv[1]);
    return 0;
}
