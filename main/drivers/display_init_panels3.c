/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 -- see docs/release-notes.md
 * Guition ESP32-S3-4848S040: ST7701S, 3-wire SPI + RGB565.
 * Wiring and register table restored from upstream fc15f27; see
 * docs/hardware/panels3.md for sources and validation limits.
 */
#include "drivers/display_init.h"
#include "drivers/display_backlight.h"
#include "drivers/display_port.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_st7701.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "util/log_tags.h"

#define PANELS3_BL_GPIO 38
#define PANELS3_SPI_CLK 48
#define PANELS3_SPI_MOSI 47
#define PANELS3_SPI_CS 39
#define PANELS3_PCLK_GPIO 21
#define PANELS3_DE_GPIO 18
#define PANELS3_HSYNC_GPIO 16
#define PANELS3_VSYNC_GPIO 17
/* Low to high: B1..B5, G0..G5, R1..R5. */
#define PANELS3_DATA_GPIOS {4, 5, 6, 7, 15, 8, 20, 3, 46, 9, 10, 11, 12, 13, 14, 0}
/* Direct PSRAM scanout shares bandwidth with the CPU. Keep the pixel clock
 * bounded to leave headroom for Wi-Fi, fonts and browser mirror transfers. */
#define PANELS3_PCLK_HZ 8000000
#define PANELS3_H_PULSE 8
#define PANELS3_H_FRONT 10
#define PANELS3_H_BACK 50
#define PANELS3_V_PULSE 8
#define PANELS3_V_FRONT 10
#define PANELS3_V_BACK 20

static bool s_display_ready;
static lv_display_t *s_lv_display;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_panel_init_done;
static esp_err_t s_panel_init_result;
static bool s_panel_init_started;

static esp_err_t backlight_ledc_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0, .freq_hz = 150, .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG_DISPLAY, "Backlight timer");
    const ledc_channel_config_t channel = {
        .gpio_num = PANELS3_BL_GPIO, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0,
    };
    return ledc_channel_config(&channel);
}

esp_err_t display_backlight_write(int percent)
{
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0,
                        (uint32_t)percent * 1023U / 100U), TAG_DISPLAY, "Backlight duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static const st7701_lcd_init_cmd_t s_guition_4848_init[] = {
    /* PAGE0 — display timing, interface, gamma */
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t []){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t []){0x31, 0x05}, 2, 0},   /* line/inversion drive tune */
    {0xCD, (uint8_t []){0x00}, 1, 0},
    /* Positive Voltage Gamma Control */
    {0xB0, (uint8_t []){0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08,
                         0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18}, 16, 0},
    /* Negative Voltage Gamma Control */
    {0xB1, (uint8_t []){0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08,
                         0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18}, 16, 0},

    /* PAGE1 — power / charge-pump (Arduino_GFX type1 values) */
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t []){0x60}, 1, 0},   /* Vop = 4.7375 V */
    {0xB1, (uint8_t []){0x32}, 1, 0},   /* VCOM/drive tune */
    {0xB2, (uint8_t []){0x07}, 1, 0},   /* VGH/drive tune */
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x49}, 1, 0},   /* VGL = -10.17 V */
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x21}, 1, 0},   /* AVDD = 6.6, AVCL = -4.6 */
    {0xC1, (uint8_t []){0x78}, 1, 0},
    {0xC2, (uint8_t []){0x78}, 1, 0},

    /* EQ / GIP timing */
    {0xE0, (uint8_t []){0x00, 0x1B, 0x02}, 3, 0},
    {0xE1, (uint8_t []){0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00,
                         0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t []){0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00,
                         0xEC, 0xA0, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t []){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0,
                         0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0}, 16, 0},
    {0xE6, (uint8_t []){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0,
                         0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0}, 16, 0},
    {0xEB, (uint8_t []){0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40}, 7, 0},
    {0xEC, (uint8_t []){0x3C, 0x00}, 2, 0},
    {0xED, (uint8_t []){0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF,
                         0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA}, 16, 0},

    /* PAGE3 — VAP & VAN (Arduino_GFX includes this; HSD txt did not) */
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE5, (uint8_t []){0xE4}, 1, 0},

    /* Back to user command page */
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},

    /* Override COLMOD: the component already sent 0x50 (RGB565); switch to
     * 0x60 (RGB666 internal pixel mode) — required by the C2 LINE_SET above. */
    {0x3A, (uint8_t []){0x60}, 1, 0},

    /* Sleep Out + Display On (per Arduino_GFX timing) */
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 0},
};

