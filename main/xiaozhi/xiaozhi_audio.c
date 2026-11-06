/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * LCD-X ES8311 output / ES7210 input; pins come from the vendor BSP.
 * TDM routing follows BoxAudioCodec in betamoojw/xiaozhi-esp32 a73e9a7e.
 */
#include <stdbool.h>
#include "xiaozhi_audio.h"
#include "xiaozhi_audio_gain.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_x.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_log.h"

static esp_codec_dev_handle_t speaker, mic;
static bool opened, recording;
static i2s_chan_handle_t tx, rx;
static const audio_codec_data_if_t *data_if;
static const audio_codec_gpio_if_t *gpio_if;
static const audio_codec_ctrl_if_t *speaker_ctrl, *mic_ctrl;
static const audio_codec_if_t *speaker_codec, *mic_codec;

static void release_partial_init(void)
{
    if (mic) esp_codec_dev_delete(mic);
    if (speaker) esp_codec_dev_delete(speaker);
    mic = speaker = NULL;
    if (mic_codec) audio_codec_delete_codec_if(mic_codec);
    if (speaker_codec) audio_codec_delete_codec_if(speaker_codec);
    mic_codec = speaker_codec = NULL;
    if (mic_ctrl) audio_codec_delete_ctrl_if(mic_ctrl);
    if (speaker_ctrl) audio_codec_delete_ctrl_if(speaker_ctrl);
    mic_ctrl = speaker_ctrl = NULL;
    if (gpio_if) audio_codec_delete_gpio_if(gpio_if);
    if (data_if) audio_codec_delete_data_if(data_if);
    gpio_if = NULL; data_if = NULL;
    if (rx) { (void)i2s_channel_disable(rx); (void)i2s_del_channel(rx); rx = NULL; }
    if (tx) { (void)i2s_channel_disable(tx); (void)i2s_del_channel(tx); tx = NULL; }
}

static esp_err_t initialize(void)
{
    if (speaker && mic) return ESP_OK;
    if (bsp_i2c_init() != ESP_OK || !bsp_i2c_get_handle()) return ESP_FAIL;
    /* The BSP's ordinary stereo input exposes acoustic MIC1/MIC2. The LCD-X
     * schematic connects ES8311 OUTP/OUTN to ES7210 MIC3 for AEC. Four-slot
     * I2S TDM orders ADC1, ADC3, ADC2, ADC4: select slots 0 and 1 below. */
    i2s_chan_config_t channels = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    channels.auto_clear = true;
    channels.dma_desc_num = 6;
    channels.dma_frame_num = 240;
    esp_err_t err = i2s_new_channel(&channels, &tx, &rx);
    if (err != ESP_OK) goto error;
    const i2s_std_config_t out = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = BSP_I2S_MCLK, .bclk = BSP_I2S_SCLK, .ws = BSP_I2S_LCLK,
                     .dout = BSP_I2S_DOUT, .din = I2S_GPIO_UNUSED}
    };
    const i2s_tdm_config_t in = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
        .gpio_cfg = {.mclk = BSP_I2S_MCLK, .bclk = BSP_I2S_SCLK, .ws = BSP_I2S_LCLK,
                     .dout = I2S_GPIO_UNUSED, .din = BSP_I2S_DSIN}
    };
    if ((err = i2s_channel_init_std_mode(tx, &out)) != ESP_OK ||
        (err = i2s_channel_init_tdm_mode(rx, &in)) != ESP_OK ||
        (err = i2s_channel_enable(tx)) != ESP_OK ||
        (err = i2s_channel_enable(rx)) != ESP_OK) goto error;
    audio_codec_i2s_cfg_t data = {.port = CONFIG_BSP_I2S_NUM, .tx_handle = tx, .rx_handle = rx};
    data_if = audio_codec_new_i2s_data(&data);
    gpio_if = audio_codec_new_gpio();
    audio_codec_i2c_cfg_t ctrl = {.port = BSP_I2C_NUM, .addr = ES8311_CODEC_DEFAULT_ADDR,
                                .bus_handle = bsp_i2c_get_handle()};
    speaker_ctrl = audio_codec_new_i2c_ctrl(&ctrl);
    ctrl.addr = ES7210_CODEC_DEFAULT_ADDR;
    mic_ctrl = audio_codec_new_i2c_ctrl(&ctrl);
    if (!data_if || !gpio_if || !speaker_ctrl || !mic_ctrl) { err = ESP_ERR_NO_MEM; goto error; }
    es8311_codec_cfg_t output = {.ctrl_if = speaker_ctrl, .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC, .pa_pin = BSP_POWER_AMP_IO,
        .use_mclk = true, .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3}};
    speaker_codec = es8311_codec_new(&output);
    es7210_codec_cfg_t input = {.ctrl_if = mic_ctrl,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4};
    mic_codec = es7210_codec_new(&input);
    if (!speaker_codec || !mic_codec) { err = ESP_FAIL; goto error; }
    esp_codec_dev_cfg_t device = {.dev_type = ESP_CODEC_DEV_TYPE_OUT, .codec_if = speaker_codec, .data_if = data_if};
    speaker = esp_codec_dev_new(&device);
    device.dev_type = ESP_CODEC_DEV_TYPE_IN; device.codec_if = mic_codec;
    mic = esp_codec_dev_new(&device);
    if (!speaker || !mic) { err = ESP_ERR_NO_MEM; goto error; }
    ESP_LOGI("xiaozhi_audio", "LCD-X TDM input: MIC1 + MIC3 playback reference");
    return ESP_OK;
error:
    release_partial_init();
    return err;
}

esp_err_t xiaozhi_audio_open(int volume)
{
    if (volume < 0 || volume > 80) return ESP_ERR_INVALID_ARG;
    esp_err_t err = initialize();
    if (err != ESP_OK) return err;
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
        esp_codec_dev_sample_info_t info = {.sample_rate = 16000, .channel = 4, .bits_per_sample = 16,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1)};
        if (esp_codec_dev_open(mic, &info) != ESP_CODEC_DEV_OK) return ESP_FAIL;
        /* Gain masks address physical ADCs, not their interleaved TDM slots.
         * The board attenuates the electrical reference before MIC3. Restore
         * its level for AEC's far-end detector; 0 dB left audible residual
         * echo in the COM24 trial. 24 dB keeps headroom at output volume 80. */
        if (xiaozhi_audio_configure_input_gain(mic_codec) != ESP_CODEC_DEV_OK) {
            (void)esp_codec_dev_close(mic);
            return ESP_FAIL;
        }
    } else {
        if (esp_codec_dev_close(mic) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    }
    recording = on;
    return ESP_OK;
}

esp_err_t xiaozhi_audio_read(int16_t *stereo, int frames)
{
    if (!recording) return ESP_ERR_INVALID_STATE;
    if (!stereo || frames <= 0 || frames > 1920) return ESP_ERR_INVALID_ARG;
    return esp_codec_dev_read(mic, stereo, frames * 4) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t xiaozhi_audio_write(int16_t *mono, int samples)
{
    if (!opened) return ESP_ERR_INVALID_STATE;
    if (!mono || samples <= 0 || samples > 1920) return ESP_ERR_INVALID_ARG;
    return esp_codec_dev_write(speaker, mono, samples * 2) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

void xiaozhi_audio_close(void)
{
    if (recording) xiaozhi_audio_mic(false);
    if (opened) esp_codec_dev_close(speaker);
    opened = false;
}
