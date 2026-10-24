/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * LCD-X ES8311 output / ES7210 input; pins are owned by the vendor BSP.
 */
#include <stdbool.h>
#include "xiaozhi_audio.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_x.h"
#include "esp_codec_dev.h"
#include "driver/i2s_std.h"

static esp_codec_dev_handle_t speaker, mic;
static bool opened, recording;

esp_err_t xiaozhi_audio_open(int volume)
{
    if (!speaker || !mic) {
        const i2s_std_config_t cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {.mclk = BSP_I2S_MCLK, .bclk = BSP_I2S_SCLK, .ws = BSP_I2S_LCLK,
                         .dout = BSP_I2S_DOUT, .din = BSP_I2S_DSIN}
        };
        if (bsp_audio_init(&cfg) != ESP_OK) return ESP_FAIL;
        if (!speaker) speaker = bsp_audio_codec_speaker_init();
        if (!mic) mic = bsp_audio_codec_microphone_init();
        if (!speaker || !mic) return ESP_FAIL;
    }
    if (!opened) {
        esp_codec_dev_sample_info_t info = {.sample_rate = 16000, .channel = 1, .bits_per_sample = 16};
        if (esp_codec_dev_open(speaker, &info) != ESP_CODEC_DEV_OK) return ESP_FAIL;
        opened = true;
    }
    return esp_codec_dev_set_out_vol(speaker, volume) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t xiaozhi_audio_mic(bool on)
{
    if (!mic) return ESP_ERR_INVALID_STATE;
    if (on == recording) return ESP_OK;
    if (on) {
        esp_codec_dev_sample_info_t info = {.sample_rate = 16000, .channel = 2, .bits_per_sample = 16};
        if (esp_codec_dev_open(mic, &info) != ESP_CODEC_DEV_OK) return ESP_FAIL;
        esp_codec_dev_set_in_gain(mic, 24.0f);
    } else {
        esp_codec_dev_close(mic);
    }
    recording = on;
    return ESP_OK;
}

esp_err_t xiaozhi_audio_read(int16_t *stereo, int frames)
{
    if (!recording) return ESP_ERR_INVALID_STATE;
    return esp_codec_dev_read(mic, stereo, frames * 4) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t xiaozhi_audio_write(int16_t *mono, int samples)
{
    if (!opened) return ESP_ERR_INVALID_STATE;
    return esp_codec_dev_write(speaker, mono, samples * 2) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

void xiaozhi_audio_close(void)
{
    if (recording) xiaozhi_audio_mic(false);
    if (opened) esp_codec_dev_close(speaker);
    opened = false;
}
