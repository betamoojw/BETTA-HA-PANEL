/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_spectrum.h"
#include "xiaozhi_spectrum_dsp.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

typedef struct {
    xiaozhi_spectrum_dsp_t *dsp;
    uint32_t producer_generation;
    int64_t last_feed;
    /* Only the following snapshot fields are shared with readers/clear. */
    uint32_t generation;
    int64_t updated;
    uint8_t bands[XIAOZHI_SPECTRUM_BANDS];
    bool valid;
} source_t;
static source_t sources[XZ_SPECTRUM_SOURCES];
static portMUX_TYPE spectrum_lock = portMUX_INITIALIZER_UNLOCKED;
static bool initialized;

void xiaozhi_spectrum_init(void)
{
    if (initialized) return;
    initialized = true;
    for (unsigned i = 0; i < XZ_SPECTRUM_SOURCES; ++i) {
        if (sources[i].dsp) continue;
        sources[i].dsp = heap_caps_calloc(1, sizeof(*sources[i].dsp), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (sources[i].dsp) xiaozhi_spectrum_dsp_init(sources[i].dsp);
    }
}
uint32_t xiaozhi_spectrum_generation(xiaozhi_spectrum_source_t source)
{
    if ((unsigned)source >= XZ_SPECTRUM_SOURCES) return 0;
    portENTER_CRITICAL(&spectrum_lock);
    uint32_t generation = sources[source].generation;
    portEXIT_CRITICAL(&spectrum_lock);
    return generation;
}
void xiaozhi_spectrum_clear(xiaozhi_spectrum_source_t source)
{
    if ((unsigned)source >= XZ_SPECTRUM_SOURCES) return;
    portENTER_CRITICAL(&spectrum_lock);
    ++sources[source].generation;
    sources[source].valid = false;
    memset(sources[source].bands, 0, XIAOZHI_SPECTRUM_BANDS);
    portEXIT_CRITICAL(&spectrum_lock);
}
void xiaozhi_spectrum_feed(xiaozhi_spectrum_source_t source, const int16_t *pcm,
                          size_t samples, uint32_t generation)
{
    if ((unsigned)source >= XZ_SPECTRUM_SOURCES || !pcm || samples > 1920) return;
    source_t *s = &sources[source];
    if (!s->dsp || generation != xiaozhi_spectrum_generation(source)) return;
    int64_t now = esp_timer_get_time();
    if (s->producer_generation != generation || now - s->last_feed >= XIAOZHI_SPECTRUM_STALE_US) {
        xiaozhi_spectrum_dsp_reset(s->dsp); s->producer_generation = generation;
    }
    s->last_feed = now;
    uint8_t bands[XIAOZHI_SPECTRUM_BANDS];
    if (!xiaozhi_spectrum_dsp_feed(s->dsp, pcm, samples, bands)) return;
    portENTER_CRITICAL(&spectrum_lock);
    if (s->generation == generation) {
        memcpy(s->bands, bands, sizeof(bands)); s->updated = now; s->valid = true;
    }
    portEXIT_CRITICAL(&spectrum_lock);
}
void xiaozhi_spectrum_snapshot(xiaozhi_spectrum_snapshot_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&spectrum_lock);
    for (unsigned i = 0; i < XZ_SPECTRUM_SOURCES; ++i) {
        source_t *s = &sources[i];
        bool active = s->valid && now >= s->updated && now - s->updated < XIAOZHI_SPECTRUM_STALE_US;
        if (i == XZ_SPECTRUM_MIC) {
            out->mic_active = active;
            if (active) memcpy(out->microphone, s->bands, sizeof(s->bands));
        } else {
            out->speaker_active = active;
            if (active) memcpy(out->speaker, s->bands, sizeof(s->bands));
        }
    }
    portEXIT_CRITICAL(&spectrum_lock);
}
