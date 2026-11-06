/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
 * Device AEC follows betamoojw/xiaozhi-esp32 (a73e9a7e), using the pinned
 * ESP-SR direct full-duplex AEC API instead of its combined AFE/WakeNet graph.
 */
#include "xiaozhi_duplex.h"
#include "xiaozhi_audio.h"
#include "xiaozhi_spectrum.h"
#include "xiaozhi_activity.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_aec.h"
#include "esp_vad.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define CAPTURE_QUEUE_LENGTH 12
#define PLAY_QUEUE_LENGTH 24
#define PLAY_CHUNK_SAMPLES 160
#define STOP_TIMEOUT_MS 3000
#define PCM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

typedef struct {
    uint32_t generation;
    int samples;
    int16_t pcm[XIAOZHI_DUPLEX_PLAY_SAMPLES];
} playback_frame_t;

static aec_handle_t *aec;
static vad_handle_t vad;
static int aec_samples;
static int16_t *stereo, *microphone, *reference, *clean;
static xiaozhi_duplex_frame_t *capture_frame, *discard_frame;
static playback_frame_t *play_frame, *submit_frame;
static uint8_t *capture_storage, *play_storage;
static StaticQueue_t capture_control, play_control;
static QueueHandle_t capture_queue, play_queue;
static StaticSemaphore_t capture_done_control, play_done_control;
static SemaphoreHandle_t capture_done, play_done;
static TaskHandle_t capture_task, play_task;
static atomic_bool stop_requested, pipeline_failed;
static atomic_uint output_generation;
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
static xiaozhi_duplex_stats_t diagnostics;
static TickType_t output_tail;
static bool output_busy;

static bool playback_active(void)
{
    TickType_t now = xTaskGetTickCount();
    portENTER_CRITICAL(&stats_lock);
    bool active = output_busy || (output_tail && (int32_t)(output_tail - now) > 0);
    portEXIT_CRITICAL(&stats_lock);
    return active || (play_queue && uxQueueMessagesWaiting(play_queue));
}

void xiaozhi_duplex_stats(xiaozhi_duplex_stats_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&stats_lock);
    *out = diagnostics;
    portEXIT_CRITICAL(&stats_lock);
    out->failed = atomic_load(&pipeline_failed);
    /* This API can also be called by an HTTP task during teardown; the queue
     * itself is accessed only by the session owner and the audio workers. */
    portENTER_CRITICAL(&stats_lock);
    out->playing = output_busy || (output_tail && (int32_t)(output_tail - xTaskGetTickCount()) > 0);
    portEXIT_CRITICAL(&stats_lock);
}

static int peak(int16_t value)
{
    return value < 0 ? -(int)value : value;
}

