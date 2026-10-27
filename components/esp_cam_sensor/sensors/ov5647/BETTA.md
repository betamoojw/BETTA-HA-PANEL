# OV5647 source

Copied from the Espressif `esp_cam_sensor` 2.3.0 registry package, repository
commit `58d4c6eea08f0f78a2beff27aa4e0155efd74b55`.
Archive SHA256: `2d69928503bcc483c374b3b9b36a49f14caa9a068539a2eff360b72ceeb86e87`.
Original Apache-2.0 headers and the component license are preserved.

BETTA adds read-only `ESP_CAM_SENSOR_STATS`: actual automatic exposure from
registers 0x3500–0x3502 (lines), and gain from 0x350a–0x350b (unity = 16).
The upstream exposure setter still adjusts the sensor's automatic-exposure
target; it is not used as an exposure measurement. Failed reads return an error.
Register decoding is covered by the host ambient-camera test.

The LCD-X profile selects 800 × 640 RAW8 at 50 fps and the original ISP profile.
Capture is intermittent, with RGB565 buffers in PSRAM. A bounded LCD-X v3.2
trial on 2026-10-27 passed light response, approach/night wake and test expiry;
see `docs/hardware/panel10.md`. Wider lighting/distance calibration is pending.

References: [Espressif source](https://github.com/espressif/esp-video-components/tree/58d4c6eea08f0f78a2beff27aa4e0155efd74b55/esp_cam_sensor/sensors/ov5647),
[Waveshare camera example](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-X/tree/main/examples/esp-idf/09_video_lcd_display).
