/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void checked_free(void *p);
#define free checked_free
#include "../../main/xiaozhi/xiaozhi_duplex.c"
#undef free

struct mock_task { void (*fn)(void *); bool exited; } tasks[2];
static unsigned tasks_created, fail_task, allocations, fail_allocation, live_allocations;
static int live_aec, live_vad, chunk_size = 256, reads, read_limit, source_sample, writes, flush_write;
static bool join_workers, fail_read, fail_write;
static TickType_t ticks = 100;
static jmp_buf worker_return;
static aec_handle_t aec_instance;
static vad_handle_with_trigger_t vad_instance;
static uint32_t spectrum_generation[XZ_SPECTRUM_SOURCES];
static unsigned spectrum_feeds[XZ_SPECTRUM_SOURCES];
uint32_t xiaozhi_spectrum_generation(xiaozhi_spectrum_source_t source) { return spectrum_generation[source]; }
void xiaozhi_spectrum_clear(xiaozhi_spectrum_source_t source) { ++spectrum_generation[source]; }
void xiaozhi_spectrum_feed(xiaozhi_spectrum_source_t source, const int16_t *pcm, size_t count, uint32_t generation)
{
    assert(generation == spectrum_generation[source] && count > 0);
    if (source == XZ_SPECTRUM_MIC) {
        int start = source_sample - (int)count;
        for (unsigned i = 0; i < count; ++i) assert(pcm[i] == 700 + start + (int)i - (start + (int)i) % 7);
    } else assert(!fail_write && pcm[0] == 123);
    ++spectrum_feeds[source];
}

