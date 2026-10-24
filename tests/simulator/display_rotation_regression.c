/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- see docs/release-notes.md
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "drivers/display_rotation.h"

/* Exercise the same LVGL packed RGB565 operation used by display_port.c.
 * Map each pixel through the native rectangle transform independently of the
 * block rotation, including the exact dirty areas retained in COM24 dumps. */
static void check(display_rect_t area, unsigned rotation, bool swap)
{
    const int w = area.x2 - area.x1, h = area.y2 - area.y1;
    const size_t n = (size_t)w * h;
    uint16_t *src = malloc(n * sizeof(*src));
    uint16_t *dst = malloc((n + 2) * sizeof(*dst));
    assert(src && dst);
    dst[0] = 0x1234;
    dst[n + 1] = 0xabcd;
    for (size_t i = 0; i < n; ++i) src[i] = (uint16_t)(i * 131U + 17U);
    display_rect_t bounds;
    assert(display_rotation_rect(area, 800, 1280, rotation, &bounds));
    const int stride = bounds.x2 - bounds.x1;
    lv_draw_sw_rotate(src, dst + 1, w, h, w * 2, stride * 2,
                      (lv_display_rotation_t)rotation, LV_COLOR_FORMAT_RGB565);
    if (swap) lv_draw_sw_rgb565_swap(dst + 1, n);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            display_rect_t pixel = {area.x1 + x, area.y1 + y,
                                    area.x1 + x + 1, area.y1 + y + 1};
            display_rect_t native;
            assert(display_rotation_rect(pixel, 800, 1280, rotation, &native));
            uint16_t expected = src[y * w + x];
            if (swap) expected = (uint16_t)((expected >> 8) | (expected << 8));
            assert(dst[1 + (native.y1 - bounds.y1) * stride + native.x1 - bounds.x1] == expected);
        }
    }
    assert(dst[0] == 0x1234 && dst[n + 1] == 0xabcd);
    free(src);
    free(dst);
}

int main(void)
{
    const display_rect_t areas[] = {
        {561, 610, 740, 676}, {561, 20, 739, 86},
        {556, 610, 744, 676}, {555, 610, 747, 676},
        {560, 20, 745, 86}, {0, 0, 1, 1}, {2, 3, 5, 8},
        {0, 0, 800, 160}
    };
    unsigned cases = 0;
    for (unsigned rotation = 1; rotation <= 3; ++rotation) {
        for (size_t i = 0; i < sizeof(areas) / sizeof(areas[0]); ++i) {
            check(areas[i], rotation, false);
            check(areas[i], rotation, true);
            cases += 2;
        }
    }
    check((display_rect_t){0, 0, 1280, 160}, 3, false);
    printf("CPU RGB565 rotation: %u cases PASS (pixels, byte order, guards)\n", cases + 1);
    return 0;
}
