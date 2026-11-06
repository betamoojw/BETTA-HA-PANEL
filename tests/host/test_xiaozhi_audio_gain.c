/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include "xiaozhi/xiaozhi_audio_gain.h"

static unsigned calls, fail_call;
static int injected_error;
static const audio_codec_if_t *expected_codec;

static int set_channel_gain(const audio_codec_if_t *codec, uint16_t mask, float db)
{
    assert(codec == expected_codec && calls < 2);
    assert(mask == ESP_CODEC_DEV_MAKE_CHANNEL_MASK(calls == 0 ? 0 : 2));
    assert(db == 24.0f);
    ++calls;
    return calls == fail_call ? injected_error : ESP_CODEC_DEV_OK;
}

int main(void)
{
    const audio_codec_if_t codec = {.set_mic_channel_gain = set_channel_gain};
    const audio_codec_if_t unsupported = {0};
    expected_codec = &codec;
    assert(xiaozhi_audio_configure_input_gain(NULL) == ESP_CODEC_DEV_INVALID_ARG);
    assert(xiaozhi_audio_configure_input_gain(&unsupported) == ESP_CODEC_DEV_NOT_SUPPORT);
    assert(calls == 0);
    assert(xiaozhi_audio_configure_input_gain(&codec) == ESP_CODEC_DEV_OK && calls == 2);

    /* An I2C register read or write failure must reach the microphone owner,
     * which closes capture instead of accepting an uncalibrated reference. */
    const int failures[] = {ESP_CODEC_DEV_READ_FAIL, ESP_CODEC_DEV_WRITE_FAIL};
    for (unsigned channel = 1; channel <= 2; ++channel) {
        for (unsigned failure = 0; failure < sizeof(failures) / sizeof(failures[0]); ++failure) {
            calls = 0;
            fail_call = channel;
            injected_error = failures[failure];
            assert(xiaozhi_audio_configure_input_gain(&codec) == injected_error);
            assert(calls == channel); /* Do not configure MIC3 after MIC1 failed. */
            calls = fail_call = 0;
            assert(xiaozhi_audio_configure_input_gain(&codec) == ESP_CODEC_DEV_OK && calls == 2);
        }
    }
    puts("Xiaozhi input calibration: physical routing, gain errors and retry passed");
    return 0;
}
