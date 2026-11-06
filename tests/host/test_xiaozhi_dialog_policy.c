/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "cJSON.h"
#include "esp_vad.h"
#include "xiaozhi/xiaozhi.h"
#include "xiaozhi/xiaozhi_activity.h"
#include "xiaozhi/xiaozhi_farewell.h"
#include "xiaozhi/xiaozhi_session.h"
#include "xiaozhi/xiaozhi_duplex.h"
#include "xiaozhi/xiaozhi_pcm.h"
#define assert(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(99); } } while (0)
#define portMAX_DELAY 0
#define ESP_ERR_INVALID_STATE 0x103
#define pdTRUE 1
#define NVS_READWRITE 1
#define NS "xiaozhi"
#define FRAME_SAMPLES 960
#define OPUS_RESET_STATE 1
#define ESP_LOGI(...) ((void)0)
typedef int nvs_handle_t;
typedef struct { char action[16]; int value; uint32_t stop_revision; char phrase[XIAOZHI_WAKE_PHRASE_SIZE]; } command_t;
typedef struct { int opcode, len; uint8_t *data; } packet_t;
static xiaozhi_status_t status;
static xiaozhi_session_t stream;
static int lock, commands = 1, packets = 2, mutex_depth, decoder, encoder;
static portMUX_TYPE pending_lock = portMUX_INITIALIZER_UNLOCKED;
static command_t pending_end[2], queue[8];
static bool pending_end_valid[2], stop_generation_valid;
static uint32_t stop_generation, stop_revision;
static unsigned queue_count;
static atomic_bool worker_started = true;
static void *worker_task;
static bool failed, ready, hello_sent, want_listen, auto_listen, pairing, audio_stopping;
static bool identity_ready = true, output_failure, capture_speech, pcm_silent;
static int64_t now, wake_retry, listen_deadline, pair_deadline, next_pair;
static unsigned connects, closes, mcp_messages, audio_writes, queued_audio, nvs_writes;
static int16_t pcm_storage[1920];
static uint8_t encoded_storage[1504];
static int16_t *pcm = pcm_storage;
static uint8_t *encoded = encoded_storage;
static xiaozhi_duplex_frame_t frame_storage, *duplex_frame = &frame_storage;
static vad_handle_t manual_vad = (vad_handle_t)1;
static char session[128];
static void *ws;
static jmp_buf worker_return;
static void end_dialog(const char *reason);
static void set_state(const char *state);
static void conversation(bool active);
int64_t esp_timer_get_time(void) { return now; }
static int xSemaphoreTake(int handle, int wait) { (void)handle; (void)wait; assert(!mutex_depth++); return pdTRUE; }
static void xSemaphoreGive(int handle) { (void)handle; assert(!--mutex_depth); }
static size_t strlcpy(char *dst, const char *src, size_t size)
{ size_t length = strlen(src); snprintf(dst, size, "%s", src); return length; }
static int xQueueReceive(int q, void *p, int wait)
{
    assert(!wait);
    if (q != commands || !queue_count) return 0;
    *(command_t *)p = queue[0]; --queue_count;
    memmove(queue, queue + 1, queue_count * sizeof(*queue)); return pdTRUE;
}
static int xQueueSend(int q, const command_t *c, int wait)
{ assert(q == commands && !wait); if (queue_count == 8) return 0; queue[queue_count++] = *c; return pdTRUE; }
static int xTaskCreate(void (*fn)(void *), const char *name, int stack, void *arg, int priority, void **task)
{ assert(fn && name && stack == 32768 && !arg && priority == 4); *task = &status; return pdPASS; }
static void vTaskDelay(int ms) { assert(ms == 10); }
static bool flash_gate_enter(int ms) { assert(ms == 1000); return true; }
static void flash_gate_leave(void) {}
static int nvs_open(const char *ns, int mode, nvs_handle_t *out)
{ assert(!strcmp(ns, NS) && mode == NVS_READWRITE); *out = 1; return ESP_OK; }
static int nvs_set_u8(nvs_handle_t h, const char *key, int value)
{ assert(h == 1 && key); (void)value; ++nvs_writes; return ESP_OK; }
#define nvs_set_u16 nvs_set_u8
static int nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t size)
{ assert(h == 1 && key && value && size); ++nvs_writes; return ESP_OK; }
static int nvs_commit(nvs_handle_t h) { assert(h == 1); return ESP_OK; }
static void nvs_close(nvs_handle_t h) { assert(h == 1); }
static bool save_enabled(bool enabled) { (void)enabled; ++nvs_writes; return true; }
static bool xiaozhi_wake_init(int word, const char *phrase) { (void)word; (void)phrase; return true; }
static void xiaozhi_wake_free(void) {}
static bool connect_session(void) { ++connects; ws = &stream; ready = true; return true; }
static void close_dialog(const char *reason)
{
    ++closes; ws = NULL; ready = want_listen = auto_listen = false;
    status.listening = false; xiaozhi_session_reset(&stream, false);
    end_dialog(reason); conversation(false); set_state(status.enabled ? "ready" : "disabled");
}
void xiaozhi_duplex_flush(void) {}
void xiaozhi_duplex_stats(xiaozhi_duplex_stats_t *out) { *out = (xiaozhi_duplex_stats_t){0}; }
bool xiaozhi_duplex_play(const int16_t *samples, int count)
{ assert(samples == pcm && count == 960); ++queued_audio; return true; }
bool xiaozhi_duplex_take(xiaozhi_duplex_frame_t *frame) { assert(frame == duplex_frame); return false; }
esp_err_t xiaozhi_duplex_stop(void) { return ESP_OK; }
static void xiaozhi_audio_close(void) {}
static int xiaozhi_audio_open(int volume) { assert(volume >= 0); return ESP_OK; }
static int xiaozhi_audio_mic(bool enable) { (void)enable; return ESP_OK; }
static int xiaozhi_audio_write(int16_t *samples, int count)
{ assert(samples == pcm && count == 960); ++audio_writes; return output_failure ? ESP_FAIL : ESP_OK; }
static int xiaozhi_audio_read(int16_t *samples, int count)
{
    assert(samples == pcm && count == 960);
    for (int i = 0; i < count; ++i) { samples[2*i] = pcm_silent ? 0 : (i & 1) ? 500 : -500; samples[2*i+1] = 0; }
    return ESP_OK;
}
vad_state_t vad_process(vad_handle_t handle, int16_t *samples, int rate, int duration)
{ assert(handle == manual_vad && samples && rate == 16000 && duration == 30); return capture_speech ? VAD_SPEECH : VAD_SILENCE; }
uint32_t xiaozhi_spectrum_generation(xiaozhi_spectrum_source_t source) { (void)source; return 1; }
void xiaozhi_spectrum_clear(xiaozhi_spectrum_source_t source) { (void)source; }
void xiaozhi_spectrum_feed(xiaozhi_spectrum_source_t source, const int16_t *samples, size_t count, uint32_t generation)
{ (void)source; assert(samples && count == 960 && generation == 1); }
static void opus_decoder_ctl(int instance, int mode) { assert(instance == decoder && mode == OPUS_RESET_STATE); }
static int opus_decode(int instance, const uint8_t *data, int bytes, int16_t *samples, int count, int fec)
{
    assert(instance == decoder && data && bytes == 1 && count == 1920 && !fec);
    for (int i = 0; i < 960; ++i) samples[i] = pcm_silent ? 0 : (i & 1) ? 500 : -500;
    return 960;
}
static int opus_encode(int instance, const int16_t *samples, int count, uint8_t *out, int maximum)
{ assert(instance == encoder && samples && count == 960 && maximum == 1500); out[0] = 1; return 1; }
static bool send_packet(const void *data, int size, bool binary) { assert(data && size == 5 && binary); return true; }
static void receive_mcp(const cJSON *j) { assert(j); ++mcp_messages; }
static void pair_step(void) {}
static void finish_connection(int64_t time) { assert(time == now); }
static bool connection_can_send(void) { return true; }
static bool send_text(const char *text) { assert(text); return true; }
static void change_capture(void) { status.listening = want_listen; }
static void update_audio_status(void) {}
static void poll_wake(void) { longjmp(worker_return, 1); }
#include "xiaozhi_dialog_functions.h"

