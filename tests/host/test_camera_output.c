/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "camera/camera_output.h"
#include "camera/camera_store.h"
#include "driver/jpeg_encode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, encodes, engines;
static int64_t clock_ms = 10000;
static uint32_t generation;
static int rotation;
static unsigned encoded_width, encoded_height;
static uint16_t encoded_first_pixel;
static bool rotate_in_encode;
static bool deliver, disable_in_encode, change_in_encode, fail_encode, fail_alloc;
static camera_config_t config;
static uint16_t pixels[32 * 16];
int display_geometry_rotation(void) { return rotation; }
#define CHECK(c)                                                          \
    do                                                                    \
    {                                                                     \
        checks++;                                                         \
        if (!(c))                                                         \
        {                                                                 \
            fprintf(stderr, "Camera output line %d: %s\n", __LINE__, #c); \
            exit(1);                                                      \
        }                                                                 \
    } while (0)
int64_t esp_timer_get_time(void)
{
    return clock_ms * 1000;
}
bool camera_store_local(camera_config_t *out)
{
    *out = config;
    return true;
}
uint32_t camera_store_generation(void)
{
    return generation;
}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *b)
{
    return b;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t wait)
{
    (void) m;
    (void) wait;
    return pdTRUE;
}
void xSemaphoreGive(SemaphoreHandle_t m)
{
    (void) m;
}
void *heap_caps_malloc(size_t n, unsigned caps)
{
    (void) caps;
    return fail_alloc ? NULL : malloc(n);
}
void vTaskDelay(TickType_t ms)
{
    clock_ms += ms;
    if (deliver)
        camera_output_publish((const uint8_t *) pixels, 32, 16);
}
void *jpeg_alloc_encoder_mem(size_t n, const jpeg_encode_memory_alloc_cfg_t *c, size_t *size)
{
    (void) c;
    *size = n;
    return fail_alloc ? NULL : malloc(n);
}
esp_err_t jpeg_new_encoder_engine(const jpeg_encode_engine_cfg_t *c, jpeg_encoder_handle_t *out)
{
    CHECK(c->timeout_ms == 1000);
    engines++;
    *out = (void *) 1;
    return ESP_OK;
}
void jpeg_del_encoder_engine(jpeg_encoder_handle_t h)
{
    (void) h;
    CHECK(engines > 0);
    engines--;
}
esp_err_t jpeg_encoder_process(jpeg_encoder_handle_t h,
                               const jpeg_encode_cfg_t *c,
                               const uint8_t *in,
                               size_t input_len,
                               uint8_t *out,
                               size_t cap,
                               uint32_t *size)
{
    (void) h;
    CHECK(input_len == c->width * c->height * 2 && cap >= 8 && c->image_quality == 55);
    encodes++;
    encoded_width = c->width;
    encoded_height = c->height;
    encoded_first_pixel = ((const uint16_t *) in)[0];
    if (rotate_in_encode) rotation = (rotation + 90) % 360;
    if (disable_in_encode)
        config.enabled = false;
    if (change_in_encode)
        generation++;
    memcpy(out,
           "\xff\xd8"
           "ABCD"
           "\xff\xd9",
           8);
    *size = 8;
    return fail_encode ? ESP_FAIL : ESP_OK;
}
static esp_err_t snapshot(void)
{
    uint8_t *data = NULL;
    size_t n = 0;
    esp_err_t err = camera_output_snapshot(&data, &n);
    if (err == ESP_OK)
        CHECK(data && n == 8);
    else
        CHECK(data == NULL && n == 0);
    free(data);
    CHECK(engines == 0);
    return err;
}
int main(void)
{
    for (unsigned i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        pixels[i] = (uint16_t) (i + 1);
    config.jpeg_quality = 55;
    config.image =
        (camera_image_config_t) { .contrast = 128, .saturation = 128, .wb_red = 100, .wb_blue = 100, .sharpen = 100, .denoise = 100 };
    CHECK(camera_output_available() && !camera_output_needed());
    CHECK(snapshot() == ESP_ERR_INVALID_STATE && encodes == 0);
    config.enabled = true;
    CHECK(!camera_output_needed());
    deliver = true;
    CHECK(snapshot() == ESP_OK && encodes == 1);
    CHECK(snapshot() == ESP_OK && encodes == 1); /* shared fresh cache, no second capture */
    CHECK(encoded_width == 32 && encoded_height == 16);
    CHECK(camera_output_rotation() == 0 && encoded_first_pixel == 1);
    rotation = 90;
    CHECK(snapshot() == ESP_OK && encodes == 2); /* Fresh cache from the old pose must be rejected. */
    CHECK(encoded_width == 16 && encoded_height == 32);
    /* User setting 180: the physically upright panel needs CCW image correction.
     * Dimensions alone cannot distinguish this from the upside-down CW result. */
    CHECK(camera_output_rotation() == 270 && encoded_first_pixel == 32);
    for (rotation = 180; rotation <= 270; rotation += 90) {
        CHECK(snapshot() == ESP_OK);
        CHECK(encoded_width == (rotation == 180 ? 32U : 16U));
        CHECK(camera_output_rotation() == (rotation == 180 ? 180 : 90));
        CHECK(encoded_first_pixel == (rotation == 180 ? 512 : 481));
    }
    rotation = 0;
    camera_output_invalidate();
    rotate_in_encode = true;
    CHECK(snapshot() == ESP_ERR_TIMEOUT); /* A pose change during encoding must not publish. */
    rotate_in_encode = false;
    rotation = 0;
    clock_ms += 4001;
    CHECK(!camera_output_needed());
    camera_output_invalidate();
    config.enabled = false;
    CHECK(snapshot() == ESP_ERR_INVALID_STATE);
    config.enabled = true;
    disable_in_encode = true;
    CHECK(snapshot() == ESP_ERR_INVALID_STATE);
    disable_in_encode = false;
    config.enabled = true;
    change_in_encode = true;
    CHECK(snapshot() == ESP_ERR_TIMEOUT);
    change_in_encode = false;
    fail_encode = true;
    CHECK(snapshot() == ESP_ERR_TIMEOUT);
    fail_encode = false;
    fail_alloc = true;
    CHECK(snapshot() == ESP_ERR_TIMEOUT);
    fail_alloc = false;
    CHECK(snapshot() == ESP_OK);
    config.resolution = 1;
    generation++;
    CHECK(snapshot() == ESP_OK);
    camera_output_invalidate();
    deliver = false;
    int64_t before = clock_ms;
    CHECK(snapshot() == ESP_ERR_TIMEOUT && clock_ms - before <= 3020);
    clock_ms += 4001;
    CHECK(!camera_output_needed());
    camera_output_invalidate();
    printf("Camera output: %u checks passed (opt-in, fresh cache, device expiry, disable/settings races, encoder cleanup)\n", checks);
    return 0;
}
