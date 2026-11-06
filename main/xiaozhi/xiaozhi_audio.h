/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
esp_err_t xiaozhi_audio_open(int volume);
esp_err_t xiaozhi_audio_mic(bool on);
/* Two compacted TDM slots: physical ES7210 MIC1 then MIC3 playback reference. */
esp_err_t xiaozhi_audio_read(int16_t *stereo, int frames);
esp_err_t xiaozhi_audio_write(int16_t *mono, int samples);
void xiaozhi_audio_close(void);