static esp_err_t panel_create(void)
{
    /* 3-wire SPI IO (GPIO bit-bang, no hardware SPI peripheral) */
    const spi_line_config_t line_config = {
        .cs_io_type  = IO_TYPE_GPIO,
        .cs_gpio_num = PANELS3_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = PANELS3_SPI_CLK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = PANELS3_SPI_MOSI,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg =
        ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    io_cfg.spi_mode = 3;  /* ST7701S uses MODE3 (CPOL=1,CPHA=1) — ESPHome: spi_mode: MODE3 */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io_handle),
        TAG_DISPLAY, "3-wire SPI IO init failed");

    /* RGB parallel panel config (inside vendor_config) */
    const esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src             = LCD_CLK_SRC_DEFAULT,
        /* PSRAM access via GDMA is hardware-limited to ≤64 byte bursts on
         * ESP32-S3 (gdma_config_transfer rejects >64 for external memory). */
        .dma_burst_size      = 64,
        .data_width          = 16,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        .in_color_format     = LCD_COLOR_FMT_RGB565,
        .out_color_format    = LCD_COLOR_FMT_RGB565,
#else
        .bits_per_pixel      = 16,
#endif
        .de_gpio_num         = PANELS3_DE_GPIO,
        .pclk_gpio_num       = PANELS3_PCLK_GPIO,
        .vsync_gpio_num      = PANELS3_VSYNC_GPIO,
        .hsync_gpio_num      = PANELS3_HSYNC_GPIO,
        .disp_gpio_num       = GPIO_NUM_NC,
        .data_gpio_nums      = PANELS3_DATA_GPIOS,
        .timings = {
            .pclk_hz           = PANELS3_PCLK_HZ,
            .h_res             = APP_SCREEN_WIDTH,
            .v_res             = APP_SCREEN_HEIGHT,
            .hsync_pulse_width = PANELS3_H_PULSE,
            .hsync_front_porch = PANELS3_H_FRONT,
            .hsync_back_porch  = PANELS3_H_BACK,
            .vsync_pulse_width = PANELS3_V_PULSE,
            .vsync_front_porch = PANELS3_V_FRONT,
            .vsync_back_porch  = PANELS3_V_BACK,
            .flags.pclk_active_neg = 0,  /* pclk_inverted: False per ESPHome Guition 4848S040 config */
        },
        /* Partial LVGL buffers copy into one PSRAM scanout framebuffer.
         * At this bounded pixel clock, direct EDMA avoids the hard real-time
         * CPU bounce-refill deadline and releases 19.2 KB of internal RAM. */
        .num_fbs = 1,
        .bounce_buffer_size_px = 0,
        .flags.fb_in_psram = 1,
    };

    /* ST7701S vendor config: board-specific init sequence for Guition 4848S040 */
    const st7701_vendor_config_t vendor_cfg = {
        .rgb_config      = &rgb_cfg,
        .init_cmds       = s_guition_4848_init,
        .init_cmds_size  = sizeof(s_guition_4848_init) / sizeof(st7701_lcd_init_cmd_t),
        /* enable_io_multiplex=1: release SPI GPIO after init, RGB takes over */
        .flags.enable_io_multiplex = 1,
    };

    const esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num  = GPIO_NUM_NC,          /* no dedicated reset pin */
        /* Pin wiring is B@LSB(D0..4), G(D5..10), R@MSB(D11..15) → bus is RGB565 byte
         * order. So MADCTL.BGR must be 0 (RGB). Setting BGR here would make the
         * panel swap R/B internally → cyan would render as yellow. */
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel  = 16,
        .vendor_config   = (void *)&vendor_cfg,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_st7701(io_handle, &panel_dev_cfg, &s_panel),
        TAG_DISPLAY, "esp_lcd_new_panel_st7701 failed");

    /* reset: sends ST7701S init sequence via SPI (and releases SPI IO) */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG_DISPLAY, "panel reset failed");
    /* init: starts the RGB panel DMA engine */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),  TAG_DISPLAY, "panel init failed");
    /* NOTE: disp_on_off is NOT called here — with enable_io_multiplex=1 the SPI IO
     * is already deleted at this point and the call would fail with an error.
     * The ST7701S init sequence has already enabled the display output. */

    ESP_LOGI(TAG_DISPLAY, "ST7701S panel ready (%dx%d, PCLK=%dHz, 1x FB in PSRAM, direct DMA)",
        APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, PANELS3_PCLK_HZ);
    return ESP_OK;
}

