/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_spectrum_dsp.h"
#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979323846f
static const uint8_t edges[XIAOZHI_SPECTRUM_BANDS + 1] = {
    1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 21, 26, 32, 39, 47, 56, 67, 79, 92, 106, 118, 125, 129
};

void xiaozhi_spectrum_dsp_reset(xiaozhi_spectrum_dsp_t *s) { s->used = s->skip = 0; }
void xiaozhi_spectrum_dsp_init(xiaozhi_spectrum_dsp_t *s)
{
    memset(s, 0, sizeof(*s));
    for (unsigned i = 0; i < XIAOZHI_SPECTRUM_WINDOW; ++i)
        s->window[i] = .5f - .5f * cosf(2.f * PI_F * i / (XIAOZHI_SPECTRUM_WINDOW - 1));
}

static void transform(xiaozhi_spectrum_dsp_t *s, uint8_t *bands)
{
    const unsigned n = XIAOZHI_SPECTRUM_WINDOW;
    float mean = 0;
    for (unsigned i = 0; i < n; ++i) mean += s->real[i];
    mean /= n;
    for (unsigned i = 0; i < n; ++i) {
        s->real[i] = (s->real[i] - mean) * s->window[i];
        s->imag[i] = 0;
    }
    for (unsigned i = 1, j = 0; i < n; ++i) {
        unsigned bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float value = s->real[i]; s->real[i] = s->real[j]; s->real[j] = value; }
    }
    for (unsigned length = 2; length <= n; length <<= 1) {
        float angle = -2.f * PI_F / length;
        float step_r = cosf(angle), step_i = sinf(angle);
        for (unsigned offset = 0; offset < n; offset += length) {
            float wr = 1, wi = 0;
            for (unsigned j = 0; j < length / 2; ++j) {
                unsigned a = offset + j, b = a + length / 2;
                float br = s->real[b] * wr - s->imag[b] * wi;
                float bi = s->real[b] * wi + s->imag[b] * wr;
                s->real[b] = s->real[a] - br; s->imag[b] = s->imag[a] - bi;
                s->real[a] += br; s->imag[a] += bi;
                float next = wr * step_r - wi * step_i;
                wi = wr * step_i + wi * step_r; wr = next;
            }
        }
    }
    for (unsigned band = 0; band < XIAOZHI_SPECTRUM_BANDS; ++band) {
        float power = 0;
        for (unsigned i = edges[band]; i < edges[band + 1]; ++i) {
            float value = s->real[i] * s->real[i] + s->imag[i] * s->imag[i];
            /* Nyquist has no negative-frequency partner. */
            if (i == n / 2) value *= .25f;
            if (value > power) power = value;
        }
        float magnitude = sqrtf(power) * (4.f / (n - 1));
        float level = magnitude > .001f ? (20.f * log10f(magnitude) + 60.f) * (255.f / 60.f) : 0;
        bands[band] = level <= 0 ? 0 : level >= 255 ? 255 : (uint8_t)(level + .5f);
    }
}

bool xiaozhi_spectrum_dsp_feed(xiaozhi_spectrum_dsp_t *s, const int16_t *pcm,
                              size_t samples, uint8_t bands[XIAOZHI_SPECTRUM_BANDS])
{
    if (!s || !pcm || !bands) return false;
    bool updated = false;
    for (size_t i = 0; i < samples; ++i) {
        if (s->skip) { --s->skip; continue; }
        s->real[s->used++] = pcm[i] / 32768.f;
        if (s->used == XIAOZHI_SPECTRUM_WINDOW) {
            transform(s, bands); updated = true;
            s->used = 0; s->skip = XIAOZHI_SPECTRUM_WINDOW;
        }
    }
    return updated;
}
