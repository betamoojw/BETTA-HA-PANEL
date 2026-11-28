/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Assistant lifecycle, commands and status shared with HTTP and LVGL.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "lvgl.h"
#include "xiaozhi_custom_wake.h"
#include "xiaozhi_spectrum.h"

/** @brief Saved wake model ID and canonical pinyin phrase. */
typedef struct
{
    int word;
    char phrase[XIAOZHI_WAKE_PHRASE_SIZE];
} xiaozhi_wake_config_t;

/** @brief Assistant state copied for HTTP and LVGL; contains no borrowed pointers. */
typedef struct
{
    bool supported, enabled, paired, listening, wake_enabled, wake_ready, wake_running, conversation;
    bool duplex_enabled, duplex_active, speaking;
    uint8_t spectrum_mic[XIAOZHI_SPECTRUM_BANDS], spectrum_speaker[XIAOZHI_SPECTRUM_BANDS];
    bool spectrum_mic_active, spectrum_speaker_active;
    uint32_t audio_tx_frames, audio_rx_frames, audio_tx_during_playback, barge_ins;
    uint32_t aec_frames, aec_playback_frames, audio_capture_drops;
    int aec_mic_peak, aec_ref_peak, aec_clean_peak, aec_processing_us;
    int aec_ref_peak_max_playback;
    uint32_t capture_stack_free, playback_stack_free;
    uint32_t normal_closes, connection_errors;
    int connection_close_code, connection_error_type, connection_tls_error;
    int connection_socket_errno, connection_http_status;
    char connection_end[32]; /* Last real failure; normal closes never erase its diagnosis. */
    xiaozhi_wake_config_t wake_config;
    bool wake_model_ready, wake_config_failed;
    uint32_t wake_config_revision;
    int volume, dialog_timeout, emoji_style; /* 0 Otto (default), 1 classic */
    /** @brief Dialog generation, audio activity revision and boot-specific command epoch. */
    uint32_t dialog_seq, activity_seq, dialog_epoch;
    bool dialog_active, dialog_voice_opened;
    uint32_t dialog_end_seq, silence_remaining_ms;
    char dialog_end_reason[24]; /* End applies only to dialog_end_seq, not a later wake. */
    uint32_t wake_frames, wake_hits;
    int wake_mic_peak, wake_ref_peak, wake_detect_us; /* Diagnostics, no audio retained. */
    char state[32], code[24], text[512], recognized[512], reply[512], emotion[24];
} xiaozhi_status_t;

/** @brief Initialize session state and load settings; the audio worker starts on demand. */
void xiaozhi_init(void);

/** @brief Copy assistant state; may wait for the state mutex. Use xiaozhi_status_try() from LVGL. */
void xiaozhi_status(xiaozhi_status_t *out);
/** @brief Snapshot UI state without waiting or checking resource files.
 * @param[out] out Receives the status; remains unchanged on lock contention.
 * @return True when a fresh status was copied.
 */
bool xiaozhi_status_try(xiaozhi_status_t *out);
/** @brief Submit an action without calling LVGL or waiting on audio.
 * @param action NUL-terminated command name.
 * @param value Action-specific number, such as a volume percentage or dialog generation.
 * @return ESP_OK means the request was accepted, not that the worker completed it.
 */
esp_err_t xiaozhi_command(const char *action, int value);

/** @brief Submit a wake configuration to the session worker.
 * @param word Wake model ID; the first ID after the built-in models selects the custom phrase.
 * @param phrase Canonical pinyin copied into the command.
 * @return ESP_OK on acceptance. Observe the wake configuration revision for the worker result.
 */
esp_err_t xiaozhi_configure_wake(int word, const char *phrase);
/** @brief Persist the emoji style from an HTTP or settings worker, never under the display lock. */
esp_err_t xiaozhi_set_emoji_style(int value);
/** @brief Register assistant routes, including upload routes on LCD-X.
 * @return ESP_OK on success or the first registration error; earlier registrations are not rolled back.
 */
esp_err_t xiaozhi_api_register(httpd_handle_t server);

/** @brief Attach the assistant UI to the screen. Call from the UI task or with the display lock held. */
void xiaozhi_ui_create(lv_obj_t *screen);
