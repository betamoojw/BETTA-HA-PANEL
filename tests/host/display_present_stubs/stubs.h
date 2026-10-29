/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define IRAM_ATTR
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_TIMEOUT 3
typedef int esp_err_t;
typedef int BaseType_t;
typedef void *SemaphoreHandle_t;
typedef void *esp_lcd_panel_handle_t;
typedef void esp_lcd_dpi_panel_event_data_t;
typedef bool (*frame_cb_t)(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *);
typedef struct { frame_cb_t on_frame_buf_complete; } esp_lcd_dpi_panel_event_callbacks_t;
void *heap_caps_calloc(size_t, size_t, unsigned);
void heap_caps_free(void *);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
void vSemaphoreDelete(SemaphoreHandle_t);
int xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t *);
int xSemaphoreTake(SemaphoreHandle_t, unsigned);
int64_t esp_timer_get_time(void);
esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t, uint32_t, void **, ...);
esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t, const esp_lcd_dpi_panel_event_callbacks_t *, void *);
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t, int, int, int, int, const void *);
