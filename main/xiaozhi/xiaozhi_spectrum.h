/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Audio worker spectrum publication and UI snapshot interface.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XIAOZHI_SPECTRUM_BANDS    24
#define XIAOZHI_SPECTRUM_STALE_US 250000

typedef enum
{
    XZ_SPECTRUM_MIC,
    XZ_SPECTRUM_SPEAKER,
    XZ_SPECTRUM_SOURCES
} xiaozhi_spectrum_source_t;

typedef struct
{
    uint8_t microphone[XIAOZHI_SPECTRUM_BANDS], speaker[XIAOZHI_SPECTRUM_BANDS];
    bool mic_active, speaker_active;
} xiaozhi_spectrum_snapshot_t;

/** @brief Initialize visualization before the session owner starts audio workers.
 * Allocation failure only disables visualization; audio must continue. Storage is retained in PSRAM.
 */
void xiaozhi_spectrum_init(void);
/** @return Generation ticket to obtain before the source's hardware I/O. */
uint32_t xiaozhi_spectrum_generation(xiaozhi_spectrum_source_t source);
/** @brief Clear a source and invalidate tickets already held by its producer. */
void xiaozhi_spectrum_clear(xiaozhi_spectrum_source_t source);
/** @brief Feed borrowed mono 16 kHz PCM from the sole producer for this source.
 * Pass the ticket obtained before hardware I/O; PCM is never modified.
 */
void xiaozhi_spectrum_feed(xiaozhi_spectrum_source_t source, const int16_t *pcm, size_t samples, uint32_t generation);
/** @brief Copy fresh band levels; absent and stale sources are returned as inactive with zeroed bands. */
void xiaozhi_spectrum_snapshot(xiaozhi_spectrum_snapshot_t *out);
