# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Serialized TLS transport fixes for the pinned ESP WebSocket client.

Keep these edits reproducible through apply_component_patches.py; never make
managed_components the source of truth. Unknown upstream text fails closed.
"""

POLL_HELPER = '''/* andre68723/BETTA-HA-PANEL: polling also accesses the TLS context.
 * Keep it under the same lock as read/write/close, with short idle waits so
 * microphone uploads are not delayed by the upstream one-second poll. */
static void esp_websocket_client_poll_connection(esp_websocket_client_handle_t client)
{
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (client->state == WEBSOCKET_STATE_CONNECTED && client->transport != NULL) {
        int readable = esp_transport_poll_read(client->transport, 20);
        if (readable < 0 || (readable > 0 && esp_websocket_client_recv(client) == ESP_FAIL)) {
            esp_websocket_client_error(client, "WebSocket receive failed or timed out");
            esp_websocket_client_abort_connection(client, WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT);
        }
    }
    xSemaphoreGiveRecursive(client->lock);
    /* Let a waiting sender run even when receive data is continuously ready. */
    vTaskDelay(1);
}

'''

OLD_POLL = '''            read_select = esp_transport_poll_read(client->transport, 1000); //Poll every 1000ms
            if (read_select < 0) {
                xSemaphoreTakeRecursive(client->lock, lock_timeout);
                esp_tls_error_handle_t error_handle = esp_transport_get_error_handle(client->transport);
                if (error_handle) {
                    const char *error_name = esp_err_to_name(error_handle->last_error);
                    esp_websocket_client_error(client, "esp_transport_poll_read() returned %d, transport_error=%s, tls_error_code=%i, tls_flags=%i, errno=%d",
                                               read_select, error_name, error_handle->esp_tls_error_code,
                                               error_handle->esp_tls_flags, errno);
                } else {
                    esp_websocket_client_error(client, "esp_transport_poll_read() returned %d, errno=%d", read_select, errno);
                }
                esp_websocket_client_abort_connection(client, WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT);
                xSemaphoreGiveRecursive(client->lock);
            } else if (read_select > 0) {
                xSemaphoreTakeRecursive(client->lock, lock_timeout);
                if (esp_websocket_client_recv(client) == ESP_FAIL) {
                    ESP_LOGE(TAG, "Error receive data");
                    esp_websocket_client_abort_connection(client, WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT);
                }
                xSemaphoreGiveRecursive(client->lock);
            } else {
                ESP_LOGV(TAG, "Read poll timeout: skipping esp_transport_poll_read().");
            }
'''

BASE_EDITS = [
    ('    if (esp_websocket_new_buf(client, true) != ESP_OK) {',
     '''    /* andre68723/BETTA-HA-PANEL: the connection may close while send
     * waits for the receive/transport lock. Recheck before touching TLS. */
    if (!esp_websocket_client_is_connected(client) || client->transport == NULL) {
        goto unlock_and_return;
    }

    if (esp_websocket_new_buf(client, true) != ESP_OK) {'''),
    ('''    do {
        rlen = esp_transport_read(client->transport, client->rx_buffer, client->buffer_size, client->config->network_timeout_ms);''',
     '''    /* andre68723/BETTA-HA-PANEL: a stalled or trickling frame must not
     * hold the shared TLS lock indefinitely and prevent stop/recovery. */
    uint64_t receive_started = _tick_get_ms();
    do {
        uint64_t elapsed = _tick_get_ms() - receive_started;
        if (!client->run || elapsed >= (uint64_t)client->config->network_timeout_ms) {
            esp_websocket_free_buf(client, false);
            return ESP_FAIL;
        }
        int remaining_ms = client->config->network_timeout_ms - (int)elapsed;
        rlen = esp_transport_read(client->transport, client->rx_buffer, client->buffer_size, remaining_ms);'''),
    ('''        if (rlen == 0 && client->last_opcode == WS_TRANSPORT_OPCODES_NONE) {''',
     '''        /* andre68723/BETTA-HA-PANEL: zero bytes during a nonempty
         * frame is a timeout, not another empty fragment to retry forever. */
        if (rlen == 0 && client->payload_offset < client->payload_len) {
            esp_websocket_free_buf(client, false);
            return ESP_FAIL;
        }
        if (rlen == 0 && client->last_opcode == WS_TRANSPORT_OPCODES_NONE) {'''),
    ('static int esp_websocket_client_send_close(esp_websocket_client_handle_t client, int code, const char *additional_data, int total_len, TickType_t timeout);',
     POLL_HELPER + 'static int esp_websocket_client_send_close(esp_websocket_client_handle_t client, int code, const char *additional_data, int total_len, TickType_t timeout);'),
    ('    int read_select = 0;\n', ''),
    (OLD_POLL, '            esp_websocket_client_poll_connection(client);\n'),
    ('''    esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_FINISH, NULL, 0);
    esp_transport_close(client->transport);
    client->state = WEBSOCKET_STATE_UNKNOW;''',
     '''    esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_FINISH, NULL, 0);
    /* andre68723/BETTA-HA-PANEL: serialize final close with queued sends. */
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    esp_transport_close(client->transport);
    client->state = WEBSOCKET_STATE_UNKNOW;
    xSemaphoreGiveRecursive(client->lock);'''),
]

SEND_SIGNATURE = 'static int esp_websocket_client_send_with_exact_opcode(esp_websocket_client_handle_t client, ws_transport_opcodes_t opcode, const uint8_t *data, int len, TickType_t timeout)'
INIT_SIGNATURE = 'esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *config)'
SEND_WRAPPERS = '''/* Preserve the upstream API's timeout and error contract. */
static int esp_websocket_client_send_with_exact_opcode(esp_websocket_client_handle_t client, ws_transport_opcodes_t opcode, const uint8_t *data, int len, TickType_t timeout)
{
    int result = esp_websocket_client_send_with_timeouts(client, opcode, data, len, timeout, timeout);
    return result == -ESP_ERR_TIMEOUT ? -1 : result;
}

/* andre68723/BETTA-HA-PANEL: busy is reported only before any frame write.
 * The application drains incoming audio before retrying, without relaxing TLS
 * serialization or replaying a partial/failed transport write. */
int betta_websocket_client_send(esp_websocket_client_handle_t client, const void *data,
    int length, bool binary, TickType_t lock_wait, TickType_t write_wait)
{
    ws_transport_opcodes_t opcode = (binary ? WS_TRANSPORT_OPCODES_BINARY : WS_TRANSPORT_OPCODES_TEXT) | WS_TRANSPORT_OPCODES_FIN;
    return esp_websocket_client_send_with_timeouts(client, opcode, data, length, write_wait, lock_wait);
}

'''
TX_EDITS = [
    (SEND_SIGNATURE, SEND_SIGNATURE.replace('send_with_exact_opcode', 'send_with_timeouts').replace(
        'TickType_t timeout)', 'TickType_t timeout, TickType_t lock_timeout)')),
]
for mutex in ('tx_lock', 'lock'):
    TX_EDITS.append((
        f'''    if (xSemaphoreTakeRecursive(client->{mutex}, timeout) != pdPASS) {{
        ESP_LOGE(TAG, "Could not lock ws-client within %" PRIu32 " timeout", timeout);
        return -1;
    }}''',
        f'''    if (xSemaphoreTakeRecursive(client->{mutex}, lock_timeout) != pdPASS) {{
        return -ESP_ERR_TIMEOUT; /* No TLS/frame bytes have been written. */
    }}'''))
TX_EDITS.append((INIT_SIGNATURE, SEND_WRAPPERS + INIT_SIGNATURE))
WRITE_EDITS = [
    ('''        if (wlen < 0 || (wlen == 0 && need_write != 0)) {
            ret = wlen;''',
     '''        /* A short payload leaves the current frame incomplete. A new
         * continuation frame cannot repair it; close without replaying bytes.
         * Reserve -ESP_ERR_TIMEOUT exclusively for the pre-write lock above. */
        if (wlen != need_write) {
            ret = -1;'''),
]
EDITS = BASE_EDITS + TX_EDITS + WRITE_EDITS


def patch_websocket_transport(path):
    text = path.read_text(encoding="utf-8")
    if SEND_WRAPPERS in text:
        if not all(new in text for old, new in BASE_EDITS + TX_EDITS if new):
            return "missing", None
        if all(new in text for old, new in WRITE_EDITS):
            return "present", None
        edits = WRITE_EDITS
    elif BASE_EDITS[-1][1] in text:
        if not all(new in text for old, new in BASE_EDITS if new):
            return "missing", None
        edits = TX_EDITS + WRITE_EDITS
    else:
        edits = EDITS
    # Upgrade only complete known generations; partial/unknown text fails closed.
    for old, new in edits:
        if text.count(old) != 1:
            return "missing", None
        text = text.replace(old, new, 1)
    return "applied", text
