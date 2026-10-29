/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- see docs/release-notes.md
 */
#include "drivers/display_present.h"
#include <stdatomic.h>
#include <string.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct display_present {
    esp_lcd_panel_handle_t panel;
    uint16_t width, height;
    uint16_t *front, *back;
    SemaphoreHandle_t completed;
    atomic_uint frame_seq;
    unsigned submitted;
    int64_t deadline;
    display_rect_t dirty;
    bool pending, dirty_valid;
    bool failed;
};

static bool IRAM_ATTR frame_complete(esp_lcd_panel_handle_t panel,
                                     esp_lcd_dpi_panel_event_data_t *event, void *user)
{
    (void)panel;
    (void)event;
    display_present_t *ctx = user;
    atomic_fetch_add_explicit(&ctx->frame_seq, 1, memory_order_release);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->completed, &woken);
    return woken == pdTRUE;
}

display_present_t *display_present_create(esp_lcd_panel_handle_t panel,
                                          uint16_t width, uint16_t height)
{
    if (!panel || !width || !height) return NULL;
    display_present_t *ctx = heap_caps_calloc(1, sizeof(*ctx), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ctx) return NULL;
    ctx->panel = panel;
    ctx->width = width;
    ctx->height = height;
    atomic_init(&ctx->frame_seq, 0);
    /* IDF initializes the DPI driver on framebuffer zero. The adapter has not
     * started yet, so no application drawing or buffer switch can precede us. */
    void *front = NULL, *back = NULL;
    if (esp_lcd_dpi_panel_get_frame_buffer(panel, 2, &front, &back) != ESP_OK ||
            !front || !back || front == back) goto fail;
    ctx->front = front;
    ctx->back = back;
    ctx->completed = xSemaphoreCreateBinary();
    if (!ctx->completed) goto fail;
    memcpy(ctx->back, ctx->front, (size_t)width * height * sizeof(uint16_t));
    /* DPI is already scanning, but no owner has installed callbacks. Set the
     * context while callbacks are null before publishing the ISR function. */
    const esp_lcd_dpi_panel_event_callbacks_t none = {0};
    if (esp_lcd_dpi_panel_register_event_callbacks(panel, &none, ctx) != ESP_OK) goto fail;
    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {.on_frame_buf_complete = frame_complete};
    if (esp_lcd_dpi_panel_register_event_callbacks(panel, &callbacks, ctx) != ESP_OK) goto fail;
    return ctx;
fail:
    if (ctx->completed) vSemaphoreDelete(ctx->completed);
    heap_caps_free(ctx);
    return NULL;
}

static esp_err_t acquire_back_buffer(display_present_t *ctx)
{
    if (!ctx->pending) return ESP_OK;
    /* A callback racing the submission may belong to the previous frame.
     * Require two later DMA frame completions before reusing the old front.
     * Wait on the next write, allowing the UI lock to be released after submit.
     * Semaphore tokens are wakeups only; the sequence rejects stale tokens. */
    while ((unsigned)(atomic_load_explicit(&ctx->frame_seq, memory_order_acquire) - ctx->submitted) < 2U) {
        const int64_t left_us = ctx->deadline - esp_timer_get_time();
        if (left_us <= 0) { ctx->failed = true; return ESP_ERR_TIMEOUT; }
        xSemaphoreTake(ctx->completed, pdMS_TO_TICKS((left_us + 999) / 1000));
    }
    uint16_t *old_front = ctx->front;
    ctx->front = ctx->back;
    ctx->back = old_front;
    /* Both buffers began equal. Repair only the preceding frame's changed
     * bounds, instead of copying 2 MiB for a clock tick or tiny indicator. */
    const size_t bytes = (size_t)(ctx->dirty.x2 - ctx->dirty.x1) * sizeof(uint16_t);
    for (int y = ctx->dirty.y1; y < ctx->dirty.y2; ++y) {
        const size_t offset = (size_t)y * ctx->width + ctx->dirty.x1;
        memcpy(ctx->back + offset, ctx->front + offset, bytes);
    }
    ctx->pending = ctx->dirty_valid = false;
    return ESP_OK;
}

esp_err_t display_present_write(display_present_t *ctx, display_rect_t area,
                                const void *pixels, bool last)
{
    if (!ctx || !pixels) return ESP_ERR_INVALID_ARG;
    if (ctx->failed) return ESP_ERR_INVALID_STATE;
    if (area.x1 < 0 || area.y1 < 0 || area.x2 > ctx->width || area.y2 > ctx->height ||
            area.x2 <= area.x1 || area.y2 <= area.y1) return ESP_ERR_INVALID_ARG;
    esp_err_t err = acquire_back_buffer(ctx);
    if (err != ESP_OK) return err;
    const uint16_t *src = pixels;
    const size_t row_pixels = area.x2 - area.x1;
    for (int y = area.y1; y < area.y2; ++y) {
        memcpy(ctx->back + (size_t)y * ctx->width + area.x1, src, row_pixels * sizeof(uint16_t));
        src += row_pixels;
    }
    if (!ctx->dirty_valid) ctx->dirty = area;
    else {
        if (area.x1 < ctx->dirty.x1) ctx->dirty.x1 = area.x1;
        if (area.y1 < ctx->dirty.y1) ctx->dirty.y1 = area.y1;
        if (area.x2 > ctx->dirty.x2) ctx->dirty.x2 = area.x2;
        if (area.y2 > ctx->dirty.y2) ctx->dirty.y2 = area.y2;
    }
    ctx->dirty_valid = true;
    if (!last) return ESP_OK;

    /* A framebuffer pointer selects IDF's cache-writeback + page-flip path;
     * no DMA2D copy can overwrite the buffer currently being scanned out. */
    err = esp_lcd_panel_draw_bitmap(ctx->panel, 0, 0, ctx->width, ctx->height, ctx->back);
    if (err != ESP_OK) { ctx->failed = true; return err; }
    ctx->submitted = atomic_load_explicit(&ctx->frame_seq, memory_order_acquire);
    ctx->deadline = esp_timer_get_time() + 1000000;
    ctx->pending = true;
    return ESP_OK;
}
