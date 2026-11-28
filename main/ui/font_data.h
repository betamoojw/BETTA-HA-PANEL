/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once

typedef enum {
    UI_TTF_POPPINS, UI_TTF_BOLD, UI_TTF_MDI, UI_TTF_EMOJI, UI_TTF_CJK, UI_TTF_COUNT
} ui_ttf_id_t;

/* UI task/display-lock only. tiny_ttf borrows these immutable bytes until reboot. */
#if defined(APP_TTF_COMPRESSED) && APP_TTF_COMPRESSED
const unsigned char *ui_font_data(ui_ttf_id_t id, unsigned int *size);
#else
#include <stddef.h>
#include "ui/fonts/ttf_data.h"
static inline const unsigned char *ui_font_data(ui_ttf_id_t id, unsigned int *size)
{
    switch (id) {
    case UI_TTF_POPPINS: *size = ttf_poppins_size; return ttf_poppins_data;
    case UI_TTF_BOLD:    *size = ttf_poppins_bold_size; return ttf_poppins_bold_data;
    case UI_TTF_MDI:     *size = ttf_mdi_size; return ttf_mdi_data;
    case UI_TTF_EMOJI:   *size = ttf_emoji_size; return ttf_emoji_data;
    case UI_TTF_CJK:     *size = ttf_cjk_size; return ttf_cjk_data;
    default:             *size = 0; return NULL;
    }
}
#endif
