# LCD-X BSP source

Waveshare LCD-X BSP 2.0.3, copied from
`waveshareteam/ESP32-P4-WIFI6-Touch-LCD-X` commit
`f15098fe62072156db1e7baf9a0945801f8a8877`, directory
`firmware/brookesia/components/esp32_p4_wifi6_touch_lcd_x`.

The original Apache-2.0 license and source headers are retained.
The manifest's esp_lvgl_adapter constraint is changed from `~0.6` to `0.7.9`
to match BETTA's shared display runtime. BETTA creates the panel and touch
handles through this BSP and registers them with its own adapter runtime.

Touch panel IO configures a 50 ms IDF timeout for transfers and shared-bus admission.
IDF 6.1.1 maps the original zero default to an infinite wait; a NACK with a busy
bus could otherwise spin indefinitely inside the LVGL input callback. Failed
GT911 construction also releases its panel IO before the next initialization
attempt. Probe addresses, GPIOs, clock speed and rotation remain as supplied.
IDF can consume this budget in multiple sequential waits; it is not a measured
50 ms end-to-end LVGL callback deadline.
