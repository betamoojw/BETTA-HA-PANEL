/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "xiaozhi_spectrum.h"
#define XIAOZHI_SPECTRUM_WINDOW 256
typedef struct {
    float real[XIAOZHI_SPECTRUM_WINDOW], imag[XIAOZHI_SPECTRUM_WINDOW];
    float window[XIAOZHI_SPECTRUM_WINDOW];
    unsigned used, skip;
} xiaozhi_spectrum_dsp_t;
void xiaozhi_spectrum_dsp_init(xiaozhi_spectrum_dsp_t *state);
void xiaozhi_spectrum_dsp_reset(xiaozhi_spectrum_dsp_t *state);
/* One 16 ms Hann window every 32 ms, with 24 contiguous bins spanning
 * 62.5 Hz through 8 kHz. Levels are peak magnitude per band, -60..0 dBFS. */
bool xiaozhi_spectrum_dsp_feed(xiaozhi_spectrum_dsp_t *state, const int16_t *pcm,
                              size_t samples, uint8_t bands[XIAOZHI_SPECTRUM_BANDS]);
