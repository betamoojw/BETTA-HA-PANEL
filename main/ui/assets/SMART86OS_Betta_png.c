/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * The boot and OTA screens share this descriptor. The embedded PNG and the
 * RGB565 fallback contain the same pixels from docs/img/brand/BETTA-OS.jpg.
 * main/CMakeLists.txt selects one representation. See main/ui/assets/README.md.
 */
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

/* Set by main/CMakeLists.txt from the size of the PNG file. */
#ifndef SMART86OS_BETTA_PNG_SIZE
#error "SMART86OS_BETTA_PNG_SIZE must be defined by the build"
#endif

extern const uint8_t _binary_SMART86OS_Betta_png_start[] asm("_binary_SMART86OS_Betta_png_start");

const lv_image_dsc_t SMART86OS_Betta = {
    .header.cf = LV_COLOR_FORMAT_RAW,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w = 370,
    .header.h = 340,
    .data_size = SMART86OS_BETTA_PNG_SIZE,
    .data = _binary_SMART86OS_Betta_png_start,
};
