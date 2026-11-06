/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../main/xiaozhi/xiaozhi_spectrum.c"

static int64_t now = 1000000;
static bool clear_on_clock;
static unsigned allocation_count, fail_allocation;
int64_t esp_timer_get_time(void)
{
    if (clear_on_clock) { clear_on_clock = false; xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER); }
    return now;
}
void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return ++allocation_count == fail_allocation ? NULL : calloc(count, size);
}
static void tone(int16_t *pcm, unsigned count, unsigned bin, float amplitude)
{
    for (unsigned i = 0; i < count; ++i)
        pcm[i] = (int16_t)(amplitude * sinf(6.28318530718f * bin * i / 256));
}
static unsigned strongest(const uint8_t *bands)
{
    unsigned best = 0;
    for (unsigned i = 1; i < 24; ++i) if (bands[i] > bands[best]) best = i;
    return best;
}
static void all_zero(const uint8_t *bands)
{ for (unsigned i = 0; i < 24; ++i) assert(bands[i] == 0); }
static void feed(xiaozhi_spectrum_source_t source, const int16_t *pcm, unsigned count)
{ xiaozhi_spectrum_feed(source, pcm, count, xiaozhi_spectrum_generation(source)); }
static void dispose(void)
{
    for (unsigned i = 0; i < XZ_SPECTRUM_SOURCES; ++i) free(sources[i].dsp);
    memset(sources, 0, sizeof(sources)); initialized = false;
    allocation_count = fail_allocation = 0;
}

