/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Protocol reference: kruzio1985/BETTA-HA-PANEL-10-Guiton-JC8012P4A1C,
 * commit 7668ebe60ed736c838e0ea5b565a5e77df0b0b2f, main/xiaozhi/ (Cpt_Kirk,
 * ForgeUI-derived implementation). Reimplemented with a single session owner.
 */
#include "xiaozhi.h"
#include "knx/knx_mcp.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "util/flash_gate.h"

#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
#ifdef CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK
#error "Xiaozhi TLS requires the shared WebSocket lock; regenerate sdkconfig from project defaults"
#endif
#include "xiaozhi_audio.h"
#include "xiaozhi_duplex.h"
#include "xiaozhi_session.h"
#include "xiaozhi_connection.h"
#include "xiaozhi_wake.h"
#include "xiaozhi_wake_words.h"
#include "xiaozhi_custom_model_store.h"
#include "xiaozhi_pcm.h"
#include "xiaozhi_dialog.h"
#include "xiaozhi_activity.h"
#include "xiaozhi_farewell.h"
#include "esp_vad.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "cJSON.h"
#include "opus.h"

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define MAX_FRAME 8192
#define FRAME_SAMPLES 960
#define NS "xiaozhi"
typedef struct { char action[16]; int value; uint32_t stop_revision; char phrase[XIAOZHI_WAKE_PHRASE_SIZE]; } command_t;
typedef struct { int opcode, len; uint8_t *data; } packet_t;
static SemaphoreHandle_t lock;
static QueueHandle_t commands, packets;
static TaskHandle_t worker_task;
static atomic_bool worker_started;
static portMUX_TYPE pending_lock = portMUX_INITIALIZER_UNLOCKED;
static command_t pending_end[2]; /* Dismiss and release retain separate bounded slots. */
static bool pending_end_valid[2], stop_generation_valid;
static uint32_t stop_generation, stop_revision;
static xiaozhi_status_t status = {.supported = true, .duplex_enabled = true, .volume = 50, .dialog_timeout = 30, .wake_config = {.phrase = XIAOZHI_WAKE_DEFAULT_PHRASE}};
static char endpoint[256], token[1024], device[18], uuid[37], session[128];
static esp_websocket_client_handle_t ws;
static bool failed; /* Worker-owned audio/protocol failures. */
static xiaozhi_connection_t connection; /* Protected by lock. */
static uint8_t *rx;
static size_t rx_len;
static int rx_opcode;
static OpusEncoder *encoder;
static OpusDecoder *decoder;
static int16_t *pcm;
static uint8_t *encoded;
static bool hello_sent, ready, want_listen;
static int64_t deadline, listen_deadline, pair_deadline, next_pair;
static bool pairing, identity_ready, auto_listen, idle_capture;
static int64_t wake_retry;
static int16_t *wake_pcm;
static int64_t wake_last_loud;
static xiaozhi_session_t stream;
static xiaozhi_duplex_frame_t *duplex_frame;
static bool audio_stopping;
static vad_handle_t manual_vad;

static xiaozhi_connection_t connection_snapshot(void)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    xiaozhi_connection_t value = connection;
    xSemaphoreGive(lock);
    return value;
}
static void connection_fail(xiaozhi_end_t reason)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    xiaozhi_connection_fail(&connection, reason);
    xSemaphoreGive(lock);
}
static bool connection_can_send(void)
{
    xiaozhi_connection_t value = connection_snapshot();
    return xiaozhi_connection_can_send(&value);
}
static void connection_reset(void)
{
    xSemaphoreTake(lock, portMAX_DELAY); connection = (xiaozhi_connection_t){0}; xSemaphoreGive(lock);
    failed = false; rx_len = 0; rx_opcode = 0;
}
static bool send_packet(const void *data, int length, bool binary)
{
    if (!ws || !connection_can_send()) return false;
    int sent = binary ? esp_websocket_client_send_bin(ws, data, length, pdMS_TO_TICKS(500)) :
        esp_websocket_client_send_text(ws, data, length, pdMS_TO_TICKS(500));
    xSemaphoreTake(lock, portMAX_DELAY);
    xiaozhi_connection_send_result(&connection, sent, length);
    xSemaphoreGive(lock);
    return sent == length;
}

static void record_connection_end(void)
{
    xiaozhi_connection_t link = connection_snapshot();
    if (link.end == XZ_END_NONE) return; /* An intentional local stop has no peer diagnosis. */
    bool error = xiaozhi_connection_error(&link);
    xSemaphoreTake(lock, portMAX_DELAY);
    /* A later normal exit must not erase the last real failure diagnosis. */
    if (error || !status.connection_errors) {
        status.connection_close_code = link.close_code;
        status.connection_error_type = link.error_type; status.connection_tls_error = link.tls_error;
        status.connection_socket_errno = link.socket_errno; status.connection_http_status = link.http_status;
        strlcpy(status.connection_end, xiaozhi_connection_reason(&link), sizeof(status.connection_end));
    }
    if (error) ++status.connection_errors; else ++status.normal_closes;
    xSemaphoreGive(lock);
    ESP_LOGI("xiaozhi", "Connection ended: %s code=%d type=%d tls=%d socket=%d http=%d",
             xiaozhi_connection_reason(&link), link.close_code, link.error_type,
             link.tls_error, link.socket_errno, link.http_status);
}

static void update_audio_status(void)
{
    xiaozhi_duplex_stats_t d;
    xiaozhi_duplex_stats(&d);
    if (d.failed && stream.realtime) failed = true;
    xSemaphoreTake(lock, portMAX_DELAY);
    status.duplex_active = stream.realtime && d.running && !audio_stopping;
    status.speaking = stream.tts || d.playing;
    status.aec_frames = d.captured_frames;
    status.aec_playback_frames = d.captured_during_playback;
    status.audio_capture_drops = d.dropped_capture_frames;
    status.aec_mic_peak = d.mic_peak; status.aec_ref_peak = d.ref_peak;
    status.aec_ref_peak_max_playback = d.ref_peak_max_playback;
    status.aec_clean_peak = d.clean_peak; status.aec_processing_us = d.processing_us;
    status.capture_stack_free = d.capture_stack_free; status.playback_stack_free = d.playback_stack_free;
    if (status.duplex_active && status.listening)
        strlcpy(status.state, status.speaking ? "speaking_listening" : "listening", sizeof(status.state));
    xSemaphoreGive(lock);
}

