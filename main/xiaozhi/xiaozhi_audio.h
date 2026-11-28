/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Codec lifecycle and PCM capture and playback interface.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
/** @brief Open or update the 16 kHz mono speaker at the requested 0-80 percent volume. */
esp_err_t xiaozhi_audio_open(int volume);

/** @brief Enable or disable 16 kHz microphone capture after codec initialization. */
esp_err_t xiaozhi_audio_mic(bool on);

/** @brief Read two compacted TDM slots: ES7210 MIC1 then the MIC3 playback reference.
 * @param[out] stereo Destination for two int16_t samples per frame.
 * @param frames Number of frames, 1 through 1920.
 * @return ESP_OK on a successful capture, otherwise a state, argument, or codec error.
 */
esp_err_t xiaozhi_audio_read(int16_t *stereo, int frames);

/** @brief Write 1 through 1920 mono PCM samples to the opened speaker. */
esp_err_t xiaozhi_audio_write(int16_t *mono, int samples);
/** @return True if a successful write can be audible at the current output level. */
bool xiaozhi_audio_output_audible(void);
/** @brief Close the audio streams after workers have stopped using them. */
void xiaozhi_audio_close(void);
