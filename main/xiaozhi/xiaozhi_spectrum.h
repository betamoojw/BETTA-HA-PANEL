/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XIAOZHI_SPECTRUM_BANDS 24
#define XIAOZHI_SPECTRUM_STALE_US 250000
typedef enum { XZ_SPECTRUM_MIC, XZ_SPECTRUM_SPEAKER, XZ_SPECTRUM_SOURCES } xiaozhi_spectrum_source_t;
typedef struct {
    uint8_t microphone[XIAOZHI_SPECTRUM_BANDS], speaker[XIAOZHI_SPECTRUM_BANDS];
    bool mic_active, speaker_active;
} xiaozhi_spectrum_snapshot_t;

/* Session owner initializes before audio workers start. Allocation failure only
 * disables visualization; audio must continue. Storage is retained in PSRAM. */
void xiaozhi_spectrum_init(void);
/* Clear is safe during a write: a ticket obtained before clear cannot publish. */
uint32_t xiaozhi_spectrum_generation(xiaozhi_spectrum_source_t source);
void xiaozhi_spectrum_clear(xiaozhi_spectrum_source_t source);
/* Exactly one PCM producer per source. Caller obtains the ticket before its
 * hardware read/write. Input is mono 16 kHz PCM and is never modified. */
void xiaozhi_spectrum_feed(xiaozhi_spectrum_source_t source, const int16_t *pcm,
                          size_t samples, uint32_t generation);
void xiaozhi_spectrum_snapshot(xiaozhi_spectrum_snapshot_t *out);