static void show_dialog(bool voice_opened)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    ++status.dialog_seq; status.dialog_active = true; status.dialog_voice_opened = voice_opened;
    status.text[0] = status.recognized[0] = status.reply[0] = status.emotion[0] = 0;
    strlcpy(status.state, "ready", sizeof(status.state));
    xiaozhi_activity_begin(status.dialog_seq);
    xSemaphoreGive(lock);
}
static void end_dialog(const char *reason)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    xiaozhi_activity_end(status.dialog_seq);
    if (status.dialog_active) {
        status.dialog_active = false; status.dialog_end_seq = status.dialog_seq;
        strlcpy(status.dialog_end_reason, reason, sizeof(status.dialog_end_reason));
    }
    xSemaphoreGive(lock);
}
static void conversation(bool active)
{
    xSemaphoreTake(lock, portMAX_DELAY); status.conversation = active; xSemaphoreGive(lock);
}

static void set_state(const char *value)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    strlcpy(status.state, value, sizeof(status.state));
    xSemaphoreGive(lock);
}

static void status_spectrum(xiaozhi_status_t *out)
{
    out->activity_seq = xiaozhi_activity_revision();
    out->silence_remaining_ms = out->dialog_active ? xiaozhi_activity_remaining_ms(out->dialog_seq) : 0;
    xiaozhi_spectrum_snapshot_t spectrum;
    xiaozhi_spectrum_snapshot(&spectrum);
    memcpy(out->spectrum_mic, spectrum.microphone, sizeof(out->spectrum_mic));
    memcpy(out->spectrum_speaker, spectrum.speaker, sizeof(out->spectrum_speaker));
    out->spectrum_mic_active = spectrum.mic_active;
    out->spectrum_speaker_active = spectrum.speaker_active;
}
void xiaozhi_status(xiaozhi_status_t *out)
{
    if (!lock) { memset(out, 0, sizeof(*out)); out->supported = true; return; }
    xSemaphoreTake(lock, portMAX_DELAY); *out = status; xSemaphoreGive(lock);
    out->wake_model_ready = xiaozhi_custom_model_available();
    status_spectrum(out);
}
bool xiaozhi_status_try(xiaozhi_status_t *out)
{
    if (!out) return false;
    if (!lock) { memset(out, 0, sizeof(*out)); out->supported = true; return true; }
    if (xSemaphoreTake(lock, 0) != pdTRUE) return false;
    *out = status;
    xSemaphoreGive(lock);
    status_spectrum(out);
    return true;
}

static bool save_enabled(bool enabled)
{
    nvs_handle_t h;
    if (!flash_gate_enter(1000)) return false;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) { flash_gate_leave(); return false; }
    esp_err_t err = nvs_set_u8(h, "enabled", enabled);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    flash_gate_leave();
    return err == ESP_OK;
}

esp_err_t xiaozhi_set_emoji_style(int value)
{
    if (value < 0 || value > 1 || !lock) return ESP_ERR_INVALID_ARG;
    if (!flash_gate_enter(1000)) return ESP_ERR_TIMEOUT;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "emoji", value);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    flash_gate_leave();
    if (err == ESP_OK) {
        xSemaphoreTake(lock, portMAX_DELAY); status.emoji_style = value; xSemaphoreGive(lock);
    }
    return err;
}

static void receive_mcp(const cJSON *message);

static void drain_packets(bool preserve_mcp)
{
    packet_t p;
    /* Bound the purge to queued frames. A cloud handshake can arrive next to
     * hello or TTS/stop; dropping initialize/tools/list loses tool discovery. */
    UBaseType_t pending = uxQueueMessagesWaiting(packets);
    while (pending-- && xQueueReceive(packets, &p, 0) == pdTRUE) {
        if (preserve_mcp && p.opcode == 1) {
            cJSON *message = cJSON_ParseWithLength((char *)p.data, p.len);
            if (message) { receive_mcp(message); cJSON_Delete(message); }
        }
        free(p.data);
    }
}

static void disconnect_session(void)
{
    bool had_connection = ws != NULL;
    xiaozhi_activity_end(status.dialog_seq);
    want_listen = auto_listen = idle_capture = false;
    conversation(false);
    knx_mcp_session_reset();
    wake_retry = esp_timer_get_time() + 1500000;
    xiaozhi_wake_reset();
    /* Never close a codec or free a buffer still owned by an audio task. A
     * bounded join failure quarantines audio until a later join succeeds. */
    audio_stopping = xiaozhi_duplex_stop() != ESP_OK;
    if (!audio_stopping) xiaozhi_audio_close();
    xiaozhi_session_reset(&stream, false);
    xSemaphoreTake(lock, portMAX_DELAY); connection.stopping = true; xSemaphoreGive(lock);
    if (ws) { esp_websocket_client_stop(ws); esp_websocket_client_destroy(ws); ws = NULL; }
    drain_packets(false);
    free(rx); rx = NULL;
    free(encoder); encoder = NULL;
    free(decoder); decoder = NULL;
    free(pcm); pcm = NULL;
    free(encoded); encoded = NULL;
    free(duplex_frame); duplex_frame = NULL;
    if (manual_vad) vad_destroy(manual_vad);
    manual_vad = NULL;
    ready = hello_sent = false;
    session[0] = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    status.listening = status.wake_ready = status.wake_running = false;
    status.duplex_active = status.speaking = false;
    xSemaphoreGive(lock);
    update_audio_status();
    /* Stop/destroy joined every callback, including errors racing a local stop. */
    if (had_connection) record_connection_end();
}

static void close_dialog(const char *reason)
{
    bool had_connection = ws != NULL;
    if (had_connection && failed) connection_fail(XZ_END_LOCAL);
    disconnect_session();
    xiaozhi_connection_t link = connection_snapshot();
    bool error = had_connection && xiaozhi_connection_error(&link);
    end_dialog(error ? "error" : reason);
    set_state(error ? "connection_error" : status.enabled ? "ready" : "disabled");
}

