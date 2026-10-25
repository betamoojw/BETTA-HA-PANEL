/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "ui/text_font.h"
#include "xiaozhi/xiaozhi_font_store.h"
#include "ui/fonts/cjk_reply_resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Reply font line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2); lv_init(); ui_fonts_set_board(TEXT_BOARD_WIDE10);
    lv_display_t *d = lv_display_create(1280, 800);
    lv_timer_pause(lv_display_get_refr_timer(d));
    const lv_font_t *base = ui_font_role(UI_TEXT_LABEL);
    CHECK(ui_font_reply(base) == base); /* No resource: safe built-in fallback. */
    xiaozhi_font_test_paths(argv[1], CJK_REPLY_BYTES, CJK_REPLY_SHA256);
    xiaozhi_font_store_init(); CHECK(xiaozhi_font_available());
    const lv_font_t *reply = ui_font_reply(base);
    CHECK(reply != base && reply == ui_font_reply(base));
    CHECK(reply->line_height == base->line_height && reply->base_line == base->base_line);
    unsigned checked = 0;
    /* Every unified ideograph in this source, rather than a few observed gaps. */
    for (unsigned cp = 0x4e00; cp <= 0x9fef; cp++) {
        lv_font_glyph_dsc_t glyph = {0};
        CHECK(lv_font_get_glyph_dsc(reply, &glyph, cp, 0));
        CHECK(!glyph.is_placeholder && glyph.box_w > 0); checked++;
    }
    const uint32_t extra[] = {0x3400, 0x4db5, 0xff0c, 0x3002, 0x1f642, 0x41};
    for (unsigned i = 0; i < sizeof(extra)/sizeof(extra[0]); i++) {
        lv_font_glyph_dsc_t glyph = {0}; CHECK(lv_font_get_glyph_dsc(reply, &glyph, extra[i], 0)); CHECK(!glyph.is_placeholder);
    }
    int count = ui_fonts_stats().instances;
    for (int i = 0; i < 100; i++) CHECK(ui_font_reply(base) == reply);
    CHECK(ui_fonts_stats().instances == count && count <= UI_FONT_MAX_INSTANCES);
    lv_obj_t *screen = lv_screen_active(); lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_t *label = lv_label_create(screen); lv_obj_set_pos(label, 30, 80); lv_obj_set_width(label, 1220);
    lv_obj_set_style_text_font(label, reply, 0); lv_obj_set_style_text_color(label, lv_color_hex(0xeeeeee), 0);
    lv_label_set_text(label, "Recognized speech / AI reply\n\n今天是星期天喔，十月二十五号。\n出门记得带雨伞，看起来可能会下点雨。\n\n請攜帶雨傘，臺灣與香港的天氣預報：\n憂鬱、蘋果、鬧鐘、龍、龜、麵包。\n\nClassic Emoji  /  Otto Robot");
    lv_obj_update_layout(screen);
    lv_draw_buf_t *shot = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888); CHECK(shot);
    if (argc > 2) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(shot->data, 1280, 800, 24, shot->header.stride,
            0xff0000, 0x00ff00, 0x0000ff, 0);
        CHECK(surface && SDL_SaveBMP(surface, argv[2]) == 0); SDL_FreeSurface(surface);
    }
    lv_draw_buf_destroy(shot);
    printf("Reply CJK: %u unified ideographs, fallbacks, cache reuse and real glyph rendering passed\n", checked);
    return 0;
}
