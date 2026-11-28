/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Shared microphone and playback activity clock.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XIAOZHI_SILENCE_TIMEOUT_MS 30000U
#define XIAOZHI_OUTPUT_TAIL_MS     120U

/** @brief Start tracking activity for a dialog generation.
 * One shared audio clock. Tickets obtained before I/O cannot revive an ended
 * dialog or extend a newer dialog. No audio buffers or transcripts are kept. */
void xiaozhi_activity_begin(uint32_t generation);
/** @brief Stop tracking only if generation still identifies the current dialog. */
void xiaozhi_activity_end(uint32_t generation);

/** @return Dialog generation to sample before starting audio I/O. */
uint32_t xiaozhi_activity_generation(void);

/** @return Revision advanced when a dialog begins or newer audio is observed. */
uint32_t xiaozhi_activity_revision(void);

/** @brief Record observed audio for the active dialog; ignore old timestamps and stale generations.
 * @param generation Dialog generation sampled before audio I/O.
 * @param observed_us Observation time on the ESP timer's monotonic microsecond clock.
 */
void xiaozhi_activity_note(uint32_t generation, int64_t observed_us);

/** @brief Record a successful, audible PCM write, allowing for the bounded I2S DMA tail. */
void xiaozhi_activity_note_playback(uint32_t generation, int64_t observed_us);

/** @return Remaining silence budget in milliseconds, or zero when expired, ended, or stale. */
uint32_t xiaozhi_activity_remaining_ms(uint32_t generation);
/** @brief Claim an expired generation atomically against concurrent audio updates. */
bool xiaozhi_activity_expire(uint32_t generation);

/** @brief Test PCM against a DC-removed energy floor, not a speech classifier.
 *
 * Microphone activity additionally requires speech VAD.
 */
bool xiaozhi_activity_has_sound(const int16_t *pcm, size_t samples);
