# LCD-X BSP source

Waveshare LCD-X BSP 2.0.3, copied from
`waveshareteam/ESP32-P4-WIFI6-Touch-LCD-X` commit
`f15098fe62072156db1e7baf9a0945801f8a8877`, directory
`firmware/brookesia/components/esp32_p4_wifi6_touch_lcd_x`.

The original Apache-2.0 license and source headers are retained.
The manifest's esp_lvgl_adapter constraint is changed from `~0.6` to `0.7.3`
to match BETTA's shared display runtime. BETTA creates the panel and touch
handles through this BSP and registers them with its own adapter runtime.
