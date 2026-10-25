/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "reply_font_file.h"
#if LV_TINY_TTF_FILE_SUPPORT
#include "xiaozhi/xiaozhi_font_store.h"
#include <stdio.h>
#include <string.h>

/* Private read-only drive: callers cannot choose another file. LVGL's 4 KiB
 * read cache avoids per-byte LittleFS calls in the TrueType outline decoder. */
static void *open_font(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    if (mode != LV_FS_MODE_RD || strcmp(path, "reply") || !xiaozhi_font_available()) return NULL;
    return fopen(xiaozhi_font_path(), "rb");
}
static lv_fs_res_t close_font(lv_fs_drv_t *drv, void *file)
{ (void)drv; return fclose(file) == 0 ? LV_FS_RES_OK : LV_FS_RES_FS_ERR; }
static lv_fs_res_t read_font(lv_fs_drv_t *drv, void *file, void *buf, uint32_t len, uint32_t *read)
{ (void)drv; *read = fread(buf, 1, len, file); return ferror(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK; }
static lv_fs_res_t seek_font(lv_fs_drv_t *drv, void *file, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    int origin = whence == LV_FS_SEEK_SET ? SEEK_SET : (whence == LV_FS_SEEK_CUR ? SEEK_CUR : SEEK_END);
    return fseek(file, pos, origin) == 0 ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}
static lv_fs_res_t tell_font(lv_fs_drv_t *drv, void *file, uint32_t *pos)
{ (void)drv; long n = ftell(file); if (n < 0) return LV_FS_RES_FS_ERR; *pos = n; return LV_FS_RES_OK; }
bool ui_reply_font_available(void) { return xiaozhi_font_available(); }
lv_font_t *ui_reply_font_create(int px)
{
    static lv_fs_drv_t driver;
    static bool registered;
    if (!xiaozhi_font_available()) return NULL;
    if (!registered) {
        lv_fs_drv_init(&driver); driver.letter = 'X'; driver.cache_size = 4096;
        driver.open_cb = open_font; driver.close_cb = close_font; driver.read_cb = read_font;
        driver.seek_cb = seek_font; driver.tell_cb = tell_font; lv_fs_drv_register(&driver);
        registered = true;
    }
    return lv_tiny_ttf_create_file_ex("X:reply", px, LV_FONT_KERNING_NONE, 64);
}
#else
bool ui_reply_font_available(void) { return false; }
lv_font_t *ui_reply_font_create(int px) { (void)px; return NULL; }
#endif
