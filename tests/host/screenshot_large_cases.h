/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Included by test_screenshot.c: real handler/codec logic, simulated LVGL/JPEG/HTTP.
 * Byte counts are application-owned payloads, never hardware heap measurements.
 */
#define PANEL_W 1280U
#define PANEL_H 800U
#define RGB_BYTES (PANEL_W * PANEL_H * 3U)
#define ARGB_BYTES (PANEL_W * PANEL_H * 4U)

static void fill_large(int overlay)
{
    opaque_cover = overlay == 2;
    lv_draw_buf_t *rgb = make_buf(PANEL_W, PANEL_H, LV_COLOR_FORMAT_RGB888, 3, PANEL_W * 3);
    for (size_t p = 0; p < RGB_BYTES; p += 3) {
        rgb->data[p] = 10; rgb->data[p + 1] = 20; rgb->data[p + 2] = 30;
    }
    if (opaque_cover) { top_buf = rgb; return; }
    page_buf = rgb;
    top_buf = make_buf(PANEL_W, PANEL_H, LV_COLOR_FORMAT_ARGB8888, 4, PANEL_W * 4);
    if (overlay) {
        top_buf->data[0] = 200; top_buf->data[3] = 255;
        top_buf->data[ARGB_BYTES - 4] = 100; top_buf->data[ARGB_BYTES - 1] = 128;
    }
}

static void check_released(void)
{
    CHECK(!locked && live_bufs == 0 && engines == 0 && jpeg_mem == 0);
    CHECK(snapshot_bytes == 0 && heap_bytes == 0);
}

static void test_large_screenshots(httpd_req_t *req)
{
    esp_err_t (*handlers[])(httpd_req_t *) = {api_screenshot_bmp_get_handler, api_screenshot_png_get_handler,
#if SOC_JPEG_ENCODE_SUPPORTED
        api_screenshot_jpg_get_handler,
#endif
    };
    for (size_t handler = 0; handler < sizeof(handlers) / sizeof(handlers[0]); handler++) {
        for (int overlay = 0; overlay < 3; overlay++) {
            reset(); fill_large(overlay);
            CHECK(handlers[handler](req) == ESP_OK && !status[0]);
            check_released();
            size_t capture_peak = overlay == 2 ? RGB_BYTES : overlay == 1 ? RGB_BYTES + ARGB_BYTES : ARGB_BYTES;
            CHECK(peak_snapshot_bytes == capture_peak);
            size_t encode_peak = RGB_BYTES + peak_heap_bytes;
            CHECK(peak_bytes == (capture_peak > encode_peak ? capture_peak : encode_peak));
            CHECK(lock_calls == 1 && unlock_calls == 1 && take_calls == (overlay == 2 ? 1 : 2));
            if (handler == 1) {
                uint8_t *rgb = malloc(RGB_BYTES);
                CHECK(decode_png(PANEL_W, PANEL_H, rgb));
                bool pixels_ok = true;
                for (size_t p = 0; p < RGB_BYTES; p += 3) {
                    uint8_t r = 30, g = 20, b = 10;
                    if (overlay == 1 && p == 0) { r = g = 0; b = 200; }
                    if (overlay == 1 && p == RGB_BYTES - 3) {
                        r = 30 * 127 / 255; g = 20 * 127 / 255; b = (100 * 128 + 10 * 127) / 255;
                    }
                    if (rgb[p] != r || rgb[p + 1] != g || rgb[p + 2] != b) pixels_ok = false;
                }
                CHECK(pixels_ok); free(rgb);
            } else if (handler == 0) {
                CHECK(body_len == RGB_BYTES + 54 && chunks == PANEL_H + 2);
                CHECK(body[54] == 10 && body[54 + (PANEL_H - 1) * PANEL_W * 3] == (overlay == 1 ? 200 : 10));
            }
            printf("1280x800 handler=%zu overlay=%d: capture=%zu heap=%zu combined=%zu payload bytes\n",
                   handler, overlay, peak_snapshot_bytes, peak_heap_bytes, peak_bytes);
            int allocation_count = heap_calls;
            for (int n = 1; n <= allocation_count; n++) {
                reset(); fill_large(overlay); fail_alloc_at = n;
                CHECK(handlers[handler](req) == ESP_OK && !strcmp(status, "500 Internal Server Error"));
                CHECK(body_len == 0); check_released();
            }
        }
        /* A competing display owner causes a bounded busy response. The same
         * request succeeds once that owner releases it; no server reset needed. */
        reset(); fill_large(1); lock_ok = false;
        CHECK(handlers[handler](req) == ESP_OK && !strcmp(status, "503 Service Unavailable"));
        CHECK(take_calls == 0 && heap_calls == 0 && unlock_calls == 0);
        lock_ok = true; status[0] = '\0';
        CHECK(handlers[handler](req) == ESP_OK && !status[0]); check_released();
        for (int n = 1; n <= 2; n++) {
            reset(); fill_large(1); fail_take_at = n;
            CHECK(handlers[handler](req) == ESP_OK && !strcmp(status, "500 Internal Server Error"));
            CHECK(unlock_calls == 1 && body_len == 0); check_released();
        }
        reset(); fill_large(1); fail_take_at = 2; fail_chunk_at = -2;
        CHECK(handlers[handler](req) == ESP_FAIL); check_released();
        /* Header/first row/middle row/end failures, or a disconnected compressed
         * response. Sending never holds the display lock, including errors. */
        int failures_at[] = {1, 2, 401, PANEL_H + 2};
        for (size_t n = 0; n < (handler == 0 ? 4U : 1U); n++) {
            reset(); fill_large(1); fail_chunk_at = handler == 0 ? failures_at[n] : -1;
            CHECK(handlers[handler](req) == ESP_FAIL); check_released();
        }
        /* No reset between consecutive requests: resource counts must return to
         * zero each time and a failed send must not poison the next response. */
        reset();
        for (int n = 0; n < 8; n++) {
            cases++; fill_large(n % 3); body_len = 0; chunks = 0;
            fail_chunk_at = n % 2 ? 0 : handler == 0 ? 2 : -1;
            CHECK(handlers[handler](req) == (n % 2 ? ESP_OK : ESP_FAIL)); check_released();
        }
    }
    /* Exercise real zlib output exhaustion, then failures during rows and finish. */
    reset(); fill_large(0);
    uint32_t noise = 1;
    for (size_t i = 0; i < RGB_BYTES; i++) {
        noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
        page_buf->data[i] = (uint8_t)noise;
    }
    CHECK(api_screenshot_png_get_handler(req) == ESP_OK && !strcmp(status, "500 Internal Server Error"));
    CHECK(body_len == 0); check_released();
    for (int n = 0; n < 2; n++) {
        reset(); fill_large(1); fail_deflate_at = n ? PANEL_H + 1 : 1;
        CHECK(api_screenshot_png_get_handler(req) == ESP_OK && !strcmp(status, "500 Internal Server Error"));
        CHECK(body_len == 0); check_released();
    }
#if SOC_JPEG_ENCODE_SUPPORTED
    for (int n = 0; n < 5; n++) {
        reset(); fill_large(1);
        fail_engine_new = n == 0; fail_jpeg_process = n == 1;
        if (n == 2) jpeg_size_to_write = 0;
        if (n == 3) jpeg_size_to_write = 3;
        if (n == 4) jpeg_size_to_write = (1U << 20) + 1;
        CHECK(api_screenshot_jpg_get_handler(req) == ESP_OK && !strcmp(status, "500 Internal Server Error"));
        CHECK(body_len == 0); check_released();
    }
#endif
}
