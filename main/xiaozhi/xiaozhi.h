/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "lvgl.h"
#include "xiaozhi_custom_wake.h"
#include "xiaozhi_spectrum.h"
typedef struct { int word; char phrase[XIAOZHI_WAKE_PHRASE_SIZE]; } xiaozhi_wake_config_t;

typedef struct {
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
    uint32_t dialog_seq, activity_seq, dialog_epoch;
    bool dialog_active, dialog_voice_opened;
    uint32_t dialog_end_seq, silence_remaining_ms;
    char dialog_end_reason[24]; /* End applies only to dialog_end_seq, not a later wake. */
    uint32_t wake_frames, wake_hits;
    int wake_mic_peak, wake_ref_peak, wake_detect_us; /* Diagnostics, no audio retained. */
    char state[32], code[24], text[512], recognized[512], reply[512], emotion[24];
} xiaozhi_status_t;

void xiaozhi_init(void);
void xiaozhi_status(xiaozhi_status_t *out);
/* UI snapshot: never waits or checks resource files. On contention, leaves
 * out unchanged; callers retain their previous view until the next refresh. */
bool xiaozhi_status_try(xiaozhi_status_t *out);
/* Commands are serialized by a worker; these never call LVGL or wait on audio. */
esp_err_t xiaozhi_command(const char *action, int value);
esp_err_t xiaozhi_configure_wake(int word, const char *phrase);
/* Storage operation: only HTTP/settings workers, never under the display lock. */
esp_err_t xiaozhi_set_emoji_style(int value);
esp_err_t xiaozhi_api_register(httpd_handle_t server);
void xiaozhi_ui_create(lv_obj_t *screen);
