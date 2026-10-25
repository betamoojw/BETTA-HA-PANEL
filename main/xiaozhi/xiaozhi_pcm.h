/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdint.h>

/* LCD-X ES7210 slots are microphone + playback reference, not two mics.
 * Retain MIC1 at full amplitude; mixing the reference attenuates speech and
 * can inject speaker output. In-place compaction is safe from low to high. */
static inline void xiaozhi_pcm_mono(int16_t *pcm, int frames, int *mic_peak, int *ref_peak)
{
    int m = 0, r = 0;
    for (int i = 0; i < frames; ++i) {
        int left = pcm[2*i], right = pcm[2*i+1];
        int ml = left < 0 ? -left : left, rr = right < 0 ? -right : right;
        if (ml > m) m = ml;
        if (rr > r) r = rr;
        pcm[i] = left;
    }
    if (mic_peak) *mic_peak = m;
    if (ref_peak) *ref_peak = r;
}
