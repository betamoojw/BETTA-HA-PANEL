/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
static void png_memory_cases(void)
{
    /* Less than 1 MiB free, no hole larger than 64 KiB: the old fixed output
     * failed before capture. Check every pixel, including full top-layer cover. */
    for (unsigned layer = 0; layer < 3; ++layer) {
        reset(); display_w = display_h = 480;
        popup = layer == 1; opaque_cover = layer == 2;
        total_limit = 900 * 1024; largest_limit = 64 * 1024;
        CHECK(invoke(0) == ESP_OK && !status[0]); clean(); verify_pixels(0);
        CHECK(largest_alloc <= 61440 && peak_bytes <= 900 * 1024);
        printf("PNG 480x480 layer=%u: peak=%zu largest=%zu encoded=%zu bytes\n",
               layer, peak_bytes, largest_alloc, body_len);
    }
    /* Incompressible content needs more total storage but never a large hole.
     * Exhaustion must produce a clean error; the next request can recover. */
    reset(); display_w = display_h = 480; noise = true;
    total_limit = 900 * 1024; largest_limit = 64 * 1024;
    CHECK(invoke(0) == ESP_OK && !strcmp(status,"500 Internal Server Error") && !body_len); clean();
    reset(); display_w = display_h = 480; noise = true;
    total_limit = 2 * 1024 * 1024; largest_limit = 64 * 1024;
    CHECK(invoke(0) == ESP_OK && !status[0]); clean(); verify_pixels(0);
    CHECK(body_len > 512 * 1024 && body_len <= 1024 * 1024);

    reset(); display_w = display_h = 128; noise = true;
    CHECK(invoke(0) == ESP_OK && !status[0]); clean(); verify_pixels(0);
    const int allocation_count = heap_calls, chunk_count = send_calls;
    CHECK(chunk_count > 3);
    for (int i = 1; i <= allocation_count; ++i) {
        reset(); display_w = display_h = 128; noise = true; fail_alloc_at = i;
        CHECK(invoke(0) == ESP_OK && !strcmp(status,"500 Internal Server Error") && !body_len); clean();
    }
    for (int i = 1; i <= chunk_count; ++i) {
        reset(); display_w = display_h = 128; noise = true; fail_send_at = i;
        CHECK(invoke(0) == ESP_FAIL && send_calls == i); clean();
    }
    /* Repeated capture after allocation/send failure does not retain bands,
     * zlib workspace or encoded blocks. */
    for (int i = 0; i < 4; ++i) {
        reset(); display_w = display_h = 480; opaque_cover = true;
        CHECK(invoke(0) == ESP_OK && !status[0]); clean(); verify_pixels(0);
    }
}
