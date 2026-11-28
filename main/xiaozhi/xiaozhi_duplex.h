/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Worker lifecycle and audio exchange for full-duplex sessions.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define XIAOZHI_DUPLEX_SAMPLES      960
#define XIAOZHI_DUPLEX_PLAY_SAMPLES 1920

/** @brief Captured 16 kHz mono PCM after echo cancellation, with capture-time metadata. */
typedef struct
{
    int16_t pcm[XIAOZHI_DUPLEX_SAMPLES];
    uint32_t sequence; /* Gaps identify frames dropped by the capture queue. */
    bool speech;
    bool playback; /* Playback was queued or active during capture. */
} xiaozhi_duplex_frame_t;

/** @brief Diagnostic snapshot; playback activity includes audio still in the output tail. */
typedef struct
{
    bool running, failed, playing;
    uint32_t captured_frames, played_frames, captured_during_playback, dropped_capture_frames;
    int mic_peak, ref_peak, clean_peak, processing_us, ref_peak_max_playback;
    uint32_t capture_stack_free, playback_stack_free;
} xiaozhi_duplex_stats_t;

/** @brief Start both audio workers after the session owner opens the codecs. */
esp_err_t xiaozhi_duplex_start(void);
/** @brief Join the workers before releasing their resources.
 * On timeout the session owner must keep the codecs open and retry.
 */
esp_err_t xiaozhi_duplex_stop(void);
/** @brief Copy a captured frame without blocking the session worker.
 * @param[out] frame Caller-owned destination.
 * @return True only when a frame was dequeued.
 */
bool xiaozhi_duplex_take(xiaozhi_duplex_frame_t *frame);

/** @brief Copy a playback block into the bounded queue from the session worker.
 * @param pcm Borrowed mono 16 kHz PCM.
 * @param samples Sample count, 1 through XIAOZHI_DUPLEX_PLAY_SAMPLES.
 * @return True if queued. A full queue may wait up to 150 ms and does not imply a transport error.
 */
bool xiaozhi_duplex_play(const int16_t *pcm, int samples);
/** @brief Invalidate queued or in-flight output; a bounded hardware DMA tail remains. */
void xiaozhi_duplex_flush(void);
/** @brief Copy diagnostics without accessing queues that may be tearing down. */
void xiaozhi_duplex_stats(xiaozhi_duplex_stats_t *out);
