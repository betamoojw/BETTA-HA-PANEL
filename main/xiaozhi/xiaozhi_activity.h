/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XIAOZHI_SILENCE_TIMEOUT_MS 30000U
#define XIAOZHI_OUTPUT_TAIL_MS 120U

/* One shared audio clock. Tickets obtained before I/O cannot revive an ended
 * dialog or extend a newer dialog. No audio buffers or transcripts are kept. */
void xiaozhi_activity_begin(uint32_t generation);
void xiaozhi_activity_end(uint32_t generation);
uint32_t xiaozhi_activity_generation(void);
uint32_t xiaozhi_activity_revision(void);
void xiaozhi_activity_note(uint32_t generation, int64_t observed_us);
/* A completed codec write can leave bounded I2S DMA audio in flight. Account
 * for that tail, only after a successful non-silent audible output write. */
void xiaozhi_activity_note_playback(uint32_t generation, int64_t observed_us);
uint32_t xiaozhi_activity_remaining_ms(uint32_t generation);
/* Atomically claims an expired generation against concurrent audio updates. */
bool xiaozhi_activity_expire(uint32_t generation);

/* A conservative AC-energy floor excludes digital silence, DC and tiny codec
 * noise. Microphone activity additionally requires the existing speech VAD. */
bool xiaozhi_activity_has_sound(const int16_t *pcm, size_t samples);
