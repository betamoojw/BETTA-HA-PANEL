/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "camera/camera_output.h"

#include "camera/camera_image.h"
#include "camera/camera_store.h"
#include "sdkconfig.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_APP_FEATURE_AMBIENT_CAMERA && CONFIG_APP_PANEL_VARIANT_10INCH_1280
#include "driver/jpeg_encode.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ui/display_geometry.h"
static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_mutex;
static portMUX_TYPE s_init_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_demand_until;
static _Atomic uint32_t s_sequence;
static uint8_t *s_jpeg;
static size_t s_length;
static uint32_t s_generation;
static int s_rotation;
static int64_t s_frame_ms;
static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}
static SemaphoreHandle_t mutex(void)
{
    portENTER_CRITICAL(&s_init_lock);
    if (!s_mutex)
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    portEXIT_CRITICAL(&s_init_lock);
    return s_mutex;
}
bool camera_output_available(void)
{
    return true;
}
int camera_output_rotation(void)
{
    /* LCD-X raw capture is upright at native pose 0 (user setting 90).
     * Turning the panel to keep the UI upright also turns the sensor. Undo
     * that physical turn in the image; using the display angle directly
     * leaves quarter-turn poses upside down. */
    return (360 - display_geometry_rotation()) % 360;
}
bool camera_output_needed(void)
{
    camera_config_t c;
    portENTER_CRITICAL(&s_init_lock);
    int64_t until = s_demand_until;
    portEXIT_CRITICAL(&s_init_lock);
    return now_ms() < until && camera_store_local(&c) && c.enabled;
}
void camera_output_invalidate(void)
{
    if (xSemaphoreTake(mutex(), pdMS_TO_TICKS(20)) != pdTRUE)
        return;
    free(s_jpeg);
    s_jpeg = NULL;
    s_length = 0;
    s_frame_ms = 0;
    xSemaphoreGive(s_mutex);
}
void camera_output_publish(const uint8_t *frame, unsigned w, unsigned h)
{
    camera_config_t c;
    uint32_t generation = camera_store_generation();
    if (!camera_output_needed() || !camera_store_local(&c) || !c.enabled)
        return;
    int ow, oh, rotation = camera_output_rotation();
    if (w > 800 || h > 640 || !camera_image_size(w, h, c.resolution, rotation, &ow, &oh))
        return;
    size_t rgb_size = (size_t) ow * oh * 2, allocated = 0, out_capacity = 0;
    jpeg_encode_memory_alloc_cfg_t in_cfg = { .buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER };
    jpeg_encode_memory_alloc_cfg_t out_cfg = { .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER };
    uint8_t *rgb = jpeg_alloc_encoder_mem(rgb_size, &in_cfg, &allocated);
    /* A high-entropy quality-95 JPEG can exceed the usual 4:1 estimate. */
    uint8_t *encoded = jpeg_alloc_encoder_mem(rgb_size + 65536, &out_cfg, &out_capacity);
    jpeg_encoder_handle_t engine = NULL;
    jpeg_encode_engine_cfg_t eng_cfg = { .intr_priority = 0, .timeout_ms = 1000 };
    if (!rgb || !encoded || !camera_image_render((const uint16_t *) frame, (int) w, (int) h, (uint16_t *) rgb, allocated, &c, rotation) ||
        jpeg_new_encoder_engine(&eng_cfg, &engine) != ESP_OK)
        goto done;
    jpeg_encode_cfg_t cfg = { .width = ow,
                              .height = oh,
                              .src_type = JPEG_ENCODE_IN_FORMAT_RGB565,
                              .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
                              .image_quality = c.jpeg_quality };
    uint32_t length = 0;
    if (jpeg_encoder_process(engine, &cfg, rgb, rgb_size, encoded, out_capacity, &length) != ESP_OK || !length || length > out_capacity ||
        generation != camera_store_generation() || rotation != camera_output_rotation() || !camera_output_needed())
        goto done;
    if (xSemaphoreTake(mutex(), pdMS_TO_TICKS(20)) == pdTRUE)
    {
        free(s_jpeg);
        s_jpeg = encoded;
        encoded = NULL;
        s_length = length;
        s_generation = generation;
        s_rotation = rotation;
        s_frame_ms = now_ms();
        atomic_fetch_add(&s_sequence, 1);
        xSemaphoreGive(s_mutex);
    }
done:
    if (engine)
        jpeg_del_encoder_engine(engine);
    free(rgb);
    free(encoded);
}
esp_err_t camera_output_snapshot(uint8_t **jpeg, size_t *length)
{
    if (!jpeg || !length)
        return ESP_ERR_INVALID_ARG;
    *jpeg = NULL;
    *length = 0;
    camera_config_t c;
    if (!camera_store_local(&c) || !c.enabled)
        return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&s_init_lock);
    s_demand_until = now_ms() + 4000;
    portEXIT_CRITICAL(&s_init_lock);
    int64_t deadline = now_ms() + 3000;
    do
    {
        if (!camera_store_local(&c) || !c.enabled)
            return ESP_ERR_INVALID_STATE;
        if (xSemaphoreTake(mutex(), pdMS_TO_TICKS(20)) == pdTRUE)
        {
            if (s_jpeg && s_generation == camera_store_generation() && s_rotation == camera_output_rotation() && now_ms() - s_frame_ms < 1500)
            {
                uint8_t *copy = heap_caps_malloc(s_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (copy)
                {
                    memcpy(copy, s_jpeg, s_length);
                    *jpeg = copy;
                    *length = s_length;
                }
                xSemaphoreGive(s_mutex);
                return copy ? ESP_OK : ESP_ERR_NO_MEM;
            }
            xSemaphoreGive(s_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    } while (now_ms() < deadline);
    return ESP_ERR_TIMEOUT;
}
uint32_t camera_output_sequence(void)
{
    return atomic_load(&s_sequence);
}
#else
int camera_output_rotation(void)
{
    return 0;
}
bool camera_output_available(void)
{
    return false;
}
bool camera_output_needed(void)
{
    return false;
}
void camera_output_publish(const uint8_t *p, unsigned w, unsigned h)
{
    (void) p;
    (void) w;
    (void) h;
}
void camera_output_invalidate(void)
{
}
esp_err_t camera_output_snapshot(uint8_t **p, size_t *n)
{
    if (p)
        *p = NULL;
    if (n)
        *n = 0;
    return ESP_ERR_NOT_SUPPORTED;
}
uint32_t camera_output_sequence(void)
{
    return 0;
}
#endif
