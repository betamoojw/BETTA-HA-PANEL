/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Notification sound selection, playback policy, and PCM synthesis.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Notification sound presets. */
typedef enum
{
    CHIME_NONE,     /**< Silence. */
    CHIME_DOORBELL, /**< Two-tone doorbell. */
    CHIME_NOTIFY    /**< Short notification tone. */
} chime_kind_t;

/** @brief Output rate in PCM samples per second. */
#define CHIME_SAMPLE_RATE 16000
/** @return True for the exact names `none`, `chime`, or `notify`, updating out only on success. */
bool chime_kind_parse(const char *text, chime_kind_t *out);
/** @return True when sound is enabled, volume is positive, and night mode does not mute it. */
bool chime_should_play(bool enabled, bool night_mute, bool night_active, int volume);
/** @return Total samples for the preset, or zero for silence or an unknown preset. */
size_t chime_synth_samples(chime_kind_t kind);
/** @brief Synthesize a slice with bounded amplitude and a smooth attack and release.
 * @param kind Sound to render.
 * @param offset Start position in samples from the beginning of the sound.
 * @param[out] pcm Destination for signed 16-bit mono samples.
 * @param count Number of samples to write, filling positions beyond the sound with zero.
 */
void chime_synth_render(chime_kind_t kind, size_t offset, int16_t *pcm, size_t count);