/* Event task only assembles bounded messages. The worker owns audio and JSON. */
static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    esp_websocket_event_data_t *e = data;
    if (id != WEBSOCKET_EVENT_DATA || (e && e->op_code == 8)) {
        xSemaphoreTake(lock, portMAX_DELAY);
        if (id == WEBSOCKET_EVENT_CONNECTED) connection.connected = true;
        else if (id == WEBSOCKET_EVENT_ERROR) {
            xiaozhi_connection_fail(&connection, XZ_END_TRANSPORT);
            if (e) {
                connection.error_type = e->error_handle.error_type;
                connection.http_status = e->error_handle.esp_ws_handshake_status_code;
                if (connection.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                    connection.tls_error = e->error_handle.esp_tls_stack_err;
                    connection.socket_errno = e->error_handle.esp_transport_sock_errno;
                }
            }
        } else if (id == WEBSOCKET_EVENT_DISCONNECTED)
            xiaozhi_connection_fail(&connection, XZ_END_DISCONNECTED);
        else if (id == WEBSOCKET_EVENT_FINISH) xiaozhi_connection_finished(&connection);
        else if (id == WEBSOCKET_EVENT_CLOSED) xiaozhi_connection_close_code(&connection, e ? e->close_status_code : -1);
        /* DATA precedes the SDK updating close_status_code; parse the wire payload. */
        else if (id == WEBSOCKET_EVENT_DATA)
            xiaozhi_connection_close_frame(&connection, e->data_ptr, e->data_len, e->payload_len, e->payload_offset, e->fin);
        xSemaphoreGive(lock);
        return;
    }
    if (!e) { connection_fail(XZ_END_LOCAL); return; }
    if (e->op_code >= 8) return;
    if (e->data_len < 0 || e->payload_offset < 0 || e->payload_len < 0 ||
        e->payload_len > MAX_FRAME || rx_len + (size_t)e->data_len > MAX_FRAME) {
        connection_fail(XZ_END_LOCAL); return;
    }
    if (e->payload_offset == 0 && (e->op_code == 1 || e->op_code == 2)) {
        rx_len = 0; rx_opcode = e->op_code;
    }
    if (!rx || (rx_opcode != 1 && rx_opcode != 2)) { connection_fail(XZ_END_LOCAL); return; }
    memcpy(rx + rx_len, e->data_ptr, e->data_len); rx_len += e->data_len;
    if (!e->fin || e->payload_offset + e->data_len != e->payload_len) return;
    packet_t p = {.opcode = rx_opcode, .len = rx_len};
    p.data = heap_caps_malloc(rx_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p.data) { connection_fail(XZ_END_LOCAL); return; }
    memcpy(p.data, rx, rx_len); p.data[rx_len] = 0; rx_len = 0; rx_opcode = 0;
    if (xQueueSend(packets, &p, 0) != pdTRUE) { free(p.data); connection_fail(XZ_END_LOCAL); }
}

static bool send_text(const char *text)
{
    return send_packet(text, (int)strlen(text), false);
}

static bool control(const char *type, const char *state)
{
    cJSON *j = cJSON_CreateObject();
    if (!j) { failed = true; return false; }
    cJSON_AddStringToObject(j, "session_id", session);
    cJSON_AddStringToObject(j, "type", type);
    if (state) { cJSON_AddStringToObject(j, "state", state); cJSON_AddStringToObject(j, "mode", stream.realtime ? "realtime" : auto_listen ? "auto" : "manual"); }
    char *s = cJSON_PrintUnformatted(j); cJSON_Delete(j);
    if (!s) failed = true;
    bool ok = s && send_text(s); free(s); return ok;
}