static void command(const char *action, uint32_t sequence)
{
    command_t c = {.value = (int)sequence}; strlcpy(c.action, action, sizeof(c.action));
    if (!strcmp(action, "start_turn")) assert(prepare_turn_start(&c));
    handle_command(&c);
}
static void packet(const char *type, const char *phase, const char *text)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "type", type);
    if (phase) cJSON_AddStringToObject(j, "state", phase);
    if (text) cJSON_AddStringToObject(j, "text", text);
    char *body = cJSON_PrintUnformatted(j); assert(body); cJSON_Delete(j);
    packet_t p = {.opcode = 1, .len = strlen(body), .data = (uint8_t *)body}; receive_packet(&p); free(body);
}
static void open_manual(void)
{ command("dialog_turn", status.dialog_seq); assert(status.dialog_active && !status.dialog_voice_opened); }
static void start_voice(void)
{ show_dialog(true); ws = &stream; ready = want_listen = status.listening = auto_listen = true; xiaozhi_session_reset(&stream, true); }
static void loop_once(void) { if (!setjmp(worker_return)) worker(NULL); }
static void farewell(void)
{
    start_voice(); uint32_t seq = status.dialog_seq;
    xiaozhi_session_tts_start(&stream); packet("stt", NULL, "好的，再见！");
    assert(closes == 1 && !ws && !status.dialog_active && !status.conversation);
    assert(status.dialog_end_seq == seq && !strcmp(status.dialog_end_reason, "farewell"));
    assert(!strcmp(status.state, "ready") && !status.connection_errors);
    packet("stt", NULL, "再见"); assert(closes == 1);
    start_voice(); assert(status.dialog_active && status.dialog_seq == seq + 1);
    command("dismiss", seq); assert(status.dialog_active && closes == 1);
}
static void noncommands(void)
{
    const char *text[] = {"不要说再见", "再见是什么意思", "他说拜拜", "“再见”", "What does bye bye mean?"};
    start_voice(); now += 25000000; uint32_t activity = xiaozhi_activity_revision();
    packet("tts", "start", "再见"); assert(status.dialog_active && !closes);
    for (unsigned i = 0; i < sizeof(text)/sizeof(text[0]); ++i) packet("stt", NULL, text[i]);
    packet("tts", "stop", "Bye bye"); packet("mcp", NULL, NULL);
    assert(status.dialog_active && !closes && mcp_messages == 1);
    assert(xiaozhi_activity_revision() == activity && xiaozhi_activity_remaining_ms(status.dialog_seq) == 5000);
}
static void commands_test(void)
{
    strcpy(status.state, "connection_error"); strcpy(status.recognized, "bye bye");
    strcpy(status.reply, "old reply"); strcpy(status.emotion, "sad");
    strcpy(status.connection_end, "transport_error"); status.connection_errors = 1;
    open_manual(); assert(!ws && !connects && !want_listen);
    assert(!strcmp(status.state, "ready") && !status.recognized[0] && !status.reply[0] && !status.emotion[0]);
    assert(!strcmp(status.connection_end, "transport_error") && status.connection_errors == 1);
    uint32_t old = status.dialog_seq; command("start_turn", old); assert(want_listen && connects == 1);
    command("stop_turn", old); assert(!want_listen);
    command("dismiss", old); assert(!status.dialog_active);
    start_voice(); uint32_t current = status.dialog_seq; unsigned ended = closes;
    command("dialog_turn", old); command("dialog_turn", current);
    assert(status.dialog_seq == current && status.dialog_voice_opened && closes == ended);
    command("stop_turn", old); assert(want_listen);
    command("dismiss", old); assert(status.dialog_active && closes == ended);
    command("timeout", 300); assert(status.dialog_timeout == 30 && !nvs_writes);
    command("disable", 0); assert(!status.dialog_active && status.dialog_seq == current);
    command("enable", 0); assert(status.dialog_seq == current);
    command("dialog_turn", current); assert(status.dialog_seq == current + 1);
    command("reset", 0); assert(!status.dialog_active && status.dialog_seq == current + 1);
    status.enabled = true; status.dialog_seq = UINT32_MAX;
    command("dialog_turn", UINT32_MAX); assert(status.dialog_seq == 0 && status.dialog_active);
    command("dismiss", UINT32_MAX); assert(status.dialog_active);
    command("dismiss", 0); assert(!status.dialog_active);
}
static void audio(void)
{
    open_manual(); ready = true;
    uint8_t data[] = {0, 0, 0, 1, 0}; packet_t p = {.opcode = 2, .len = sizeof(data), .data = data};
    now += 25000000; uint32_t activity = xiaozhi_activity_revision();
    pcm_silent = true; receive_packet(&p); assert(audio_writes == 1 && activity == xiaozhi_activity_revision());
    pcm_silent = false; status.volume = 0; receive_packet(&p); assert(activity == xiaozhi_activity_revision());
    status.volume = 50; output_failure = true; receive_packet(&p); assert(activity == xiaozhi_activity_revision());
    output_failure = failed = false; receive_packet(&p); assert(xiaozhi_activity_remaining_ms(status.dialog_seq) == 30000);
    for (int i = 0; i < 1100; ++i) { now += 60000; receive_packet(&p); assert(!xiaozhi_activity_expire(status.dialog_seq)); }
    xiaozhi_session_reset(&stream, true); now += 25000000; activity = xiaozhi_activity_revision();
    receive_packet(&p); assert(queued_audio == 1 && activity == xiaozhi_activity_revision());
}
static void capture(void)
{
    open_manual(); now += 25000000; uint32_t activity = xiaozhi_activity_revision();
    pcm_silent = false; capture_speech = false; assert(send_capture()); assert(activity == xiaozhi_activity_revision());
    pcm_silent = true; capture_speech = true; assert(send_capture()); assert(activity == xiaozhi_activity_revision());
    pcm_silent = false; assert(send_capture()); assert(xiaozhi_activity_remaining_ms(status.dialog_seq) == 30000);
    now += 25000000; activity = xiaozhi_activity_revision();
    xiaozhi_session_reset(&stream, true); assert(!send_capture()); assert(activity == xiaozhi_activity_revision());
}
static void timeout_test(void)
{
    open_manual(); now += 29999000; loop_once(); assert(status.dialog_active);
    now += 1000; loop_once(); assert(!status.dialog_active && !strcmp(status.dialog_end_reason, "silence"));
    uint32_t old = status.dialog_seq; start_voice(); command("stop_turn", old);
    assert(status.dialog_active && want_listen);
    now += 25000000; xiaozhi_activity_note(status.dialog_seq, now); loop_once();
    assert(status.dialog_active && xiaozhi_activity_remaining_ms(status.dialog_seq) == 30000);
}
static esp_err_t enqueue(const char *action, uint32_t sequence)
{ command_t c = {.value = (int)sequence}; strlcpy(c.action, action, sizeof(c.action)); return enqueue_command(&c); }
static void dismiss_error(void)
{
    const char *states[] = {"connection_error", "audio_or_connection_error", "pairing", "storage_error", "disabled"};
    strcpy(status.connection_end, "transport_error"); status.connection_errors = 3; status.normal_closes = 2;
    status.connection_tls_error = -0x7880; status.connection_close_code = 1006;
    status.connection_error_type = 1; status.connection_socket_errno = 104; status.connection_http_status = 503;
    for (unsigned i = 0; i < sizeof(states)/sizeof(states[0]); ++i) {
        status.enabled = true; open_manual();
        uint32_t seq = status.dialog_seq;
        end_dialog("error"); strcpy(status.state, states[i]);
        pairing = i == 2; pair_deadline = next_pair = now + 1000000;
        if (i == 4) status.enabled = false;
        unsigned prior_closes = closes;
        assert(enqueue("dismiss", seq - 1) == ESP_OK); loop_once();
        assert(!strcmp(status.dialog_end_reason, "error") && !strcmp(status.state, states[i]));
        assert(enqueue("dismiss", seq) == ESP_OK); loop_once();
        assert(!status.dialog_active && status.dialog_seq == seq && status.dialog_end_seq == seq);
        assert(!strcmp(status.dialog_end_reason, "manual"));
        assert(!strcmp(status.state, i < 2 ? "ready" : states[i]));
        assert(pairing == (i == 2) && closes == prior_closes && !connects && !ws);
        assert(!xiaozhi_activity_remaining_ms(seq));
        assert(status.connection_errors == 3 && status.normal_closes == 2 && !strcmp(status.connection_end, "transport_error"));
        assert(status.connection_tls_error == -0x7880 && status.connection_close_code == 1006);
        assert(status.connection_error_type == 1 && status.connection_socket_errno == 104 && status.connection_http_status == 503);
        assert(enqueue("dismiss", seq) == ESP_OK); loop_once();
        assert(closes == prior_closes && !strcmp(status.dialog_end_reason, "manual"));
        pairing = false;
    }
    status.enabled = true; open_manual();
    uint32_t old = status.dialog_seq; end_dialog("error"); strcpy(status.state, "connection_error");
    assert(enqueue("dismiss", old) == ESP_OK);
    start_voice(); uint32_t current = status.dialog_seq;
    loop_once();
    assert(status.dialog_active && status.dialog_seq == current && want_listen && ws);
    assert(status.connection_errors == 3 && !strcmp(status.connection_end, "transport_error"));
}
static void fill_queue(void)
{ for (unsigned i = 0; i < 8; ++i) assert(enqueue("timeout", 30) == ESP_OK); }
static void queue_test(void)
{
    open_manual(); uint32_t seq = status.dialog_seq;
    command("start_turn", seq); fill_queue();
    assert(enqueue("dismiss", seq) == ESP_OK); /* A destroyed UI cannot retry. */
    assert(enqueue("start_turn", seq) == ESP_ERR_TIMEOUT);
    loop_once(); assert(!status.dialog_active && !want_listen && !queue_count);

    open_manual(); seq = status.dialog_seq;
    assert(enqueue("dismiss", seq) == ESP_OK);
    assert(enqueue("dialog_turn", seq) == ESP_OK); /* Immediate close and reopen needs no extra click. */
    loop_once(); assert(status.dialog_active && status.dialog_seq == seq + 1 && !status.dialog_voice_opened);
    command("dismiss", status.dialog_seq);

    open_manual(); seq = status.dialog_seq; command("start_turn", seq);
    command_t delayed_start = {.action = "start_turn", .value = (int)seq};
    assert(prepare_turn_start(&delayed_start)); /* Producer is descheduled before queue send. */
    fill_queue(); assert(enqueue("stop_turn", seq) == ESP_OK);
    assert(enqueue("stop_turn", seq - 1) == ESP_OK); /* Old release cannot replace current pending. */
    assert(enqueue("start_turn", seq) == ESP_ERR_TIMEOUT);
    loop_once(); assert(status.dialog_active && !want_listen);
    handle_command(&delayed_start); assert(!want_listen); /* Old producer resumes after stop drained. */
    assert(enqueue("start_turn", seq) == ESP_OK);
    loop_once(); assert(want_listen); /* A fresh retry after the drain is ordered after stop. */

    delayed_start = (command_t){.action = "start_turn", .value = (int)seq};
    assert(prepare_turn_start(&delayed_start));
    assert(enqueue("stop_turn", seq) == ESP_OK && !queue_count); /* Empty queue follows exactly the same ordering. */
    loop_once(); assert(!want_listen);
    handle_command(&delayed_start); assert(!want_listen);
    assert(enqueue("start_turn", seq) == ESP_OK); loop_once(); assert(want_listen);
    fill_queue();
    assert(enqueue("stop_turn", seq + 100000) == ESP_OK); /* A future sequence cannot poison cancellation. */
    assert(enqueue("dismiss", seq + 100000) == ESP_OK);
    assert(enqueue("stop_turn", seq) == ESP_OK);
    loop_once(); assert(!want_listen && status.dialog_active);
    assert(enqueue("start_turn", seq) == ESP_OK); loop_once(); assert(want_listen);

    fill_queue(); assert(enqueue("dismiss", seq) == ESP_OK);
    start_voice(); uint32_t current = status.dialog_seq;
    loop_once(); assert(status.dialog_active && status.dialog_seq == current && want_listen);
    fill_queue(); assert(enqueue("stop_turn", current) == ESP_OK);
    assert(enqueue("dismiss", seq) == ESP_OK);
    loop_once(); assert(status.dialog_active && status.dialog_seq == current && !want_listen);
}
int main(int argc, char **argv)
{
    assert(argc == 2); status.enabled = status.paired = true; status.volume = 50; status.dialog_timeout = 30; now = 1000000;
    if (!strcmp(argv[1], "farewell")) farewell();
    else if (!strcmp(argv[1], "noncommands")) noncommands();
    else if (!strcmp(argv[1], "commands")) commands_test();
    else if (!strcmp(argv[1], "audio")) audio();
    else if (!strcmp(argv[1], "capture")) capture();
    else if (!strcmp(argv[1], "timeout")) timeout_test();
    else if (!strcmp(argv[1], "queue")) queue_test();
    else if (!strcmp(argv[1], "dismiss-error")) dismiss_error();
    else assert(false);
    printf("AI Assistant production policy passed: %s\n", argv[1]);
}
