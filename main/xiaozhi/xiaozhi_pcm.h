/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Extract mono microphone samples and peaks from LCD-X capture slots.
 */
#pragma once
#include <stdint.h>

/** @brief Compact the microphone samples in place and measure both peaks.
 * LCD-X ES7210 slots are microphone + playback reference, not two mics.
 * Retain MIC1 at full amplitude; mixing the reference attenuates speech and
 * can inject speaker output. In-place compaction is safe from low to high.
 *
 * @param[in,out] pcm Interleaved microphone and reference samples; the first frames samples become mono.
 * @param frames Number of two-sample input frames.
 * @param[out] mic_peak Optional absolute microphone peak.
 * @param[out] ref_peak Optional absolute playback reference peak.
 */
static inline void xiaozhi_pcm_mono(int16_t *pcm, int frames, int *mic_peak, int *ref_peak)
{
    int m = 0, r = 0;
    for (int i = 0; i < frames; ++i)
    {
        int left = pcm[2 * i], right = pcm[2 * i + 1];
        int ml = left < 0 ? -left : left, rr = right < 0 ? -right : right;
        if (ml > m)
        {
            m = ml;
        }
        if (rr > r)
        {
            r = rr;
        }
        pcm[i] = left;
    }
    if (mic_peak)
    {
        *mic_peak = m;
    }
    if (ref_peak)
    {
        *ref_peak = r;
    }
}
