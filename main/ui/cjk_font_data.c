/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/cjk_font_data.h"
#include <stdbool.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "zlib.h"
#include "cjk_font_packed.h"

#define CJK_FONT_MAX_BYTES (512U * 1024U)

/* tiny_ttf borrows the data. One immutable buffer is retained until reboot,
 * shared by all sizes; it must never be freed while any font references it. */
static unsigned char *s_font_data;

static voidpf font_alloc(voidpf opaque, uInt count, uInt size)
{
    (void)opaque;
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void font_free(voidpf opaque, voidpf memory)
{
    (void)opaque;
    heap_caps_free(memory);
}

static unsigned char *decode_font(const unsigned char *data, size_t size, size_t raw_size)
{
    if (!data || !size || size > CJK_FONT_MAX_BYTES || raw_size < 12 || raw_size > CJK_FONT_MAX_BYTES) return NULL;
    unsigned char *font = heap_caps_malloc(raw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!font) return NULL;
    z_stream stream = {.zalloc = font_alloc, .zfree = font_free};
    bool complete = false;
    if (inflateInit(&stream) == Z_OK) {
        stream.next_in = (Bytef *)data;
        stream.avail_in = (uInt)size;
        stream.next_out = font;
        stream.avail_out = (uInt)raw_size;
        int status = inflate(&stream, Z_FINISH);
        complete = status == Z_STREAM_END && stream.total_out == raw_size && stream.avail_in == 0;
        (void)inflateEnd(&stream);
    }
    /* The committed UI subset is a TrueType font, not CFF or a font collection. */
    if (complete && memcmp(font, "\0\1\0\0", 4) == 0) return font;
    heap_caps_free(font);
    return NULL;
}

const unsigned char *ui_cjk_font_data(unsigned int *size)
{
    *size = 0;
    if (!s_font_data) {
        s_font_data = decode_font(cjk_font_packed, sizeof(cjk_font_packed), CJK_FONT_RAW_SIZE);
    }
    if (s_font_data) *size = CJK_FONT_RAW_SIZE;
    return s_font_data;
}
