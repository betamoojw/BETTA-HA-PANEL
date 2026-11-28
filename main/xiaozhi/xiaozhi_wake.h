/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Audio worker interface for local wake and speech detection.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/** @brief Initialize local inference on the sole audio worker; no microphone data is sent. */
bool xiaozhi_wake_init(int word, const char *phrase);
/** @return PCM sample count required by the active wake model, or zero if none is loaded. */
int xiaozhi_wake_chunk(void);

/** @brief Test one model-sized block of mono 16 kHz PCM for the configured wake word. */
bool xiaozhi_wake_detect(int16_t *mono);

/** @return True when VAD reports speech in any complete 30 ms window of mono 16 kHz PCM. */
bool xiaozhi_voice_active(int16_t *mono, int samples);
/** @brief Release detection state; initialize again before the next session. */
void xiaozhi_wake_reset(void);
/** @brief Release wake and voice detection resources on the owning audio worker. */
void xiaozhi_wake_free(void);