static void checked_free(void *p) { if (p) { assert(live_allocations); --live_allocations; free(p); } }
void *heap_caps_malloc(size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (++allocations == fail_allocation) return NULL;
    void *p = malloc(size); assert(p); ++live_allocations; return p;
}
void *heap_caps_calloc(size_t n, size_t size, unsigned caps)
{ void *p = heap_caps_malloc(n * size, caps); if (p) memset(p, 0, n * size); return p; }
void *heap_caps_aligned_alloc(size_t alignment, size_t size, unsigned caps)
{
    assert(alignment == 16);
    void *p = heap_caps_malloc(size, caps);
    assert(!p || (uintptr_t)p % alignment == 0);
    return p;
}
aec_handle_t *aec_create_from_config(aec_config_t *cfg)
{
    assert(!live_aec && cfg->mic_num == 1 && cfg->ref_num == 1 && cfg->out_num == 1);
    assert(cfg->sample_rate == 16000 && cfg->mode == AEC_MODE_FD_LOW_COST);
    assert(cfg->nlp_level == AEC_NLP_LEVEL_VERYAGGR && cfg->caps == PCM_CAPS);
    ++live_aec; return &aec_instance;
}
int aec_get_chunksize(const aec_handle_t *handle) { assert(handle == &aec_instance); return chunk_size; }
void aec_process(const aec_handle_t *handle, int16_t *mic, int16_t *ref, int16_t *out)
{
    assert(handle == &aec_instance && live_aec);
    assert((uintptr_t)mic % 16 == 0 && (uintptr_t)ref % 16 == 0 && (uintptr_t)out % 16 == 0);
    for (int i = 0; i < chunk_size; ++i) {
        int index = source_sample - chunk_size + i;
        assert(mic[i] == 1000 + index && ref[i] == 300 + index % 7);
        out[i] = mic[i] - ref[i];
    }
}
void aec_destroy(aec_handle_t *handle) { assert(handle == &aec_instance && live_aec); --live_aec; }
vad_handle_t vad_create(vad_mode_t mode)
{ assert(mode == VAD_MODE_3 && !live_vad); ++live_vad; return &vad_instance; }
void vad_destroy(vad_handle_t handle) { assert(handle == &vad_instance && live_vad); --live_vad; }
vad_state_t vad_process(vad_handle_t handle, int16_t *pcm, int rate, int ms)
{ assert(handle == &vad_instance && rate == 16000 && ms == 30); return pcm[0] > 0 ? VAD_SPEECH : VAD_SILENCE; }
int64_t esp_timer_get_time(void) { return ticks++ * 1000; }
TickType_t xTaskGetTickCount(void) { return ticks; }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t handle) { assert(!handle); return 3072; }
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *out)
{
    (void)name; assert(stack >= 4096 && !arg && priority == 5 && tasks_created < 2);
    unsigned index = tasks_created++;
    if (tasks_created == fail_task) { *out = NULL; return 0; }
    tasks[index] = (struct mock_task){.fn = fn}; *out = &tasks[index]; return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { assert(!task); longjmp(worker_return, 1); }
static void run_worker(unsigned index)
{
    assert(tasks[index].fn && !tasks[index].exited);
    if (!setjmp(worker_return)) tasks[index].fn(NULL);
    tasks[index].exited = true;
}
QueueHandle_t xQueueCreateStatic(unsigned capacity, unsigned size, uint8_t *storage, StaticQueue_t *control)
{ *control = (StaticQueue_t){.capacity = capacity, .size = size, .storage = storage}; return control; }
int xQueueSend(QueueHandle_t q, const void *data, TickType_t delay)
{
    assert(!delay); if (q->count == q->capacity) return 0;
    memcpy(q->storage + ((q->head + q->count) % q->capacity) * q->size, data, q->size);
    ++q->count; return pdTRUE;
}
int xQueueReceive(QueueHandle_t q, void *data, TickType_t delay)
{
    if (!q->count) { if (delay) atomic_store(&stop_requested, true); return 0; }
    memcpy(data, q->storage + q->head * q->size, q->size);
    q->head = (q->head + 1) % q->capacity; --q->count; return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t q) { return q->count; }
void xQueueReset(QueueHandle_t q) { q->head = q->count = 0; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *s) { s->count = 0; return s; }
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t delay)
{
    (void)delay;
    if (!s->count && join_workers) {
        unsigned index = s == capture_done ? 0 : 1;
        if (tasks[index].fn && !tasks[index].exited) run_worker(index);
    }
    if (!s->count) return 0;
    --s->count; return pdTRUE;
}
void xSemaphoreGive(SemaphoreHandle_t s) { assert(!s->count); s->count = 1; }
void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
esp_err_t xiaozhi_audio_read(int16_t *pcm, int count)
{
    assert(count == chunk_size);
    if (fail_read) return ESP_FAIL;
    if (reads++ == read_limit) { atomic_store(&stop_requested, true); return ESP_ERR_TIMEOUT; }
    for (int i = 0; i < count; ++i, ++source_sample) {
        pcm[2 * i] = 1000 + source_sample;
        pcm[2 * i + 1] = 300 + source_sample % 7;
    }
    return ESP_OK;
}
esp_err_t xiaozhi_audio_write(int16_t *pcm, int count)
{
    assert(count > 0 && count <= 160 && pcm[0] == 123);
    ++writes;
    if (writes == flush_write) xiaozhi_duplex_flush();
    return fail_write ? ESP_FAIL : ESP_OK;
}

