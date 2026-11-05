/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "lvgl.h"
#include "src/image/lv_image_decoder_private.h"
#include "ui/notification_emoji_data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

LV_IMAGE_DECLARE(Temp); LV_IMAGE_DECLARE(RGB); LV_IMAGE_DECLARE(visiblecolor);
LV_IMAGE_DECLARE(raw_Temp); LV_IMAGE_DECLARE(raw_RGB); LV_IMAGE_DECLARE(raw_visiblecolor);
extern const notification_emoji_entry_t raw_k_notification_emoji[];
extern const unsigned raw_k_notification_emoji_count;
static unsigned checked;
static uint32_t render_hash = 2166136261U;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{ (void)area; (void)pixels; lv_display_flush_ready(display); }

static void check_image(lv_obj_t *card, const lv_image_dsc_t *raw, const lv_image_dsc_t *packed)
{
    assert(raw->header.w == packed->header.w && raw->header.h == packed->header.h);
    assert(raw->header.cf == packed->header.cf);
    assert(packed->header.flags & LV_IMAGE_FLAGS_COMPRESSED);
    lv_image_decoder_dsc_t decoder;
    lv_image_decoder_args_t args = {.no_cache = true};
    assert(lv_image_decoder_open(&decoder, packed, &args) == LV_RESULT_OK);
    assert(decoder.decoded && decoder.decoded->data_size >= raw->data_size);
    assert(decoder.decoded->header.stride == raw->header.w * 2);
    assert(memcmp(decoder.decoded->data, raw->data, raw->data_size) == 0);
    lv_image_decoder_close(&decoder);

    lv_obj_t *image = lv_image_create(card);
    lv_obj_set_pos(image, 4, 4);
    for (unsigned light = 0; light < 2; ++light) {
        lv_obj_set_style_bg_color(card, light ? lv_color_white() : lv_color_black(), 0);
        lv_image_set_src(image, raw);
        lv_obj_update_layout(card);
        lv_draw_buf_t *before = lv_snapshot_take(card, LV_COLOR_FORMAT_ARGB8888);
        assert(before);
        lv_image_set_src(image, packed);
        lv_obj_update_layout(card);
        lv_draw_buf_t *after = lv_snapshot_take(card, LV_COLOR_FORMAT_ARGB8888);
        assert(after && before->header.w == after->header.w && before->header.h == after->header.h);
        for (unsigned row = 0; row < before->header.h; ++row) {
            const uint8_t *a = before->data + row * before->header.stride;
            const uint8_t *b = after->data + row * after->header.stride;
            assert(memcmp(a, b, before->header.w * 4) == 0);
            for (unsigned col = 0; col < before->header.w * 4; ++col)
                render_hash = (render_hash ^ a[col]) * 16777619U;
        }
        lv_draw_buf_destroy(before); lv_draw_buf_destroy(after);
    }
    lv_obj_delete(image);
    ++checked;
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(640, 160);
    static uint16_t buffer[640 * 8];
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_obj_t *card = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 608, 108);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(card, false);
    check_image(card, &raw_Temp, &Temp);
    check_image(card, &raw_RGB, &RGB);
    check_image(card, &raw_visiblecolor, &visiblecolor);
    assert(raw_k_notification_emoji_count == k_notification_emoji_count);
    for (unsigned i = 0; i < k_notification_emoji_count; ++i) {
        assert(raw_k_notification_emoji[i].codepoint == k_notification_emoji[i].codepoint);
        for (unsigned size = 0; size < NOTIFICATION_EMOJI_SIZES; ++size)
            check_image(card, &raw_k_notification_emoji[i].image[size], &k_notification_emoji[i].image[size]);
    }
    assert(checked == 151);
    lv_obj_delete(card); lv_display_delete(display); lv_deinit();
    printf("151 images: native LVGL decode bytes and 302 light/dark renders identical (FNV32 %08x)\n", (unsigned)render_hash);
    return 0;
}
