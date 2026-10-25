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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "util/flash_gate.h"

#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
#include "xiaozhi_audio.h"
#include "xiaozhi_wake.h"
#include "xiaozhi_pcm.h"
#include "xiaozhi_dialog.h"
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
typedef struct { char action[16]; int value; } command_t;
typedef struct { int opcode, len; uint8_t *data; } packet_t;
static SemaphoreHandle_t lock;
static QueueHandle_t commands, packets;
static TaskHandle_t worker_task;
static xiaozhi_status_t status = {.supported = true, .volume = 50, .dialog_timeout = 30};
static char endpoint[256], token[1024], device[18], uuid[37], session[128];
static esp_websocket_client_handle_t ws;
static volatile bool connected, failed;
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
static int64_t last_activity, wake_retry;
static int16_t *wake_pcm;

static void activity(void)
{
    last_activity = esp_timer_get_time();
    xSemaphoreTake(lock, portMAX_DELAY); ++status.activity_seq; xSemaphoreGive(lock);
}
static void show_dialog(void)
{
    activity();
    xSemaphoreTake(lock, portMAX_DELAY); ++status.dialog_seq; xSemaphoreGive(lock);
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

void xiaozhi_status(xiaozhi_status_t *out)
{
    if (!lock) { memset(out, 0, sizeof(*out)); out->supported = true; return; }
    xSemaphoreTake(lock, portMAX_DELAY); *out = status; xSemaphoreGive(lock);
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
    want_listen = auto_listen = idle_capture = false;
    conversation(false);
    wake_retry = esp_timer_get_time() + 1500000;
    xiaozhi_wake_reset();
    xiaozhi_audio_close();
    if (ws) { esp_websocket_client_stop(ws); esp_websocket_client_destroy(ws); ws = NULL; }
    drain_packets(false);
    free(rx); rx = NULL;
    free(encoder); encoder = NULL;
    free(decoder); decoder = NULL;
    free(pcm); pcm = NULL;
    free(encoded); encoded = NULL;
    connected = ready = hello_sent = false;
    session[0] = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    status.listening = status.wake_ready = status.wake_running = false;
    xSemaphoreGive(lock);
}

/* Event task only assembles bounded messages. The worker owns audio and JSON. */
static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == WEBSOCKET_EVENT_CONNECTED) { connected = true; return; }
    if (id == WEBSOCKET_EVENT_ERROR || id == WEBSOCKET_EVENT_DISCONNECTED) { failed = true; return; }
    if (id != WEBSOCKET_EVENT_DATA) return;
    esp_websocket_event_data_t *e = data;
    if (e->op_code >= 8) return;
    if (e->data_len < 0 || e->payload_offset < 0 || e->payload_len < 0 ||
        e->payload_len > MAX_FRAME || rx_len + (size_t)e->data_len > MAX_FRAME) {
        failed = true; return;
    }
    if (e->payload_offset == 0 && (e->op_code == 1 || e->op_code == 2)) {
        rx_len = 0; rx_opcode = e->op_code;
    }
    if (!rx || (rx_opcode != 1 && rx_opcode != 2)) { failed = true; return; }
    memcpy(rx + rx_len, e->data_ptr, e->data_len); rx_len += e->data_len;
    if (!e->fin || e->payload_offset + e->data_len != e->payload_len) return;
    packet_t p = {.opcode = rx_opcode, .len = rx_len};
    p.data = heap_caps_malloc(rx_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p.data) { failed = true; return; }
    memcpy(p.data, rx, rx_len); p.data[rx_len] = 0; rx_len = 0; rx_opcode = 0;
    if (xQueueSend(packets, &p, 0) != pdTRUE) { free(p.data); failed = true; }
}

static bool send_text(const char *text)
{
    return ws && esp_websocket_client_send_text(ws, text, strlen(text), pdMS_TO_TICKS(500)) == (int)strlen(text);
}

static bool control(const char *type, const char *state)
{
    cJSON *j = cJSON_CreateObject();
    if (!j) return false;
    cJSON_AddStringToObject(j, "session_id", session);
    cJSON_AddStringToObject(j, "type", type);
    if (state) { cJSON_AddStringToObject(j, "state", state); cJSON_AddStringToObject(j, "mode", auto_listen ? "auto" : "manual"); }
    char *s = cJSON_PrintUnformatted(j); cJSON_Delete(j);
    bool ok = s && send_text(s); free(s); return ok;
}

