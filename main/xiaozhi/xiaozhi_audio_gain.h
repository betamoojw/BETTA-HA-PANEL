/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief LCD-X microphone and reference channel gain calibration.
 */
#pragma once
#include "audio_codec_if.h"

/** @brief Configure LCD-X codec gain per input channel.
 * The pinned codec-dev channel-gain wrapper
 * discards the driver's return value, so use the owned interface directly.
 * Its channel gain callback has no codec-dev cached state to update.
 *
 * @param codec Borrowed input codec interface. Channel masks address physical ADCs.
 * @return ESP_CODEC_DEV_OK if both gains were set, otherwise the first codec error.
 */
static inline int xiaozhi_audio_configure_input_gain(const audio_codec_if_t *codec)
{
    if (!codec)
    {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (!codec->set_mic_channel_gain)
    {
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    int err = codec->set_mic_channel_gain(codec, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 24.0f);
    if (err != ESP_CODEC_DEV_OK)
    {
        return err;
    }
    return codec->set_mic_channel_gain(codec, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 24.0f);
}
