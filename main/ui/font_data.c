/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/font_data.h"
#include <stdbool.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "zlib.h"
#include "poppins_font_packed.h"
#include "bold_font_packed.h"
#include "mdi_font_packed.h"
#include "emoji_font_packed.h"
#include "cjk_font_packed.h"

#define FONT_MAX_BYTES (512U * 1024U)

typedef struct {
    const unsigned char *packed;
    unsigned int packed_size;
    unsigned int raw_size;
} packed_font_t;

static const packed_font_t s_sources[UI_TTF_COUNT] = {
    [UI_TTF_POPPINS] = {poppins_font_packed, sizeof(poppins_font_packed), POPPINS_FONT_RAW_SIZE},
    [UI_TTF_BOLD] = {bold_font_packed, sizeof(bold_font_packed), BOLD_FONT_RAW_SIZE},
    [UI_TTF_MDI] = {mdi_font_packed, sizeof(mdi_font_packed), MDI_FONT_RAW_SIZE},
    [UI_TTF_EMOJI] = {emoji_font_packed, sizeof(emoji_font_packed), EMOJI_FONT_RAW_SIZE},
    [UI_TTF_CJK] = {cjk_font_packed, sizeof(cjk_font_packed), CJK_FONT_RAW_SIZE},
};

/* One successful decode per face. Drawing, measurement and clock instances
 * borrow the same buffer across all sizes, including after a language change. */
static unsigned char *s_font_data[UI_TTF_COUNT];

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
    if (!data || !size || size > FONT_MAX_BYTES || raw_size < 12 || raw_size > FONT_MAX_BYTES) return NULL;
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
    /* All five committed subsets use the TrueType sfnt signature. */
    if (complete && memcmp(font, "\0\1\0\0", 4) == 0) return font;
    heap_caps_free(font);
    return NULL;
}

const unsigned char *ui_font_data(ui_ttf_id_t id, unsigned int *size)
{
    *size = 0;
    if ((unsigned int)id >= UI_TTF_COUNT) return NULL;
    const packed_font_t *source = &s_sources[id];
    if (!s_font_data[id]) {
        s_font_data[id] = decode_font(source->packed, source->packed_size, source->raw_size);
    }
    if (s_font_data[id]) *size = source->raw_size;
    return s_font_data[id];
}
