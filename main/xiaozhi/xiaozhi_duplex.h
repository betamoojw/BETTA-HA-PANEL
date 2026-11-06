/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define XIAOZHI_DUPLEX_SAMPLES 960
#define XIAOZHI_DUPLEX_PLAY_SAMPLES 1920

typedef struct {
    int16_t pcm[XIAOZHI_DUPLEX_SAMPLES];
    uint32_t sequence; /* Gaps identify frames dropped by the capture queue. */
    bool speech;
    bool playback; /* Playback was queued or active during capture. */
} xiaozhi_duplex_frame_t;

typedef struct {
    bool running, failed, playing;
    uint32_t captured_frames, played_frames, captured_during_playback, dropped_capture_frames;
    int mic_peak, ref_peak, clean_peak, processing_us, ref_peak_max_playback;
    uint32_t capture_stack_free, playback_stack_free;
} xiaozhi_duplex_stats_t;

/* Session-owner calls only. Open the speaker and microphone before start.
 * stop joins both workers before releasing their resources. On timeout, keep
 * the codecs open and retry stop; live resources are deliberately retained. */
esp_err_t xiaozhi_duplex_start(void);
esp_err_t xiaozhi_duplex_stop(void);
bool xiaozhi_duplex_take(xiaozhi_duplex_frame_t *frame);
bool xiaozhi_duplex_play(const int16_t *pcm, int samples);
/* Invalidates queued/in-flight output; a bounded hardware DMA tail remains. */
void xiaozhi_duplex_flush(void);
void xiaozhi_duplex_stats(xiaozhi_duplex_stats_t *out);