static bool connect_session(void)
{
    /* Discard the idle capture stream before the cloud handshake. Resume only
     * on listen/start, so buffered wake audio is not treated as the question. */
    if (idle_capture && xiaozhi_audio_mic(false) != ESP_OK) goto error;
    idle_capture = false;
    if (!identity_ready || strncmp(endpoint, "wss://", 6) || !token[0]) return false;
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
    failed = false; rx_len = 0; rx_opcode = 0;
    deadline = esp_timer_get_time() + 15000000;
    if (esp_websocket_client_start(ws) != ESP_OK) goto error;
    set_state("connecting"); return true;
error:
    disconnect_session(); set_state("audio_or_connection_error"); return false;
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
    activity();
    const char *method = str(cJSON_GetObjectItemCaseSensitive(message, "payload"), "method");
    char *text = cJSON_PrintUnformatted(response);
    bool sent = text && send_text(text);
    if (!sent) failed = true;
    /* Log method names only, never arguments, credentials or session IDs. */
    const char *kind = !strcmp(method, "initialize") ? "initialize" :
        !strcmp(method, "tools/list") ? "tools/list" :
        !strcmp(method, "tools/call") ? "tools/call" : "other";
    ESP_LOGI("xiaozhi", "MCP %s response %s", kind, sent ? "sent" : "failed");
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
            const char *text = str(j, "text");
            if (text[0]) {
                activity();
                xSemaphoreTake(lock, portMAX_DELAY);
                strlcpy(status.text, text, sizeof(status.text));
                strlcpy(!strcmp(type, "stt") ? status.recognized : status.reply, text, sizeof(status.text));
                xSemaphoreGive(lock);
            }
            if (!strcmp(type, "tts")) {
                activity();
                if (!strcmp(str(j, "state"), "start")) {
                    want_listen = false;
                    xiaozhi_audio_mic(false);
                    xSemaphoreTake(lock, portMAX_DELAY); status.listening = false; xSemaphoreGive(lock);
                    set_state("speaking");
                } else if (!strcmp(str(j, "state"), "stop")) {
                    set_state("ready");
                    if (auto_listen) { want_listen = true; listen_deadline = esp_timer_get_time() + 60000000; }
                }
            }
        } else if (!strcmp(type, "llm")) {
            const char *emotion = str(j, "emotion");
            if (emotion[0]) {
                xSemaphoreTake(lock, portMAX_DELAY); strlcpy(status.emotion, emotion, sizeof(status.emotion)); xSemaphoreGive(lock);
            }
        }
        cJSON_Delete(j);
    } else if (ready && !status.listening && p->len >= 4) {
        int len = ((int)p->data[2] << 8) | p->data[3];
        if (p->data[0] != 0 || len != p->len - 4) { failed = true; return; }
        /* Opus can decode the server's 24/48 kHz stream directly at 16 kHz. */
        int n = opus_decode(decoder, p->data + 4, len, pcm, 1920, 0);
        if (n < 0 || xiaozhi_audio_write(pcm, n) != ESP_OK) failed = true;
        else activity();
    }
}