static void panel_init_task(void *arg)
{
    (void)arg;
    /* Keep RGB/GDMA completion callbacks on the LVGL core. Wi-Fi stays on CPU0. */
    ESP_LOGI(TAG_DISPLAY, "RGB/GDMA initialization on core %d (cache-safe ISR)", xPortGetCoreID());
    s_panel_init_result = panel_create();
    xSemaphoreGive(s_panel_init_done);
    vTaskDelete(NULL);
}

static esp_err_t panel_create_on_draw_core(void)
{
    if (s_panel_init_started) return ESP_ERR_INVALID_STATE;
    if (!s_panel_init_done) s_panel_init_done = xSemaphoreCreateBinary();
    if (!s_panel_init_done) return ESP_ERR_NO_MEM;
    s_panel_init_started = true;
    if (xTaskCreatePinnedToCore(panel_init_task, "rgb_init", 4096, NULL, 20, NULL, 1) != pdPASS) {
        s_panel_init_started = false;
        return ESP_ERR_NO_MEM;
    }
    /* Retain the tiny completion semaphore for the driver's lifetime. Even a
     * timed-out constructor can finish without using a deleted task/queue. */
    if (xSemaphoreTake(s_panel_init_done, pdMS_TO_TICKS(30000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    return s_panel_init_result;
}


esp_err_t display_init(void)
{
    if (s_display_ready) return ESP_OK;
    ESP_RETURN_ON_ERROR(backlight_ledc_init(), TAG_DISPLAY, "Backlight init");
    ESP_RETURN_ON_ERROR(display_port_init(), TAG_DISPLAY, "Adapter init");
    ESP_RETURN_ON_ERROR(panel_create_on_draw_core(), TAG_DISPLAY, "ST7701S init");
    esp_lv_adapter_display_config_t config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        s_panel, NULL, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, ESP_LV_ADAPTER_ROTATE_0);
    config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    config.profile.use_psram = true;
    config.profile.enable_ppa_accel = false;
    s_lv_display = esp_lv_adapter_register_display(&config);
    if (!s_lv_display) return ESP_ERR_NO_MEM;
    lv_display_set_default(s_lv_display);
    lv_display_set_antialiasing(s_lv_display, APP_LVGL_ANTIALIASING != 0);
    ESP_RETURN_ON_ERROR(display_port_start(), TAG_DISPLAY, "Adapter start");
    ESP_RETURN_ON_ERROR(display_brightness_start(), TAG_DISPLAY, "Brightness timer");
    s_display_ready = true;
    /* The shared boot splash policy turns the initially dark backlight on. */
    ESP_LOGI(TAG_DISPLAY, "Guition 4848S040: RGB 480x480, partial refresh, GPIO38 backlight");
    return ESP_OK;
}

bool display_is_ready(void) { return s_display_ready; }
bool display_lock(uint32_t timeout_ms) { return display_port_lock(timeout_ms); }
void display_unlock(void) { display_port_unlock(); }
esp_err_t bsp_display_backlight_off(void) { return display_set_brightness_percent(0); }
