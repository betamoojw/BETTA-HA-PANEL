/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_activity.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE activity_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t current_generation;
static uint32_t revision;
static int64_t last_audio_us;
static bool active;

void xiaozhi_activity_begin(uint32_t generation)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&activity_lock);
    current_generation = generation; last_audio_us = now; active = true; ++revision;
    portEXIT_CRITICAL(&activity_lock);
}
void xiaozhi_activity_end(uint32_t generation)
{
    portENTER_CRITICAL(&activity_lock);
    if (generation == current_generation) active = false;
    portEXIT_CRITICAL(&activity_lock);
}
uint32_t xiaozhi_activity_generation(void)
{
    portENTER_CRITICAL(&activity_lock);
    uint32_t result = current_generation;
    portEXIT_CRITICAL(&activity_lock);
    return result;
}
uint32_t xiaozhi_activity_revision(void)
{
    portENTER_CRITICAL(&activity_lock);
    uint32_t result = revision;
    portEXIT_CRITICAL(&activity_lock);
    return result;
}
void xiaozhi_activity_note(uint32_t generation, int64_t observed_us)
{
    portENTER_CRITICAL(&activity_lock);
    if (active && generation == current_generation && observed_us > last_audio_us) {
        last_audio_us = observed_us; ++revision;
    }
    portEXIT_CRITICAL(&activity_lock);
}
void xiaozhi_activity_note_playback(uint32_t generation, int64_t observed_us)
{
    xiaozhi_activity_note(generation, observed_us + (int64_t)XIAOZHI_OUTPUT_TAIL_MS * 1000);
}
uint32_t xiaozhi_activity_remaining_ms(uint32_t generation)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&activity_lock);
    int64_t left = (int64_t)XIAOZHI_SILENCE_TIMEOUT_MS * 1000 - (now - last_audio_us);
    uint32_t result = active && generation == current_generation && left > 0 ?
        (left >= (int64_t)XIAOZHI_SILENCE_TIMEOUT_MS * 1000 ? XIAOZHI_SILENCE_TIMEOUT_MS : (uint32_t)((left + 999) / 1000)) : 0;
    portEXIT_CRITICAL(&activity_lock);
    return result;
}
bool xiaozhi_activity_expire(uint32_t generation)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&activity_lock);
    bool expired = active && generation == current_generation && now >= last_audio_us &&
        now - last_audio_us >= (int64_t)XIAOZHI_SILENCE_TIMEOUT_MS * 1000;
    if (expired) active = false;
    portEXIT_CRITICAL(&activity_lock);
    return expired;
}
bool xiaozhi_activity_has_sound(const int16_t *pcm, size_t samples)
{
    if (!pcm || samples < 2 || samples > 1920) return false;
    int64_t sum = 0, squares = 0;
    for (size_t i = 0; i < samples; ++i) {
        int64_t value = pcm[i]; sum += value; squares += value * value;
    }
    /* DC-removed RMS >= 32/32768 (-60 dBFS). This never decides a barge-in. */
    int64_t count = (int64_t)samples;
    return squares * count - sum * sum >= count * count * 32 * 32;
}
