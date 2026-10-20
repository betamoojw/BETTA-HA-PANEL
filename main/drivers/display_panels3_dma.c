/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * S3-only link wrappers: configure the RGB driver's otherwise private DMA
 * allocation without modifying the installed IDF or unrelated peripherals.
 */
#include "drivers/display_panels3_dma.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_private/gdma.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/interrupts.h"

static TaskHandle_t s_constructor_task;
static gdma_channel_handle_t s_rgb_dma;
static portMUX_TYPE s_counter_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_isr_resets;

esp_err_t __real_gdma_new_ahb_channel(const gdma_channel_alloc_config_t *, gdma_channel_handle_t *, gdma_channel_handle_t *);
esp_err_t __real_gdma_reset(gdma_channel_handle_t);
esp_err_t __real_esp_intr_alloc_intrstatus(int, int, uint32_t, uint32_t, intr_handler_t, void *, intr_handle_t *);

esp_err_t __wrap_gdma_new_ahb_channel(const gdma_channel_alloc_config_t *config,
                                    gdma_channel_handle_t *tx, gdma_channel_handle_t *rx)
{
    if (xTaskGetCurrentTaskHandle() != s_constructor_task || !config || !tx || rx)
        return __real_gdma_new_ahb_channel(config, tx, rx);
    gdma_channel_alloc_config_t adjusted = *config;
    /* Refill must preempt ordinary peripheral interrupts, not only LVGL tasks.
     * Level 3 remains compatible with the driver's FreeRTOS ISR callbacks. */
    adjusted.intr_priority = 3;
    esp_err_t err = __real_gdma_new_ahb_channel(&adjusted, tx, rx);
    if (err == ESP_OK) s_rgb_dma = *tx;
    return err;
}

esp_err_t __wrap_esp_intr_alloc_intrstatus(int source, int flags, uint32_t reg,
                                         uint32_t mask, intr_handler_t handler,
                                         void *arg, intr_handle_t *handle)
{
    if (source == ETS_LCD_CAM_INTR_SOURCE && xTaskGetCurrentTaskHandle() == s_constructor_task)
        /* DMA EOF must preempt VSYNC: the RGB VSYNC handler checks the EOF
         * count before deciding to restart. Equal priorities can let VSYNC
         * inspect a stale count while the final DMA EOF is still pending. */
        flags = (flags & ~ESP_INTR_FLAG_LOWMED) | ESP_INTR_FLAG_LEVEL2;
    return __real_esp_intr_alloc_intrstatus(source, flags, reg, mask, handler, arg, handle);
}

esp_err_t IRAM_ATTR __wrap_gdma_reset(gdma_channel_handle_t channel)
{
    /* Count VSYNC resets, including explicit post-write recovery requests.
     * Initialization/task resets and other peripherals are excluded. */
    if (channel == s_rgb_dma && xPortInIsrContext()) {
        portENTER_CRITICAL_ISR(&s_counter_lock);
        s_isr_resets++;
        portEXIT_CRITICAL_ISR(&s_counter_lock);
    }
    return __real_gdma_reset(channel);
}

static void report_dma(void *arg)
{
    (void)arg;
    static uint32_t last = UINT32_MAX;
    portENTER_CRITICAL(&s_counter_lock);
    uint32_t count = s_isr_resets;
    portEXIT_CRITICAL(&s_counter_lock);
    if (count != last) {
        ESP_LOGI("display", "RGB DMA ISR recoveries: %lu (LCD priority 2, GDMA priority 3)", (unsigned long)count);
        last = count;
    }
}

void display_panels3_dma_begin(void) { s_constructor_task = xTaskGetCurrentTaskHandle(); }
void display_panels3_dma_end(void)
{
    s_constructor_task = NULL;
    const esp_timer_create_args_t args = {.callback = report_dma, .name = "rgb_dma_stats"};
    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) == ESP_OK) {
        if (esp_timer_start_periodic(timer, 10000000) != ESP_OK) esp_timer_delete(timer);
    }
}