int main(void)
{
    xiaozhi_spectrum_dsp_t dsp, split;
    int16_t pcm[1024], saved[1024];
    uint8_t bands[24], low[24], chunked[24];
    memset(pcm, 0, sizeof(pcm)); xiaozhi_spectrum_dsp_init(&dsp);
    assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, bands)); all_zero(bands);
    for (unsigned i = 0; i < 1024; ++i) pcm[i] = 12000;
    xiaozhi_spectrum_dsp_reset(&dsp);
    assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, bands)); all_zero(bands); /* DC rejection. */
    /* All PCM constants, including extreme values, are exact DC, not noise.
     * A quiet tone just above the display floor must still pass unchanged. */
    const int16_t dc[] = {INT16_MIN, -1, 0, 1, INT16_MAX};
    for (unsigned j = 0; j < sizeof(dc) / sizeof(dc[0]); ++j) {
        for (unsigned i = 0; i < 256; ++i) pcm[i] = dc[j];
        xiaozhi_spectrum_dsp_reset(&dsp);
        assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, bands)); all_zero(bands);
    }
    tone(pcm, 256, 16, 64); xiaozhi_spectrum_dsp_reset(&dsp);
    assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, bands)); assert(bands[10] > 0);
    const unsigned bins[] = {2, 8, 16, 64, 120};
    const unsigned expected[] = {1, 7, 10, 17, 22};
    for (unsigned i = 0; i < sizeof(bins) / sizeof(bins[0]); ++i) {
        tone(pcm, 256, bins[i], 16384); xiaozhi_spectrum_dsp_reset(&dsp);
        assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, bands));
        assert(strongest(bands) == expected[i] && bands[expected[i]] > 220 && bands[expected[i]] < 235);
    }
    tone(pcm, 256, 16, 4096); xiaozhi_spectrum_dsp_reset(&dsp);
    assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, low));
    tone(pcm, 1024, 16, 16384); memcpy(saved, pcm, sizeof(pcm));
    xiaozhi_spectrum_dsp_reset(&dsp);
    assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 1024, bands));
    assert(bands[10] - low[10] >= 50 && bands[10] - low[10] <= 52);
    xiaozhi_spectrum_dsp_init(&split);
    const unsigned chunks[] = {17, 143, 11, 90, 400, 363};
    unsigned offset = 0;
    for (unsigned i = 0; i < sizeof(chunks) / sizeof(chunks[0]); ++i) {
        xiaozhi_spectrum_dsp_feed(&split, pcm + offset, chunks[i], chunked); offset += chunks[i];
    }
    assert(offset == 1024 && !memcmp(bands, chunked, 24) && !memcmp(saved, pcm, sizeof(pcm)));
    for (unsigned i = 0; i < 256; ++i) pcm[i] = i & 1 ? INT16_MIN : INT16_MAX;
    xiaozhi_spectrum_dsp_reset(&dsp); assert(xiaozhi_spectrum_dsp_feed(&dsp, pcm, 256, bands));
    assert(strongest(bands) == 23 && bands[23] == 255);

    xiaozhi_spectrum_snapshot_t snapshot;
    xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.mic_active && !snapshot.speaker_active);
    xiaozhi_spectrum_init(); assert(allocation_count == 2);
    xiaozhi_spectrum_init(); assert(allocation_count == 2);
    tone(pcm, 1024, 16, 16384); feed(XZ_SPECTRUM_MIC, pcm, 128);
    xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.mic_active);
    feed(XZ_SPECTRUM_MIC, pcm + 128, 128); feed(XZ_SPECTRUM_SPEAKER, pcm, 256);
    xiaozhi_spectrum_snapshot(&snapshot);
    assert(snapshot.mic_active && snapshot.speaker_active && snapshot.microphone[10] > 220);
    snapshot.microphone[10] = 0; xiaozhi_spectrum_snapshot(&snapshot); assert(snapshot.microphone[10] > 220);
    now += 249999; xiaozhi_spectrum_snapshot(&snapshot); assert(snapshot.mic_active);
    ++now; xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.mic_active && !snapshot.speaker_active);
    all_zero(snapshot.microphone); all_zero(snapshot.speaker);
    uint32_t old = xiaozhi_spectrum_generation(XZ_SPECTRUM_SPEAKER);
    xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
    xiaozhi_spectrum_feed(XZ_SPECTRUM_SPEAKER, pcm, 256, old);
    xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.speaker_active);
    feed(XZ_SPECTRUM_SPEAKER, pcm, 128);
    now += 250000; feed(XZ_SPECTRUM_SPEAKER, pcm + 128, 128);
    xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.speaker_active); /* No old partial window. */
    feed(XZ_SPECTRUM_SPEAKER, pcm, 128);
    xiaozhi_spectrum_snapshot(&snapshot); assert(snapshot.speaker_active);
    xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
    old = xiaozhi_spectrum_generation(XZ_SPECTRUM_SPEAKER);
    clear_on_clock = true;
    xiaozhi_spectrum_feed(XZ_SPECTRUM_SPEAKER, pcm, 256, old);
    xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.speaker_active); /* Clear racing the transform. */
    /* Ten minutes of dual-source PCM in virtual time. Only the two analyzers
     * remain allocated; rapid tone/silence/source changes leave no stale bands. */
    tone(pcm, 512, 16, 16384);
    int16_t silence[512] = {0};
    xiaozhi_spectrum_clear(XZ_SPECTRUM_MIC); xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
    for (unsigned i = 0; i < 18750; ++i) {
        now += 32000;
        feed(XZ_SPECTRUM_MIC, i & 1 ? pcm : silence, 512);
        feed(XZ_SPECTRUM_SPEAKER, i & 1 ? silence : pcm, 512);
        xiaozhi_spectrum_snapshot(&snapshot);
        assert(snapshot.mic_active && snapshot.speaker_active);
        assert((i & 1 ? snapshot.microphone : snapshot.speaker)[10] > 220);
        all_zero(i & 1 ? snapshot.speaker : snapshot.microphone);
    }
    assert(allocation_count == 2);
    assert(sources[XZ_SPECTRUM_MIC].dsp->used == 0 && sources[XZ_SPECTRUM_SPEAKER].dsp->used == 0);
    xiaozhi_spectrum_clear(XZ_SPECTRUM_MIC); xiaozhi_spectrum_clear(XZ_SPECTRUM_SPEAKER);
    xiaozhi_spectrum_snapshot(&snapshot); all_zero(snapshot.microphone); all_zero(snapshot.speaker);
    dispose(); fail_allocation = 1; xiaozhi_spectrum_init();
    feed(XZ_SPECTRUM_MIC, pcm, 256); feed(XZ_SPECTRUM_SPEAKER, pcm, 256);
    xiaozhi_spectrum_snapshot(&snapshot); assert(!snapshot.mic_active && snapshot.speaker_active);
    dispose();
    puts("Xiaozhi spectrum: frequency, level, silence, chunks, immutable PCM, freshness, cancellation and allocation fallback passed");
}
