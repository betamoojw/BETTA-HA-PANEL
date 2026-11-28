/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Streaming spectrum DSP state and input contract.
 */
#pragma once
#include "xiaozhi_spectrum.h"
#define XIAOZHI_SPECTRUM_WINDOW 256

typedef struct
{
    float real[XIAOZHI_SPECTRUM_WINDOW], imag[XIAOZHI_SPECTRUM_WINDOW];
    float window[XIAOZHI_SPECTRUM_WINDOW];
    unsigned used, skip;
} xiaozhi_spectrum_dsp_t;

/** @brief Initialize the stream and precompute the Hann window. */
void xiaozhi_spectrum_dsp_init(xiaozhi_spectrum_dsp_t *state);

/** @brief Discard accumulated samples and restart cadence while retaining the window coefficients. */
void xiaozhi_spectrum_dsp_reset(xiaozhi_spectrum_dsp_t *state);
/** @brief Process one 16 ms Hann window every 32 ms, with 24 contiguous bins spanning
 * 62.5 Hz through 8 kHz. Levels are peak magnitude per band, -60..0 dBFS.
 * @param[in,out] state Per-source DSP state.
 * @param pcm Borrowed mono 16 kHz input.
 * @param samples Input block length in samples.
 * @param[out] bands Destination for the most recent complete window, if any.
 * @return True only if a new window produced band levels.
 */
bool xiaozhi_spectrum_dsp_feed(xiaozhi_spectrum_dsp_t *state, const int16_t *pcm, size_t samples, uint8_t bands[XIAOZHI_SPECTRUM_BANDS]);