static bool connect_session(void)
{
    if (audio_stopping) { end_dialog("error"); set_state("audio_or_connection_error"); return false; }
    /* Discard the idle capture stream before the cloud handshake. Resume only
     * on listen/start, so buffered wake audio is not treated as the question. */
    if (idle_capture && xiaozhi_audio_mic(false) != ESP_OK) goto error;
    idle_capture = false;
    if (!identity_ready || strncmp(endpoint, "wss://", 6) || !token[0]) {
        end_dialog("error"); set_state("audio_or_connection_error"); return false;
    }
    rx = heap_caps_malloc(MAX_FRAME, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    pcm = heap_caps_malloc(3840 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    encoded = heap_caps_malloc(1504, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    encoder = heap_caps_malloc(opus_encoder_get_size(1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    decoder = heap_caps_malloc(opus_decoder_get_size(1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rx || !pcm || !encoded || !encoder || !decoder) goto error;
    if (opus_encoder_init(encoder, 16000, 1, OPUS_APPLICATION_VOIP) != OPUS_OK ||
        opus_decoder_init(decoder, 16000, 1) != OPUS_OK) goto error;
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(1));
    if (xiaozhi_audio_open(status.volume) != ESP_OK) goto error;
    esp_websocket_client_config_t cfg = {.uri = endpoint, .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_reconnect = true, .network_timeout_ms = 5000, .task_stack = 8192, .buffer_size = 2048};
    ws = esp_websocket_client_init(&cfg);
    if (!ws) goto error;
    char auth[1040]; snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_websocket_client_append_header(ws, "Authorization", auth);
    esp_websocket_client_append_header(ws, "Protocol-Version", "3");
    esp_websocket_client_append_header(ws, "Device-Id", device);
    esp_websocket_client_append_header(ws, "Client-Id", uuid);
    memset(auth, 0, sizeof(auth));
    esp_websocket_register_events(ws, WEBSOCKET_EVENT_ANY, ws_event, NULL);
    connection_reset();
    deadline = esp_timer_get_time() + 15000000;
    if (esp_websocket_client_start(ws) != ESP_OK) goto error;
    set_state("connecting"); return true;
error:
    disconnect_session(); end_dialog("error"); set_state("audio_or_connection_error"); return false;
}

typedef struct { char *data; size_t len; bool overflow; } response_t;
static esp_err_t http_event(esp_http_client_event_t *e)
{
    response_t *r = e->user_data;
    if (e->event_id != HTTP_EVENT_ON_DATA || e->data_len <= 0) return ESP_OK;
    if (r->len + e->data_len >= MAX_FRAME) { r->overflow = true; return ESP_ERR_INVALID_SIZE; }
    memcpy(r->data + r->len, e->data, e->data_len); r->len += e->data_len;
    r->data[r->len] = 0; return ESP_OK;
}

static cJSON *cloud_request(bool activate)
{
    response_t r = {.data = heap_caps_calloc(1, MAX_FRAME, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)};
    if (!r.data) return NULL;
    esp_http_client_config_t cfg = {.url = activate ? OTA_URL "activate" : OTA_URL,
        .method = HTTP_METHOD_POST, .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 5000, .disable_auto_redirect = true, .event_handler = http_event, .user_data = &r};
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) { free(r.data); return NULL; }
    esp_http_client_set_header(h, "Device-Id", device);
    esp_http_client_set_header(h, "Client-Id", uuid);
    esp_http_client_set_header(h, "Activation-Version", "1");
    esp_http_client_set_header(h, "Content-Type", "application/json");
    char body[512];
    snprintf(body, sizeof(body), "{\"version\":2,\"language\":\"en\",\"mac_address\":\"%s\",\"uuid\":\"%s\",\"chip_model_name\":\"esp32p4\",\"application\":{\"name\":\"betta-ha-panel-10.1\",\"version\":\"%s\"}}", device, uuid, esp_app_get_description()->version);
    const char *payload = activate ? "{}" : body;
    esp_http_client_set_post_field(h, payload, strlen(payload));
    esp_err_t err = esp_http_client_perform(h);
    int code = esp_http_client_get_status_code(h);
    esp_http_client_cleanup(h);
    cJSON *j = err == ESP_OK && code == 200 && !r.overflow ? cJSON_Parse(r.data) : NULL;
    free(r.data); return j;
}

static const char *str(cJSON *j, const char *key)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static void pair_step(void)
{
    cJSON *j = cloud_request(false);
    if (!j) { set_state("pairing_retry"); return; }
    cJSON *a = cJSON_GetObjectItemCaseSensitive(j, "activation");
    if (cJSON_IsObject(a)) {
        xSemaphoreTake(lock, portMAX_DELAY);
        strlcpy(status.code, str(a, "code"), sizeof(status.code));
        xSemaphoreGive(lock);
        set_state("pairing");
        cJSON_Delete(j); j = cloud_request(true); cJSON_Delete(j); return;
    }
    cJSON *w = cJSON_GetObjectItemCaseSensitive(j, "websocket");
    const char *url = str(w, "url"), *secret = str(w, "token");
    if (strncmp(url, "wss://", 6) || strlen(url) >= sizeof(endpoint) || strpbrk(url, "\r\n ") ||
        !secret[0] || strlen(secret) >= sizeof(token) || strpbrk(secret, "\r\n")) {
        cJSON_Delete(j); set_state("pairing_error"); return;
    }
    nvs_handle_t h;
    bool gate = flash_gate_enter(1000);
    esp_err_t err = gate ? nvs_open(NS, NVS_READWRITE, &h) : ESP_ERR_TIMEOUT;
    if (err == ESP_OK) {
        /* One NVS blob binds URL and token atomically. Never expose it over HTTP. */
        cJSON *saved = cJSON_CreateObject();
        cJSON_AddStringToObject(saved, "url", url); cJSON_AddStringToObject(saved, "token", secret);
        char *credentials = cJSON_PrintUnformatted(saved); cJSON_Delete(saved);
        err = credentials ? nvs_set_str(h, "credentials", credentials) : ESP_ERR_NO_MEM;
        free(credentials);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (gate) flash_gate_leave();
    if (err == ESP_OK) {
        strlcpy(endpoint, url, sizeof(endpoint)); strlcpy(token, secret, sizeof(token));
        xSemaphoreTake(lock, portMAX_DELAY); status.paired = true; status.code[0] = 0; xSemaphoreGive(lock);
        pairing = false; set_state("ready");
    } else set_state("storage_error");
    cJSON_Delete(j);
}

static void receive_mcp(const cJSON *message)
{
    cJSON *response = knx_mcp_message(message, session);
    if (!response) return;
    const char *method = str(cJSON_GetObjectItemCaseSensitive(message, "payload"), "method");
    char *text = cJSON_PrintUnformatted(response);
    bool sent = text && send_text(text);
    if (!text) failed = true;
    /* Log method names only, never arguments, credentials or session IDs. */
    const char *kind = !strcmp(method, "initialize") ? "initialize" :
        !strcmp(method, "tools/list") ? "tools/list" :
        !strcmp(method, "tools/call") ? "tools/call" : "other";
    ESP_LOGI("xiaozhi", "MCP %s response %s", kind, sent ? "sent" : "not sent");
    free(text); cJSON_Delete(response);
}

static void receive_packet(packet_t *p)
{
    if (p->opcode == 1) {
        cJSON *j = cJSON_ParseWithLength((char *)p->data, p->len);
        if (!j) { failed = true; return; }
        const char *type = str(j, "type");
        if (!strcmp(type, "hello")) {
            const char *id = str(j, "session_id");
            cJSON *audio = cJSON_GetObjectItemCaseSensitive(j, "audio_params");
            if (strcmp(str(audio, "format"), "opus") || !id[0] || strlen(id) >= sizeof(session) || strcmp(str(j, "transport"), "websocket")) failed = true;
            else { strlcpy(session, id, sizeof(session)); ready = true; set_state("ready"); }
        } else if (!strcmp(type, "mcp")) {
            receive_mcp(j);
        } else if (!strcmp(type, "stt") || !strcmp(type, "tts")) {
            bool tts = !strcmp(type, "tts");
            const char *phase = str(j, "state");
            if (tts && !strcmp(phase, "start")) {
                /* A server-side interruption may replace a reply before its
                 * already-decoded tail has drained. Never play that old tail. */
                if (stream.realtime) xiaozhi_duplex_flush();
                else xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
                xiaozhi_session_tts_start(&stream);
                opus_decoder_ctl(decoder, OPUS_RESET_STATE);
            }
            const char *text = str(j, "text");
            if (!tts && status.dialog_active && text[0] && xiaozhi_farewell(text)) {
                xSemaphoreTake(lock, portMAX_DELAY);
                strlcpy(status.recognized, text, sizeof(status.recognized));
                xSemaphoreGive(lock);
                cJSON_Delete(j); close_dialog("farewell"); return;
            }
            if (!tts && text[0] && stream.realtime) {
                xiaozhi_duplex_stats_t audio;
                xiaozhi_duplex_stats(&audio);
                /* Realtime turn detection belongs to the server. Residual
                 * echo can trip local VAD without producing a new utterance;
                 * it must not abort an otherwise valid reply. */
                if (xiaozhi_session_user_turn(&stream, audio.playing)) {
                    xiaozhi_duplex_flush();
                    xSemaphoreTake(lock, portMAX_DELAY); ++status.barge_ins; xSemaphoreGive(lock);
                    ESP_LOGI("xiaozhi", "Recognized user turn interrupted playback");
                }
            }
            if (text[0] && (!tts || !stream.drop_audio)) {
                xSemaphoreTake(lock, portMAX_DELAY);
                strlcpy(status.text, text, sizeof(status.text));
                strlcpy(!strcmp(type, "stt") ? status.recognized : status.reply, text, sizeof(status.text));
                xSemaphoreGive(lock);
            }
            if (tts) {
                if (!strcmp(phase, "start")) {
                    if (!stream.realtime) {
                        want_listen = false;
                        if (xiaozhi_audio_mic(false) != ESP_OK) failed = true;
                        xSemaphoreTake(lock, portMAX_DELAY); status.listening = false; xSemaphoreGive(lock);
                    }
                    set_state(stream.realtime ? "speaking_listening" : "speaking");
                } else if (!strcmp(phase, "stop")) {
                    xiaozhi_session_tts_stop(&stream);
                    if (!stream.realtime) {
                        set_state("ready");
                        if (auto_listen) { want_listen = true; listen_deadline = esp_timer_get_time() + 60000000; }
                    }
                }
            }
        } else if (!strcmp(type, "llm")) {
            const char *emotion = str(j, "emotion");
            if (emotion[0]) {
                xSemaphoreTake(lock, portMAX_DELAY); strlcpy(status.emotion, emotion, sizeof(status.emotion)); xSemaphoreGive(lock);
            }
        }
        cJSON_Delete(j);
    } else if (ready && xiaozhi_session_accept_audio(&stream, status.listening) && p->len >= 4) {
        int len = ((int)p->data[2] << 8) | p->data[3];
        if (p->data[0] != 0 || len != p->len - 4) { failed = true; return; }
        /* Opus can decode the server's 24/48 kHz stream directly at 16 kHz. */
        int n = opus_decode(decoder, p->data + 4, len, pcm, 1920, 0);
        uint32_t spectrum_generation = xiaozhi_spectrum_generation(XZ_SPECTRUM_SPEAKER);
        uint32_t activity_generation = xiaozhi_activity_generation();
        if (n <= 0 || (stream.realtime ? !xiaozhi_duplex_play(pcm, n) : xiaozhi_audio_write(pcm, n) != ESP_OK)) failed = true;
        else {
            if (!stream.realtime) xiaozhi_spectrum_feed(XZ_SPECTRUM_SPEAKER, pcm, n, spectrum_generation);
            if (!stream.realtime && status.volume > 0 && xiaozhi_activity_has_sound(pcm, n))
                xiaozhi_activity_note_playback(activity_generation, esp_timer_get_time());
            xSemaphoreTake(lock, portMAX_DELAY); ++status.audio_rx_frames; xSemaphoreGive(lock);
        }
    }
}

static void wake_config_result(bool failed_config)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    status.wake_config_failed = failed_config; ++status.wake_config_revision;
    xSemaphoreGive(lock);
}
static bool prepare_turn_start(command_t *c)
{
    portENTER_CRITICAL(&pending_lock);
    bool blocked = false;
    for (unsigned i = 0; i < 2; ++i)
        blocked |= pending_end_valid[i] && (uint32_t)pending_end[i].value == (uint32_t)c->value;
    c->stop_revision = stop_generation_valid && stop_generation == (uint32_t)c->value ? stop_revision : 0;
    portEXIT_CRITICAL(&pending_lock);
    return !blocked;
}
static bool turn_start_current(const command_t *c)
{
    portENTER_CRITICAL(&pending_lock);
    bool current = !stop_generation_valid || stop_generation != (uint32_t)c->value || c->stop_revision == stop_revision;
    portEXIT_CRITICAL(&pending_lock);
    return current;
}
static bool defer_turn_end(const command_t *c)
{
    int slot = !strcmp(c->action, "dismiss") ? 0 : !strcmp(c->action, "stop_turn") ? 1 : -1;
    if (slot < 0) return false;
    uint32_t generation = (uint32_t)c->value;
    /* Guard before updating cancellation history: an arbitrary future sequence
     * must not poison the current dialog's release. This read takes no status lock. */
    if (generation != xiaozhi_activity_generation()) return true;
    portENTER_CRITICAL(&pending_lock);
    /* A delayed UI teardown must never replace a newer generation's release. */
    if (!stop_generation_valid || (int32_t)(generation - stop_generation) >= 0) {
        stop_generation = generation; stop_generation_valid = true; ++stop_revision;
        pending_end[slot] = *c; pending_end_valid[slot] = true;
    }
    portEXIT_CRITICAL(&pending_lock);
    return true;
}
static bool take_turn_end(command_t *out)
{
    bool found = false;
    portENTER_CRITICAL(&pending_lock);
    for (unsigned i = 0; i < 2; ++i) {
        if (!pending_end_valid[i]) continue;
        *out = pending_end[i]; pending_end_valid[i] = false; found = true; break;
    }
    portEXIT_CRITICAL(&pending_lock);
    return found;
}
static void handle_command(command_t *c)
{
    if (!strcmp(c->action, "dismiss") && !status.dialog_active) {
        /* A failed session is already torn down. Acknowledge its visible
         * recovery view without losing diagnosis or touching another session. */
        if ((uint32_t)c->value == status.dialog_seq && status.dialog_end_seq == status.dialog_seq &&
            !strcmp(status.dialog_end_reason, "error")) {
            xSemaphoreTake(lock, portMAX_DELAY);
            strlcpy(status.dialog_end_reason, "manual", sizeof(status.dialog_end_reason));
            if (!strcmp(status.state, "connection_error") || !strcmp(status.state, "audio_or_connection_error"))
                strlcpy(status.state, status.enabled ? "ready" : "disabled", sizeof(status.state));
            xSemaphoreGive(lock);
        }
        return;
    }
    if (!strcmp(c->action, "dialog_turn")) {
        if (status.dialog_active || (uint32_t)c->value != status.dialog_seq) return;
        strlcpy(c->action, "dialog", sizeof(c->action));
    }
    if (!strcmp(c->action, "start_turn") || !strcmp(c->action, "stop_turn")) {
        if (!status.dialog_active || (uint32_t)c->value != status.dialog_seq) return;
        bool start = !strcmp(c->action, "start_turn");
        if (start && !turn_start_current(c)) return;
        strlcpy(c->action, start ? "start" : "stop", sizeof(c->action));
    }
    if (!strcmp(c->action, "wake_config")) {
        xiaozhi_wake_config_t next = {.word = c->value};
        strlcpy(next.phrase, c->phrase, sizeof(next.phrase));
        close_dialog("manual");
        /* Validate with the real engine before persisting; rejected words retain
         * the previous configuration. Only this worker ever owns either model. */
        bool valid = xiaozhi_wake_init(next.word, next.phrase);
        xiaozhi_wake_free();
        if (!valid) { set_state("wake_config_error"); wake_config_result(true); return; }
        if (!flash_gate_enter(1000)) { set_state("storage_error"); wake_config_result(true); return; }
        nvs_handle_t h;
        esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_blob(h, "wake_config", &next, sizeof(next));
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        flash_gate_leave();
        if (err != ESP_OK) { set_state("storage_error"); wake_config_result(true); return; }
        xSemaphoreTake(lock, portMAX_DELAY); status.wake_config = next; xSemaphoreGive(lock);
        wake_config_result(false);
        wake_retry = 0; set_state(status.enabled ? "ready" : "disabled"); return;
    }
    if (!strcmp(c->action, "reset")) {
        nvs_handle_t h;
        if (!flash_gate_enter(1000)) { set_state("storage_error"); return; }
        esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_u8(h, "enabled", 0);
            if (err == ESP_OK) err = nvs_set_u8(h, "volume", 50);
            if (err == ESP_OK) err = nvs_set_u8(h, "wake", 0);
            if (err == ESP_OK) err = nvs_set_u8(h, "duplex", 1);
            if (err == ESP_OK) err = nvs_set_u16(h, "dialog_timeout", 30);
            if (err == ESP_OK) err = nvs_set_u8(h, "emoji", 0);
            const xiaozhi_wake_config_t defaults = {.phrase = XIAOZHI_WAKE_DEFAULT_PHRASE};
            if (err == ESP_OK) err = nvs_set_blob(h, "wake_config", &defaults, sizeof(defaults));
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        flash_gate_leave();
        if (err != ESP_OK) { set_state("storage_error"); return; }
        pairing = false; close_dialog("disabled");
        xSemaphoreTake(lock, portMAX_DELAY); status.enabled = false; status.volume = 50; status.wake_enabled = false; status.dialog_timeout = 30;
        status.emoji_style = 0;
        status.duplex_enabled = true;
        status.wake_config = (xiaozhi_wake_config_t){.phrase = XIAOZHI_WAKE_DEFAULT_PHRASE};
        status.code[0] = status.text[0] = status.recognized[0] = status.reply[0] = status.emotion[0] = 0; xSemaphoreGive(lock);
        set_state("disabled"); return;
    }
    if (!strcmp(c->action, "enable") || !strcmp(c->action, "disable")) {
        bool enable = !strcmp(c->action, "enable");
        if (!save_enabled(enable)) { set_state("storage_error"); return; }
        xSemaphoreTake(lock, portMAX_DELAY); status.enabled = enable; xSemaphoreGive(lock);
        if (!enable) { pairing = false; close_dialog("disabled"); }
        set_state(enable ? "ready" : "disabled"); return;
    }
    /* Retain the legacy command/range without rewriting old persisted values.
     * Audio inactivity is now fixed at 30 seconds for every client. */
    if (!strcmp(c->action, "timeout")) return;
    if (!strcmp(c->action, "wake") || !strcmp(c->action, "duplex")) {
        nvs_handle_t h;
        if (!flash_gate_enter(1000)) { set_state("storage_error"); return; }
        esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_u8(h, c->action, c->value);
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        flash_gate_leave();
        if (err != ESP_OK) { set_state("storage_error"); return; }
        close_dialog("manual");
        xSemaphoreTake(lock, portMAX_DELAY);
        if (!strcmp(c->action, "wake")) status.wake_enabled = c->value != 0;
        else status.duplex_enabled = c->value != 0;
        xSemaphoreGive(lock);
        wake_retry = 0;
        set_state(status.enabled ? "ready" : "disabled"); return;
    }
    if (!status.enabled) return;
    if (!strcmp(c->action, "dialog")) {
        if (status.dialog_active || ws) close_dialog("manual");
        show_dialog(false);
    } else if (!strcmp(c->action, "pair")) {
        if (!identity_ready) { set_state("storage_error"); return; }
        close_dialog("manual"); pairing = true;
        pair_deadline = esp_timer_get_time() + 180000000; next_pair = 0; set_state("pairing");
    } else if (!strcmp(c->action, "start") || !strcmp(c->action, "open")) {
        if (!strcmp(c->action, "open") && (!status.wake_enabled || !status.wake_ready)) return;
        if (!status.paired || pairing) { set_state("pair_first"); return; }
        bool automatic = !strcmp(c->action, "open");
        if (ws && auto_listen != automatic) close_dialog("manual");
        if (!status.dialog_active) show_dialog(automatic);
        if (!ws && !connect_session()) return;
        auto_listen = automatic;
        conversation(true);
        want_listen = true;
        listen_deadline = esp_timer_get_time() + 60000000;
    } else if (!strcmp(c->action, "stop")) {
        want_listen = false;
    } else if (!strcmp(c->action, "disconnect") ||
               (!strcmp(c->action, "dismiss") && status.dialog_active && (uint32_t)c->value == status.dialog_seq)) {
        pairing = false; close_dialog("manual");
    } else if (!strcmp(c->action, "volume")) {
        nvs_handle_t h;
        bool gate = flash_gate_enter(1000);
        esp_err_t err = gate ? nvs_open(NS, NVS_READWRITE, &h) : ESP_ERR_TIMEOUT;
        if (err == ESP_OK) {
            err = nvs_set_u8(h, "volume", c->value);
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        if (gate) flash_gate_leave();
        if (err != ESP_OK) { set_state("storage_error"); return; }
        xSemaphoreTake(lock, portMAX_DELAY); status.volume = c->value; xSemaphoreGive(lock);
        if (ws && xiaozhi_audio_open(status.volume) != ESP_OK) failed = true;
    }
}

static void poll_wake(void)
{
    if (audio_stopping) return;
    bool enabled = status.enabled && status.paired && status.wake_enabled && !pairing;
    if (!enabled) {
        if (idle_capture) { xiaozhi_audio_close(); idle_capture = false; }
        if (wake_pcm) { free(wake_pcm); wake_pcm = NULL; xiaozhi_wake_free(); }
        xSemaphoreTake(lock, portMAX_DELAY); status.wake_ready = status.wake_running = false; xSemaphoreGive(lock);
        return;
    }
    if (ws || esp_timer_get_time() < wake_retry) {
        xSemaphoreTake(lock, portMAX_DELAY); status.wake_running = false; xSemaphoreGive(lock); return;
    }
    if (!wake_pcm) {
        wake_pcm = heap_caps_malloc(3840 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!wake_pcm) goto error;
    }
    /* Disconnect releases the detector independently of this PCM allocation. */
    if (!xiaozhi_wake_init(status.wake_config.word, status.wake_config.phrase)) goto error;
    if (!idle_capture) {
        if (xiaozhi_audio_open(0) != ESP_OK || xiaozhi_audio_mic(true) != ESP_OK) goto error;
        idle_capture = true; wake_last_loud = 0; set_state("ready");
    }
    xSemaphoreTake(lock, portMAX_DELAY); status.wake_ready = status.wake_running = true; xSemaphoreGive(lock);
    int n = xiaozhi_wake_chunk();
    if (xiaozhi_audio_read(wake_pcm, n) != ESP_OK) goto error;
    int mic_peak, ref_peak;
    xiaozhi_pcm_mono(wake_pcm, n, &mic_peak, &ref_peak);
    int64_t started = esp_timer_get_time();
    if (mic_peak >= 256) wake_last_loud = started;
    bool detected = xiaozhi_wake_detect(wake_pcm);
    int detect_us = (int)(esp_timer_get_time() - started);
    xSemaphoreTake(lock, portMAX_DELAY);
    ++status.wake_frames;
    status.wake_mic_peak = mic_peak; status.wake_ref_peak = ref_peak;
    status.wake_detect_us = detect_us;
    if (detected) ++status.wake_hits;
    xSemaphoreGive(lock);
    if (detected) {
        /* An energy marker, not a measured human speech endpoint. No idle
         * recording or transcript is retained in these timing diagnostics. */
        ESP_LOGI("xiaozhi", "Local wake detected (word=%d, chunk=%d, loud_age_ms=%d)",
                 status.wake_config.word, n,
                 wake_last_loud ? (int)((esp_timer_get_time() - wake_last_loud) / 1000) : -1);
        show_dialog(true);
        if (connect_session()) {
            auto_listen = want_listen = true; conversation(true);
            listen_deadline = esp_timer_get_time() + 60000000;
        }
    }
    return;
error:
    idle_capture = false; xiaozhi_audio_close(); xiaozhi_wake_free(); free(wake_pcm); wake_pcm = NULL;
    xSemaphoreTake(lock, portMAX_DELAY); status.wake_ready = status.wake_running = false; xSemaphoreGive(lock);
    set_state("wake_error"); wake_retry = esp_timer_get_time() + 30000000;
}

static void change_capture(void)
{
    if (want_listen) {
        if (!control("abort", NULL)) return;
        xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
        drain_packets(true);
        xiaozhi_session_reset(&stream, auto_listen && status.duplex_enabled);
        if (!stream.realtime && !manual_vad) manual_vad = vad_create(VAD_MODE_3);
        if (stream.realtime && !duplex_frame)
            duplex_frame = heap_caps_malloc(sizeof(*duplex_frame), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if ((stream.realtime ? !duplex_frame : !manual_vad) || xiaozhi_audio_mic(true) != ESP_OK ||
            (stream.realtime && xiaozhi_duplex_start() != ESP_OK)) { failed = true; return; }
    } else {
        if (stream.realtime) { close_dialog("manual"); return; }
        if (xiaozhi_audio_mic(false) != ESP_OK) { failed = true; return; }
    }
    if (!control("listen", want_listen ? "start" : "stop")) return;
    xSemaphoreTake(lock, portMAX_DELAY); status.listening = want_listen; xSemaphoreGive(lock);
    set_state(want_listen ? "listening" : "waiting");
    if (want_listen && auto_listen)
        ESP_LOGI("xiaozhi", "Voice session listening (%s)", stream.realtime ? "realtime, local AEC" : "auto");
}

static bool send_capture(void)
{
    const int16_t *samples = pcm;
    bool overlap = false;
    if (stream.realtime) {
        if (!xiaozhi_duplex_take(duplex_frame)) return false;
        samples = duplex_frame->pcm; overlap = duplex_frame->playback;
    } else {
        uint32_t spectrum_generation = xiaozhi_spectrum_generation(XZ_SPECTRUM_MIC);
        uint32_t activity_generation = xiaozhi_activity_generation();
        if (xiaozhi_audio_read(pcm, FRAME_SAMPLES) != ESP_OK) { failed = true; return true; }
        xiaozhi_pcm_mono(pcm, FRAME_SAMPLES, NULL, NULL);
        xiaozhi_spectrum_feed(XZ_SPECTRUM_MIC, pcm, FRAME_SAMPLES, spectrum_generation);
        bool speech = false;
        for (int offset = 0; manual_vad && offset < FRAME_SAMPLES; offset += 480)
            speech |= vad_process(manual_vad, pcm + offset, 16000, 30) == VAD_SPEECH;
        if (speech && xiaozhi_activity_has_sound(pcm, FRAME_SAMPLES))
            xiaozhi_activity_note(activity_generation, esp_timer_get_time());
    }
    int n = opus_encode(encoder, samples, FRAME_SAMPLES, encoded + 4, 1500);
    if (n <= 0) { failed = true; return true; }
    encoded[0] = encoded[1] = 0; encoded[2] = n >> 8; encoded[3] = n & 255;
    if (send_packet(encoded, n + 4, true)) {
        xSemaphoreTake(lock, portMAX_DELAY);
        ++status.audio_tx_frames;
        if (overlap) ++status.audio_tx_during_playback;
        xSemaphoreGive(lock);
    }
    return true;
}

static void finish_connection(int64_t now)
{
    xiaozhi_connection_t link = connection_snapshot();
    if (!ws || (!failed && link.end == XZ_END_NONE && (ready || now <= deadline))) return;
    if (failed) connection_fail(XZ_END_LOCAL);
    else if (!ready && now > deadline) connection_fail(XZ_END_HANDSHAKE_TIMEOUT);
    disconnect_session();
    link = connection_snapshot(); /* stop/destroy joined the final callback */
    bool error = xiaozhi_connection_error(&link);
    end_dialog(error ? "error" : "peer_close");
    set_state(error ? "connection_error" : "ready");
}

static void worker(void *unused)
{
    (void)unused;
    for (;;) {
        if (audio_stopping && xiaozhi_duplex_stop() == ESP_OK) {
            audio_stopping = false; xiaozhi_audio_close();
        }
        command_t c;
        while (take_turn_end(&c)) handle_command(&c);
        while (xQueueReceive(commands, &c, 0) == pdTRUE) handle_command(&c);
        while (take_turn_end(&c)) handle_command(&c);
        int64_t now = esp_timer_get_time();
        if (pairing && now >= pair_deadline) { pairing = false; set_state("pairing_expired"); }
        if (pairing && now >= next_pair) { pair_step(); next_pair = esp_timer_get_time() + 5000000; }
        finish_connection(now);
        if (ws && connection_can_send() && !hello_sent) {
            hello_sent = send_text("{\"type\":\"hello\",\"version\":3,\"transport\":\"websocket\",\"features\":{\"mcp\":true},\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}");
        }
        packet_t p;
        bool received = ws && xQueueReceive(packets, &p, 0) == pdTRUE;
        if (received) { receive_packet(&p); free(p.data); }
        /* The atomic claim arbitrates a simultaneous real audio observation.
         * Empty PCM, text, network traffic and UI refreshes cannot extend it. */
        if (status.dialog_active && xiaozhi_activity_expire(status.dialog_seq)) close_dialog("silence");
        if (want_listen && !stream.realtime && now >= listen_deadline) want_listen = false;
        if (ready && !failed && want_listen != status.listening) change_capture();
        bool sent = ready && status.listening && !failed && connection_can_send() && send_capture();
        update_audio_status();
        if (!received && !sent) vTaskDelay(pdMS_TO_TICKS(10));
        poll_wake();
    }
}

static esp_err_t enqueue_command(command_t *c)
{
    const char *action = c->action;
    if (!commands) return ESP_ERR_INVALID_STATE;
    if (!strcmp(action, "start_turn") && !prepare_turn_start(c)) return ESP_ERR_TIMEOUT;
    /* The long-lived worker needs no state-lock round trip for an enqueue.
     * In particular a Talk release or a generation-tagged dismiss is not lost
     * merely because an HTTP status copy currently owns the mutex. */
    if (atomic_load(&worker_started)) {
        /* All guarded endings use the same bounded path, even with queue space.
         * This orders a delayed start producer against an accepted release. */
        if (defer_turn_end(c)) return ESP_OK;
        return xQueueSend(commands, c, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
    }
    if (xSemaphoreTake(lock, 0) != pdTRUE) return ESP_ERR_TIMEOUT;
    /* Screen teardown must not allocate an audio worker while disabled. */
    if (!worker_task && !status.enabled && strcmp(action, "enable") && strcmp(action, "reset") && strcmp(action, "wake") && strcmp(action, "duplex") && strcmp(action, "timeout") && strcmp(action, "wake_config")) {
        xSemaphoreGive(lock); return ESP_OK;
    }
    if (!worker_task && xTaskCreate(worker, "xiaozhi", 32768, NULL, 4, &worker_task) != pdPASS) {
        xSemaphoreGive(lock); return ESP_ERR_NO_MEM;
    }
    atomic_store(&worker_started, true);
    xSemaphoreGive(lock);
    if (defer_turn_end(c)) return ESP_OK;
    return xQueueSend(commands, c, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_err_t xiaozhi_command(const char *action, int value)
{
    if (!action || !commands || strlen(action) >= sizeof(((command_t *)0)->action)) return ESP_ERR_INVALID_STATE;
    if (strcmp(action, "reset") && strcmp(action, "enable") && strcmp(action, "disable") && strcmp(action, "pair") &&
        strcmp(action, "wake") && strcmp(action, "duplex") && strcmp(action, "timeout") && strcmp(action, "open") && strcmp(action, "start") && strcmp(action, "stop") && strcmp(action, "start_turn") && strcmp(action, "stop_turn") && strcmp(action, "disconnect") && strcmp(action, "dialog") && strcmp(action, "dialog_turn") && strcmp(action, "dismiss") && strcmp(action, "volume")) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "volume") && (value < 0 || value > 80)) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "wake") && value != 0 && value != 1) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "duplex") && value != 0 && value != 1) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "timeout") && (value < 10 || value > 300)) return ESP_ERR_INVALID_ARG;
    command_t c = {.value = value}; strlcpy(c.action, action, sizeof(c.action));
    return enqueue_command(&c);
}
esp_err_t xiaozhi_configure_wake(int word, const char *phrase)
{
    if (word < 0 || word > XIAOZHI_WAKE_WORD_COUNT || !xiaozhi_wake_phrase_valid(phrase)) return ESP_ERR_INVALID_ARG;
    if (word == XIAOZHI_WAKE_WORD_COUNT && !xiaozhi_custom_model_available()) return ESP_ERR_INVALID_STATE;
    command_t c = {.action = "wake_config", .value = word};
    strlcpy(c.phrase, phrase, sizeof(c.phrase));
    return enqueue_command(&c);
}

void xiaozhi_init(void)
{
    if (lock) return;
    lock = xSemaphoreCreateMutex(); commands = xQueueCreate(8, sizeof(command_t)); packets = xQueueCreate(24, sizeof(packet_t));
    if (!lock || !commands || !packets) return;
    status.dialog_epoch = esp_random();
    if (!status.dialog_epoch) status.dialog_epoch = 1;
    uint8_t mac[6] = {0}; bool mac_ok = esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK;
    snprintf(device, sizeof(device), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvs_handle_t h;
    bool init_gate = flash_gate_enter(1000);
    if (init_gate && nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        uint8_t enabled = 0, volume = 50, wake = 0, duplex = 1;
        nvs_get_u8(h, "enabled", &enabled); status.enabled = enabled == 1;
        nvs_get_u8(h, "volume", &volume); status.volume = volume <= 80 ? volume : 50;
        nvs_get_u8(h, "wake", &wake); status.wake_enabled = wake == 1;
        nvs_get_u8(h, "duplex", &duplex); status.duplex_enabled = duplex != 0;
        /* Keep the saved legacy timeout untouched; every current client uses 30 seconds. */
        status.dialog_timeout = XIAOZHI_SILENCE_TIMEOUT_MS / 1000;
        uint8_t emoji = 0; nvs_get_u8(h, "emoji", &emoji); status.emoji_style = emoji == 1 ? 1 : 0;
        xiaozhi_wake_config_t saved = {0}; size_t config_len = sizeof(saved);
        if (nvs_get_blob(h, "wake_config", &saved, &config_len) == ESP_OK && config_len == sizeof(saved) &&
            saved.word >= 0 && saved.word <= XIAOZHI_WAKE_WORD_COUNT &&
            memchr(saved.phrase, 0, sizeof(saved.phrase)) && xiaozhi_wake_phrase_valid(saved.phrase)) status.wake_config = saved;
        size_t len = sizeof(uuid);
        esp_err_t identity_err = nvs_get_str(h, "uuid", uuid, &len);
        if (identity_err != ESP_OK || strlen(uuid) != 36) {
            uint8_t b[16]; esp_fill_random(b, sizeof(b)); b[6] = (b[6] & 15) | 64; b[8] = (b[8] & 63) | 128;
            snprintf(uuid, sizeof(uuid), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
            identity_err = nvs_set_str(h, "uuid", uuid);
            if (identity_err == ESP_OK) identity_err = nvs_commit(h);
        }
        identity_ready = mac_ok && identity_err == ESP_OK;
        char credentials[1536]; len = sizeof(credentials);
        if (nvs_get_str(h, "credentials", credentials, &len) == ESP_OK) {
            cJSON *j = cJSON_Parse(credentials);
            strlcpy(endpoint, str(j, "url"), sizeof(endpoint)); strlcpy(token, str(j, "token"), sizeof(token));
            cJSON_Delete(j); memset(credentials, 0, sizeof(credentials));
            status.paired = !strncmp(endpoint, "wss://", 6) && token[0] && !strpbrk(token, "\r\n") && !strpbrk(endpoint, "\r\n ");
        }
        nvs_close(h);
    }
    if (init_gate) flash_gate_leave();
    set_state(status.enabled ? "ready" : "disabled");
    /* Opus uses substantial temporary stack. Keep this flash-writing worker internal. */
    if (status.enabled && xTaskCreate(worker, "xiaozhi", 32768, NULL, 4, &worker_task) != pdPASS) {
        vQueueDelete(commands); commands = NULL; set_state("memory_error");
    }
    if (worker_task) atomic_store(&worker_started, true);
}
#else
esp_err_t xiaozhi_configure_wake(int word, const char *phrase) { (void)word; (void)phrase; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t xiaozhi_set_emoji_style(int value) { (void)value; return ESP_ERR_NOT_SUPPORTED; }
void xiaozhi_init(void) {}
void xiaozhi_status(xiaozhi_status_t *out) { memset(out, 0, sizeof(*out)); strcpy(out->state, "unsupported"); }
bool xiaozhi_status_try(xiaozhi_status_t *out) { if (!out) return false; xiaozhi_status(out); return true; }
esp_err_t xiaozhi_command(const char *action, int value) { (void)action; (void)value; return ESP_ERR_NOT_SUPPORTED; }
#endif