static void capture_worker(void *unused)
{
    (void)unused;
    int used = 0;
    uint32_t sequence = 0;
    bool overlap = false;
    while (!atomic_load(&stop_requested) && !atomic_load(&pipeline_failed)) {
        uint32_t activity_generation = xiaozhi_activity_generation();
        uint32_t spectrum_generation = xiaozhi_spectrum_generation(XZ_SPECTRUM_MIC);
        if (xiaozhi_audio_read(stereo, aec_samples) != ESP_OK) {
            if (!atomic_load(&stop_requested)) atomic_store(&pipeline_failed, true);
            break;
        }
        if (atomic_load(&stop_requested)) break;
        int mic_peak = 0, ref_peak = 0, clean_peak = 0;
        for (int i = 0; i < aec_samples; ++i) {
            microphone[i] = stereo[2 * i];
            reference[i] = stereo[2 * i + 1];
            int m = peak(microphone[i]), r = peak(reference[i]);
            if (m > mic_peak) mic_peak = m;
            if (r > ref_peak) ref_peak = r;
        }
        int64_t before = esp_timer_get_time();
        aec_process(aec, microphone, reference, clean);
        int elapsed = (int)(esp_timer_get_time() - before);
        xiaozhi_spectrum_feed(XZ_SPECTRUM_MIC, clean, aec_samples, spectrum_generation);
        bool playing = playback_active();
        for (int i = 0; i < aec_samples; ++i) {
            int c = peak(clean[i]);
            if (c > clean_peak) clean_peak = c;
            capture_frame->pcm[used++] = clean[i];
            overlap |= playing;
            if (used == XIAOZHI_DUPLEX_SAMPLES) {
                capture_frame->speech = false;
                for (int offset = 0; offset < XIAOZHI_DUPLEX_SAMPLES; offset += 480)
                    capture_frame->speech |= vad_process(vad, capture_frame->pcm + offset, 16000, 30) == VAD_SPEECH;
                /* Timestamp at observation, not later when the network worker
                 * consumes this frame. VAD alone never interrupts playback. */
                if (capture_frame->speech && xiaozhi_activity_has_sound(capture_frame->pcm, XIAOZHI_DUPLEX_SAMPLES))
                    xiaozhi_activity_note(activity_generation, esp_timer_get_time());
                capture_frame->playback = overlap;
                capture_frame->sequence = ++sequence;
                if (xQueueSend(capture_queue, capture_frame, 0) != pdTRUE) {
                    /* Keep current speech when a bounded network/MCP operation
                     * stalls the session worker. Never stall hardware capture. */
                    if (xQueueReceive(capture_queue, discard_frame, 0) == pdTRUE) {
                        portENTER_CRITICAL(&stats_lock);
                        ++diagnostics.dropped_capture_frames;
                        portEXIT_CRITICAL(&stats_lock);
                    }
                    if (xQueueSend(capture_queue, capture_frame, 0) != pdTRUE)
                        atomic_store(&pipeline_failed, true);
                }
                portENTER_CRITICAL(&stats_lock);
                ++diagnostics.captured_frames;
                if (overlap) ++diagnostics.captured_during_playback;
                portEXIT_CRITICAL(&stats_lock);
                used = 0;
                overlap = false;
            }
        }
        uint32_t stack_free = uxTaskGetStackHighWaterMark(NULL);
        portENTER_CRITICAL(&stats_lock);
        diagnostics.mic_peak = mic_peak;
        diagnostics.ref_peak = ref_peak;
        if (playing && ref_peak > diagnostics.ref_peak_max_playback)
            diagnostics.ref_peak_max_playback = ref_peak;
        diagnostics.clean_peak = clean_peak;
        diagnostics.processing_us = elapsed;
        diagnostics.capture_stack_free = stack_free;
        portEXIT_CRITICAL(&stats_lock);
    }
    xSemaphoreGive(capture_done);
    vTaskDelete(NULL);
}

static void playback_worker(void *unused)
{
    (void)unused;
    while (!atomic_load(&stop_requested) && !atomic_load(&pipeline_failed)) {
        if (xQueueReceive(play_queue, play_frame, pdMS_TO_TICKS(20)) != pdTRUE) continue;
        if (play_frame->generation != atomic_load(&output_generation)) continue;
        bool complete = true;
        for (int offset = 0; offset < play_frame->samples; offset += PLAY_CHUNK_SAMPLES) {
            if (atomic_load(&stop_requested) || play_frame->generation != atomic_load(&output_generation)) {
                complete = false;
                break;
            }
            int count = play_frame->samples - offset;
            if (count > PLAY_CHUNK_SAMPLES) count = PLAY_CHUNK_SAMPLES;
            portENTER_CRITICAL(&stats_lock);
            output_busy = true;
            portEXIT_CRITICAL(&stats_lock);
            uint32_t spectrum_generation = xiaozhi_spectrum_generation(XZ_SPECTRUM_SPEAKER);
            uint32_t activity_generation = xiaozhi_activity_generation();
            esp_err_t err = xiaozhi_audio_write(play_frame->pcm + offset, count);
            if (err == ESP_OK && play_frame->generation == atomic_load(&output_generation) && !atomic_load(&stop_requested)) {
                xiaozhi_spectrum_feed(XZ_SPECTRUM_SPEAKER, play_frame->pcm + offset, count, spectrum_generation);
                if (xiaozhi_audio_output_audible() && xiaozhi_activity_has_sound(play_frame->pcm + offset, count))
                    xiaozhi_activity_note_playback(activity_generation, esp_timer_get_time());
            }
            portENTER_CRITICAL(&stats_lock);
            output_busy = false;
            /* Includes the BSP's queued DMA frames after a successful write. */
            output_tail = xTaskGetTickCount() + pdMS_TO_TICKS(XIAOZHI_OUTPUT_TAIL_MS);
            portEXIT_CRITICAL(&stats_lock);
            if (err != ESP_OK) {
                if (!atomic_load(&stop_requested)) atomic_store(&pipeline_failed, true);
                complete = false;
                break;
            }
        }
        uint32_t stack_free = uxTaskGetStackHighWaterMark(NULL);
        portENTER_CRITICAL(&stats_lock);
        if (complete) ++diagnostics.played_frames;
        diagnostics.playback_stack_free = stack_free;
        portEXIT_CRITICAL(&stats_lock);
    }
    xSemaphoreGive(play_done);
    vTaskDelete(NULL);
}

