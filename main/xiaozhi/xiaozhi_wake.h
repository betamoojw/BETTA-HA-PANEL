/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Single audio-worker owner; local inference never sends microphone data. */
bool xiaozhi_wake_init(int word, const char *phrase);
int xiaozhi_wake_chunk(void);
bool xiaozhi_wake_detect(int16_t *mono);
bool xiaozhi_voice_active(int16_t *mono, int samples);
/* Releases state; call init again before the next detection/capture session. */
void xiaozhi_wake_reset(void);
void xiaozhi_wake_free(void);
