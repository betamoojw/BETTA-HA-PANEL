/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once

/* UI task/display-lock only. The returned bytes stay valid for every font's lifetime. */
#if defined(APP_CJK_COMPRESSED) && APP_CJK_COMPRESSED
const unsigned char *ui_cjk_font_data(unsigned int *size);
#else
#include "ui/fonts/ttf_data.h"
static inline const unsigned char *ui_cjk_font_data(unsigned int *size)
{
    *size = ttf_cjk_size;
    return ttf_cjk_data;
}
#endif