void xiaozhi_duplex_flush(void)
{
    atomic_fetch_add(&output_generation, 1);
    xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
    if (play_queue) xQueueReset(play_queue);
}

static void release_resources(void)
{
    if (capture_queue) vQueueDelete(capture_queue);
    if (play_queue) vQueueDelete(play_queue);
    capture_queue = play_queue = NULL;
    if (capture_done) vSemaphoreDelete(capture_done);
    if (play_done) vSemaphoreDelete(play_done);
    capture_done = play_done = NULL;
    if (aec) aec_destroy(aec);
    if (vad) vad_destroy(vad);
    aec = NULL; vad = NULL;
    free(stereo); free(microphone); free(reference); free(clean);
    stereo = microphone = reference = clean = NULL;
    free(capture_frame); free(discard_frame); free(play_frame); free(submit_frame);
    capture_frame = discard_frame = NULL;
    play_frame = submit_frame = NULL;
    free(capture_storage); free(play_storage);
    capture_storage = play_storage = NULL;
}

esp_err_t xiaozhi_duplex_stop(void)
{
    atomic_store(&stop_requested, true);
    xiaozhi_spectrum_clear(XZ_SPECTRUM_MIC);
    xiaozhi_duplex_flush();
    TickType_t started = xTaskGetTickCount();
    if (capture_task) {
        if (xSemaphoreTake(capture_done, pdMS_TO_TICKS(STOP_TIMEOUT_MS)) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        capture_task = NULL;
    }
    if (play_task) {
        TickType_t elapsed = xTaskGetTickCount() - started;
        TickType_t remaining = elapsed < pdMS_TO_TICKS(STOP_TIMEOUT_MS) ? pdMS_TO_TICKS(STOP_TIMEOUT_MS) - elapsed : 0;
        if (xSemaphoreTake(play_done, remaining) != pdTRUE) return ESP_ERR_TIMEOUT;
        play_task = NULL;
    }
    release_resources();
    portENTER_CRITICAL(&stats_lock);
    diagnostics.running = false;
    output_busy = false; output_tail = 0;
    portEXIT_CRITICAL(&stats_lock);
    return ESP_OK;
}

esp_err_t xiaozhi_duplex_start(void)
{
    if (capture_task || play_task || aec) return ESP_ERR_INVALID_STATE;
    xiaozhi_spectrum_clear(XZ_SPECTRUM_MIC);
    xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
    atomic_store(&stop_requested, false);
    atomic_store(&pipeline_failed, false);
    atomic_fetch_add(&output_generation, 1);
    portENTER_CRITICAL(&stats_lock);
    memset(&diagnostics, 0, sizeof(diagnostics));
    output_busy = false; output_tail = 0;
    portEXIT_CRITICAL(&stats_lock);
    aec_config_t cfg = {.mic_num = 1, .ref_num = 1, .out_num = 1,
        .filter_length = 4, .sample_rate = 16000, .caps = PCM_CAPS,
        .mode = AEC_MODE_FD_LOW_COST, .nlp_level = AEC_NLP_LEVEL_VERYAGGR};
    aec = aec_create_from_config(&cfg);
    vad = vad_create(VAD_MODE_3);
    aec_samples = aec ? aec_get_chunksize(aec) : 0;
    if (!aec || !vad || aec_samples <= 0 || aec_samples > XIAOZHI_DUPLEX_PLAY_SAMPLES) goto error;
    stereo = heap_caps_aligned_alloc(16, aec_samples * 2 * sizeof(int16_t), PCM_CAPS);
    microphone = heap_caps_aligned_alloc(16, aec_samples * sizeof(int16_t), PCM_CAPS);
    reference = heap_caps_aligned_alloc(16, aec_samples * sizeof(int16_t), PCM_CAPS);
    clean = heap_caps_aligned_alloc(16, aec_samples * sizeof(int16_t), PCM_CAPS);
    capture_frame = heap_caps_calloc(1, sizeof(*capture_frame), PCM_CAPS);
    discard_frame = heap_caps_malloc(sizeof(*discard_frame), PCM_CAPS);
    play_frame = heap_caps_malloc(sizeof(*play_frame), PCM_CAPS);
    submit_frame = heap_caps_malloc(sizeof(*submit_frame), PCM_CAPS);
    capture_storage = heap_caps_malloc(CAPTURE_QUEUE_LENGTH * sizeof(*capture_frame), PCM_CAPS);
    play_storage = heap_caps_malloc(PLAY_QUEUE_LENGTH * sizeof(*play_frame), PCM_CAPS);
    if (!stereo || !microphone || !reference || !clean || !capture_frame || !discard_frame ||
        !play_frame || !submit_frame || !capture_storage || !play_storage) goto error;
    capture_queue = xQueueCreateStatic(CAPTURE_QUEUE_LENGTH, sizeof(*capture_frame), capture_storage, &capture_control);
    play_queue = xQueueCreateStatic(PLAY_QUEUE_LENGTH, sizeof(*play_frame), play_storage, &play_control);
    capture_done = xSemaphoreCreateBinaryStatic(&capture_done_control);
    play_done = xSemaphoreCreateBinaryStatic(&play_done_control);
    if (!capture_queue || !play_queue || !capture_done || !play_done) goto error;
    if (xTaskCreate(capture_worker, "xz_capture", 8192, NULL, 5, &capture_task) != pdPASS) goto error;
    if (xTaskCreate(playback_worker, "xz_playback", 4096, NULL, 5, &play_task) != pdPASS) goto error;
    portENTER_CRITICAL(&stats_lock);
    diagnostics.running = true;
    portEXIT_CRITICAL(&stats_lock);
    return ESP_OK;
error:
    atomic_store(&pipeline_failed, true);
    return xiaozhi_duplex_stop() == ESP_OK ? ESP_ERR_NO_MEM : ESP_ERR_TIMEOUT;
}

bool xiaozhi_duplex_take(xiaozhi_duplex_frame_t *frame)
{
    return frame && capture_queue && xQueueReceive(capture_queue, frame, 0) == pdTRUE;
}

bool xiaozhi_duplex_play(const int16_t *pcm, int samples)
{
    if (!pcm || samples <= 0 || samples > XIAOZHI_DUPLEX_PLAY_SAMPLES || !play_queue ||
        atomic_load(&stop_requested) || atomic_load(&pipeline_failed)) return false;
    submit_frame->generation = atomic_load(&output_generation);
    submit_frame->samples = samples;
    memcpy(submit_frame->pcm, pcm, samples * sizeof(*pcm));
    return xQueueSend(play_queue, submit_frame, 0) == pdTRUE;
}
