/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/* Optional host microbenchmark; never an ESP32 CPU or PSRAM measurement. */
#include "xiaozhi_spectrum_dsp.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

static double seconds(void)
{
    return (double)clock() / CLOCKS_PER_SEC;
}

int main(void)
{
    xiaozhi_spectrum_dsp_t state;
    int16_t pcm[512];
    uint8_t bands[XIAOZHI_SPECTRUM_BANDS];
    volatile unsigned checksum = 0;
    const unsigned iterations = 20000;
    printf("Host only; analyzer_bytes=%zu; two_sources_bytes=%zu\n", sizeof(state), 2 * sizeof(state));
    for (unsigned mode = 0; mode < 3; ++mode) {
        for (unsigned i = 0; i < 512; ++i)
            pcm[i] = mode == 0 ? 0 : mode == 1 ? 12000 :
                (int16_t)(12000 * sin(6.28318530718 * 16 * i / 256));
        for (unsigned repeat = 0; repeat < 5; ++repeat) {
            xiaozhi_spectrum_dsp_init(&state);
            double start = seconds();
            for (unsigned i = 0; i < iterations; ++i) {
                if (xiaozhi_spectrum_dsp_feed(&state, pcm, 512, bands)) checksum += bands[10];
            }
            printf("%s repeat=%u windows=%u us_per_window=%.3f\n",
                   mode == 0 ? "silence" : mode == 1 ? "DC" : "tone",
                   repeat, iterations, (seconds() - start) * 1e6 / iterations);
        }
    }
    printf("checksum=%u\n", checksum);
    return 0;
}