static void reset_trial(void)
{
    assert(!live_allocations && !live_aec && !live_vad && !capture_task && !play_task);
    memset(tasks, 0, sizeof(tasks));
    tasks_created = fail_task = allocations = fail_allocation = 0;
    reads = source_sample = writes = flush_write = 0;
    read_limit = 4; fail_read = fail_write = false; join_workers = true;
    memset(spectrum_feeds, 0, sizeof(spectrum_feeds));
}
int main(void)
{
    xiaozhi_duplex_frame_t received;
    xiaozhi_duplex_stats_t stats;
    reset_trial(); assert(xiaozhi_duplex_start() == ESP_OK);
    assert(xiaozhi_duplex_start() == ESP_ERR_INVALID_STATE);
    run_worker(0);
    assert(spectrum_feeds[XZ_SPECTRUM_MIC] == 4 && !spectrum_feeds[XZ_SPECTRUM_SPEAKER]);
    assert(xiaozhi_duplex_take(&received) && received.speech && !received.playback && received.sequence == 1);
    for (int i = 0; i < 960; ++i) assert(received.pcm[i] == 700 + i - i % 7);
    assert(!xiaozhi_duplex_take(&received));
    xiaozhi_duplex_stats(&stats); assert(stats.captured_frames == 1 && !stats.failed);
    assert(xiaozhi_duplex_stop() == ESP_OK && xiaozhi_duplex_stop() == ESP_OK);

    reset_trial(); read_limit = 60;
    assert(xiaozhi_duplex_start() == ESP_OK);
    int16_t reply[1920]; for (int i = 0; i < 1920; ++i) reply[i] = 123;
    assert(!xiaozhi_duplex_play(reply, 1921) && xiaozhi_duplex_play(reply, 1920));
    run_worker(0);
    xiaozhi_duplex_stats(&stats);
    assert(stats.captured_frames == 16 && stats.dropped_capture_frames == 4 && stats.captured_during_playback == 16);
    assert(xiaozhi_duplex_take(&received) && received.playback && received.sequence == 5);
    assert(stats.ref_peak_max_playback == 306);
    assert(received.pcm[0] == 700 + 3840 - 3840 % 7);
    assert(xiaozhi_duplex_stop() == ESP_OK);

    reset_trial(); flush_write = 2;
    assert(xiaozhi_duplex_start() == ESP_OK && xiaozhi_duplex_play(reply, 1920));
    run_worker(1); assert(writes == 2); /* In-flight invalidation, not just queue clearing. */
    assert(spectrum_feeds[XZ_SPECTRUM_SPEAKER] == 1); /* A cancelled in-flight write cannot reappear. */
    assert(xiaozhi_duplex_stop() == ESP_OK);

    reset_trial(); fail_write = true;
    assert(xiaozhi_duplex_start() == ESP_OK && xiaozhi_duplex_play(reply, 960));
    run_worker(1); xiaozhi_duplex_stats(&stats); assert(stats.failed && writes == 1);
    assert(!spectrum_feeds[XZ_SPECTRUM_SPEAKER]);
    assert(xiaozhi_duplex_stop() == ESP_OK);

    reset_trial(); fail_read = true;
    assert(xiaozhi_duplex_start() == ESP_OK);
    run_worker(0); xiaozhi_duplex_stats(&stats); assert(stats.failed && !stats.captured_frames);
    assert(xiaozhi_duplex_stop() == ESP_OK);

    reset_trial(); join_workers = false;
    assert(xiaozhi_duplex_start() == ESP_OK);
    assert(xiaozhi_duplex_stop() == ESP_ERR_TIMEOUT && live_aec && live_allocations);
    assert(xiaozhi_duplex_start() == ESP_ERR_INVALID_STATE && !xiaozhi_duplex_play(reply, 960));
    join_workers = true;
    assert(xiaozhi_duplex_stop() == ESP_OK);
    for (unsigned failure = 1; failure <= 10; ++failure) {
        reset_trial(); fail_allocation = failure;
        assert(xiaozhi_duplex_start() == ESP_ERR_NO_MEM);
        assert(!live_allocations && !live_aec && !live_vad);
    }
    for (unsigned failure = 1; failure <= 2; ++failure) {
        reset_trial(); fail_task = failure;
        assert(xiaozhi_duplex_start() == ESP_ERR_NO_MEM);
        assert(!live_allocations && !live_aec && !live_vad);
    }
    reset_trial(); chunk_size = 1921;
    assert(xiaozhi_duplex_start() == ESP_ERR_NO_MEM && !live_aec && !live_vad);
    puts("Xiaozhi duplex AEC routing, framing, backpressure, cancellation and safe teardown passed");
}
