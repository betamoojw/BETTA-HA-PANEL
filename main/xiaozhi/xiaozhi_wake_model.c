/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_wake_model.h"
#include "xiaozhi_wake_words.h"
#include "esp_heap_caps.h"
#include "zlib.h"
#include <stdint.h>
#include <stdbool.h>

#define MODEL(n) \
    extern const uint8_t n##_start[] asm("_binary_" #n "_bin_start"); \
    extern const uint8_t n##_end[] asm("_binary_" #n "_bin_end")
MODEL(wn9_nihaoxiaozhi);
MODEL(wn9_hiesp);
MODEL(wn9_jarvis_tts);

static voidpf model_alloc(voidpf unused, uInt count, uInt size)
{
    (void)unused;
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static void model_free(voidpf unused, voidpf data) { (void)unused; heap_caps_free(data); }

void *xiaozhi_wake_model_load(int word)
{
    const uint8_t *const starts[] = {wn9_nihaoxiaozhi_start, wn9_hiesp_start, wn9_jarvis_tts_start};
    const uint8_t *const ends[] = {wn9_nihaoxiaozhi_end, wn9_hiesp_end, wn9_jarvis_tts_end};
    if (!xiaozhi_wake_word_model(word)) return NULL;
    const uint8_t *data = starts[word];
    size_t size = ends[word] - data;
    if (size <= 4 || size > 512 * 1024) return NULL;
    uint32_t raw_size = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    if (raw_size < 160 || raw_size > 512 * 1024) return NULL;
    void *raw = heap_caps_malloc(raw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!raw) return NULL;
    z_stream stream = {.zalloc = model_alloc, .zfree = model_free};
    if (inflateInit(&stream) == Z_OK) {
        stream.next_in = (Bytef *)(data + 4); stream.avail_in = size - 4;
        stream.next_out = raw; stream.avail_out = raw_size;
        int result = inflate(&stream, Z_FINISH);
        bool complete = result == Z_STREAM_END && stream.total_out == raw_size && !stream.avail_in;
        inflateEnd(&stream);
        if (complete) return raw;
    }
    heap_caps_free(raw);
    return NULL;
}
void xiaozhi_wake_model_free(void *data) { heap_caps_free(data); }