static void handle_command(command_t *c)
{
    if (!strcmp(c->action, "reset")) {
        nvs_handle_t h;
        if (!flash_gate_enter(1000)) { set_state("storage_error"); return; }
        esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_u8(h, "enabled", 0);
            if (err == ESP_OK) err = nvs_set_u8(h, "volume", 50);
            if (err == ESP_OK) err = nvs_set_u8(h, "wake", 0);
            if (err == ESP_OK) err = nvs_set_u16(h, "dialog_timeout", 30);
            if (err == ESP_OK) err = nvs_set_u8(h, "emoji", 0);
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        flash_gate_leave();
        if (err != ESP_OK) { set_state("storage_error"); return; }
        pairing = false; disconnect_session();
        xSemaphoreTake(lock, portMAX_DELAY); status.enabled = false; status.volume = 50; status.wake_enabled = false; status.dialog_timeout = 30;
        status.emoji_style = 0;
        status.code[0] = status.text[0] = status.recognized[0] = status.reply[0] = status.emotion[0] = 0; xSemaphoreGive(lock);
        set_state("disabled"); return;
    }
    if (!strcmp(c->action, "enable") || !strcmp(c->action, "disable")) {
        bool enable = !strcmp(c->action, "enable");
        if (!save_enabled(enable)) { set_state("storage_error"); return; }
        xSemaphoreTake(lock, portMAX_DELAY); status.enabled = enable; xSemaphoreGive(lock);
        if (!enable) { pairing = false; disconnect_session(); }
        set_state(enable ? "ready" : "disabled"); return;
    }
    if (!strcmp(c->action, "wake") || !strcmp(c->action, "timeout")) {
        nvs_handle_t h;
        if (!flash_gate_enter(1000)) { set_state("storage_error"); return; }
        esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = !strcmp(c->action, "wake") ? nvs_set_u8(h, "wake", c->value) : nvs_set_u16(h, "dialog_timeout", c->value);
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        flash_gate_leave();
        if (err != ESP_OK) { set_state("storage_error"); return; }
        if (!strcmp(c->action, "wake")) {
            disconnect_session();
            xSemaphoreTake(lock, portMAX_DELAY); status.wake_enabled = c->value != 0; xSemaphoreGive(lock);
            wake_retry = 0;
        } else {
            xSemaphoreTake(lock, portMAX_DELAY); status.dialog_timeout = c->value; xSemaphoreGive(lock);
        }
        set_state(status.enabled ? "ready" : "disabled"); return;
    }
    if (!status.enabled) return;
    if (!strcmp(c->action, "pair")) {
        if (!identity_ready) { set_state("storage_error"); return; }
        disconnect_session(); pairing = true;
        pair_deadline = esp_timer_get_time() + 180000000; next_pair = 0; set_state("pairing");
    } else if (!strcmp(c->action, "start") || !strcmp(c->action, "open")) {
        if (!strcmp(c->action, "open") && (!status.wake_enabled || !status.wake_ready)) return;
        if (!status.paired || pairing) { set_state("pair_first"); return; }
        if (!ws && !connect_session()) return;
        auto_listen = !strcmp(c->action, "open");
        conversation(true); activity();
        want_listen = true;
        listen_deadline = esp_timer_get_time() + 60000000;
    } else if (!strcmp(c->action, "stop")) {
        want_listen = false;
    } else if (!strcmp(c->action, "disconnect")) {
        pairing = false; disconnect_session(); set_state("ready");
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
    if (!xiaozhi_wake_init()) goto error;
    if (!idle_capture) {
        if (xiaozhi_audio_open(0) != ESP_OK || xiaozhi_audio_mic(true) != ESP_OK) goto error;
        idle_capture = true; set_state("ready");
    }
    xSemaphoreTake(lock, portMAX_DELAY); status.wake_ready = status.wake_running = true; xSemaphoreGive(lock);
    int n = xiaozhi_wake_chunk();
    if (xiaozhi_audio_read(wake_pcm, n) != ESP_OK) goto error;
    int mic_peak, ref_peak;
    xiaozhi_pcm_mono(wake_pcm, n, &mic_peak, &ref_peak);
    int64_t started = esp_timer_get_time();
    bool detected = xiaozhi_wake_detect(wake_pcm);
    int detect_us = (int)(esp_timer_get_time() - started);
    xSemaphoreTake(lock, portMAX_DELAY);
    ++status.wake_frames;
    status.wake_mic_peak = mic_peak; status.wake_ref_peak = ref_peak;
    status.wake_detect_us = detect_us;
    if (detected) ++status.wake_hits;
    xSemaphoreGive(lock);
    if (detected) {
        show_dialog();
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

static void worker(void *unused)
{
    (void)unused;
    for (;;) {
        command_t c;
        while (xQueueReceive(commands, &c, 0) == pdTRUE) handle_command(&c);
        int64_t now = esp_timer_get_time();
        if (pairing && now >= pair_deadline) { pairing = false; set_state("pairing_expired"); }
        if (pairing && now >= next_pair) { pair_step(); next_pair = esp_timer_get_time() + 5000000; }
        if (ws && (failed || (!ready && now > deadline))) {
            disconnect_session(); set_state("connection_error");
        }
        if (ws && connected && !hello_sent) {
            hello_sent = send_text("{\"type\":\"hello\",\"version\":3,\"transport\":\"websocket\",\"features\":{\"mcp\":true},\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}");
            if (!hello_sent) failed = true;
        }
        packet_t p;
        bool received = ws && xQueueReceive(packets, &p, 0) == pdTRUE;
        if (received) { receive_packet(&p); free(p.data); }
        if (ws && xiaozhi_dialog_expired(esp_timer_get_time()/1000, last_activity/1000, status.dialog_timeout)) {
            disconnect_session(); set_state("ready");
        }
        if (want_listen && now >= listen_deadline) want_listen = false;
        if (ready && want_listen != status.listening) {
            if (want_listen) { control("abort", NULL); drain_packets(true); }
            if (!control("listen", want_listen ? "start" : "stop") ||
                xiaozhi_audio_mic(want_listen) != ESP_OK) failed = true;
            xSemaphoreTake(lock, portMAX_DELAY); status.listening = want_listen; xSemaphoreGive(lock);
            set_state(want_listen ? "listening" : "waiting");
        }
        if (ready && status.listening && !failed) {
            if (xiaozhi_audio_read(pcm, FRAME_SAMPLES) != ESP_OK) failed = true;
            else {
                xiaozhi_pcm_mono(pcm, FRAME_SAMPLES, NULL, NULL);
                if (!auto_listen || xiaozhi_voice_active(pcm, FRAME_SAMPLES)) activity();
                int n = opus_encode(encoder, pcm, FRAME_SAMPLES, encoded + 4, 1500);
                if (n <= 0) failed = true;
                else {
                    encoded[0] = encoded[1] = 0; encoded[2] = n >> 8; encoded[3] = n & 255;
                    if (esp_websocket_client_send_bin(ws, (char *)encoded, n + 4, pdMS_TO_TICKS(500)) != n + 4) failed = true;
                }
            }
        } else if (!received) vTaskDelay(pdMS_TO_TICKS(10));
        poll_wake();
    }
}

esp_err_t xiaozhi_command(const char *action, int value)
{
    if (!action || !commands || strlen(action) >= sizeof(((command_t *)0)->action)) return ESP_ERR_INVALID_STATE;
    if (strcmp(action, "reset") && strcmp(action, "enable") && strcmp(action, "disable") && strcmp(action, "pair") &&
        strcmp(action, "wake") && strcmp(action, "timeout") && strcmp(action, "open") && strcmp(action, "start") && strcmp(action, "stop") && strcmp(action, "disconnect") && strcmp(action, "volume")) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "volume") && (value < 0 || value > 80)) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "wake") && value != 0 && value != 1) return ESP_ERR_INVALID_ARG;
    if (!strcmp(action, "timeout") && (value < 10 || value > 300)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(lock, portMAX_DELAY);
    /* Screen teardown must not allocate an audio worker while disabled. */
    if (!worker_task && !status.enabled && strcmp(action, "enable") && strcmp(action, "reset") && strcmp(action, "wake") && strcmp(action, "timeout")) {
        xSemaphoreGive(lock); return ESP_OK;
    }
    if (!worker_task && xTaskCreate(worker, "xiaozhi", 32768, NULL, 4, &worker_task) != pdPASS) {
        xSemaphoreGive(lock); return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(lock);
    command_t c = {.value = value}; strlcpy(c.action, action, sizeof(c.action));
    return xQueueSend(commands, &c, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void xiaozhi_init(void)
{
    if (lock) return;
    lock = xSemaphoreCreateMutex(); commands = xQueueCreate(8, sizeof(command_t)); packets = xQueueCreate(24, sizeof(packet_t));
    if (!lock || !commands || !packets) return;
    uint8_t mac[6] = {0}; bool mac_ok = esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK;
    snprintf(device, sizeof(device), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvs_handle_t h;
    bool init_gate = flash_gate_enter(1000);
    if (init_gate && nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        uint8_t enabled = 0, volume = 50, wake = 0; uint16_t timeout = 30;
        nvs_get_u8(h, "enabled", &enabled); status.enabled = enabled == 1;
        nvs_get_u8(h, "volume", &volume); status.volume = volume <= 80 ? volume : 50;
        nvs_get_u8(h, "wake", &wake); status.wake_enabled = wake == 1;
        nvs_get_u16(h, "dialog_timeout", &timeout); status.dialog_timeout = timeout >= 10 && timeout <= 300 ? timeout : 30;
        uint8_t emoji = 0; nvs_get_u8(h, "emoji", &emoji); status.emoji_style = emoji == 1 ? 1 : 0;
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
}
#else
esp_err_t xiaozhi_set_emoji_style(int value) { (void)value; return ESP_ERR_NOT_SUPPORTED; }
void xiaozhi_init(void) {}
void xiaozhi_status(xiaozhi_status_t *out) { memset(out, 0, sizeof(*out)); strcpy(out->state, "unsupported"); }
esp_err_t xiaozhi_command(const char *action, int value) { (void)action; (void)value; return ESP_ERR_NOT_SUPPORTED; }
#endif
