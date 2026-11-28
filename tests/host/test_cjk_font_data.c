/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "ui/fonts/ttf_data.h"

static unsigned allocations, live, fail_at, checks;
static size_t live_bytes, peak_bytes;
typedef union { max_align_t align; size_t bytes; } allocation_t;

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (++allocations == fail_at) return NULL;
    allocation_t *block = malloc(sizeof(*block) + size);
    if (!block) return NULL;
    block->bytes = size;
    live++;
    live_bytes += size;
    if (live_bytes > peak_bytes) peak_bytes = live_bytes;
    return block + 1;
}
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps)
{
    void *memory = heap_caps_malloc(count * size, caps);
    if (memory) memset(memory, 0, count * size);
    return memory;
}
void heap_caps_free(void *memory)
{
    if (memory) {
        allocation_t *block = (allocation_t *)memory - 1;
        assert(live && live_bytes >= block->bytes);
        live_bytes -= block->bytes;
        live--;
        free(block);
    }
}

/* Exercise the production decoder, including rejection paths before cache publication. */
#include "../../main/ui/font_data.c"
#define CHECK(expr) do { checks++; if (!(expr)) { fprintf(stderr, "Font data line %d: %s\n", __LINE__, #expr); exit(1); } } while (0)

static void reject(const unsigned char *data, size_t size, size_t raw)
{
    CHECK(decode_font(data, size, raw) == NULL);
    CHECK(live == 0);
}

int main(void)
{
    const unsigned char *packed = cjk_font_packed;
    const size_t packed_size = sizeof(cjk_font_packed), raw = CJK_FONT_RAW_SIZE;
    CHECK(raw == ttf_cjk_size);
    unsigned char *decoded = decode_font(packed, packed_size, raw);
    CHECK(decoded && memcmp(decoded, ttf_cjk_data, raw) == 0);
    CHECK(live == 1); /* zlib workspaces have already been freed. */
    unsigned successful_allocations = allocations;
    CHECK(successful_allocations >= 2);
    heap_caps_free(decoded);
    CHECK(live == 0);

    reject(NULL, packed_size, raw); reject(packed, 0, raw);
    reject(packed, FONT_MAX_BYTES + 1, raw);
    reject(packed, packed_size, 0); reject(packed, packed_size, 11);
    reject(packed, packed_size, FONT_MAX_BYTES + 1);
    reject(packed, packed_size - 1, raw);
    reject(packed, packed_size, raw - 1);
    reject(packed, packed_size, raw + 1);
    unsigned char *changed = malloc(packed_size + 1); CHECK(changed != NULL);
    memcpy(changed, packed, packed_size); changed[packed_size] = 0;
    reject(changed, packed_size + 1, raw);
    changed[packed_size - 1] ^= 1;
    reject(changed, packed_size, raw); free(changed);
    const unsigned char invalid_font[12] = {'b', 'a', 'd', '!'};
    unsigned char invalid_archive[100]; uLongf invalid_size = sizeof(invalid_archive);
    CHECK(compress2(invalid_archive, &invalid_size, invalid_font, sizeof(invalid_font), 9) == Z_OK);
    reject(invalid_archive, invalid_size, sizeof(invalid_font));

    const unsigned char *originals[] = {ttf_poppins_data, ttf_poppins_bold_data, ttf_mdi_data, ttf_emoji_data, ttf_cjk_data};
    unsigned int sizes[] = {ttf_poppins_size, ttf_poppins_bold_size, ttf_mdi_size, ttf_emoji_size, ttf_cjk_size};
    unsigned int size = 99;
    CHECK(!ui_font_data(UI_TTF_COUNT, &size) && !size);
    size_t retained = 0;
    for (unsigned id = 0; id < UI_TTF_COUNT; id++) {
        for (unsigned failure = 1; failure <= successful_allocations; failure++) {
            fail_at = failure; allocations = 0;
            CHECK(ui_font_data(id, &size) == NULL);
            CHECK(size == 0 && s_font_data[id] == NULL && live == id);
            CHECK(live_bytes == retained);
        }
        fail_at = 0;
        peak_bytes = live_bytes;
        const unsigned char *font = ui_font_data(id, &size);
        CHECK(font && size == sizes[id] && memcmp(font, originals[id], size) == 0);
        CHECK(live == id + 1);
        retained += size;
        CHECK(live_bytes == retained && peak_bytes > live_bytes);
        printf("Face %u retained %u bytes, temporary %zu bytes\n", id, size, peak_bytes - live_bytes);
    }
    unsigned cached_allocations = allocations;
    fail_at = allocations + 1;
    for (unsigned id = 0; id < UI_TTF_COUNT; id++) {
        for (unsigned i = 0; i < 96; i++) {
            CHECK(ui_font_data(id, &size) == s_font_data[id] && size == sizes[id]);
        }
        CHECK(memcmp(s_font_data[id], originals[id], size) == 0);
    }
    CHECK(allocations == cached_allocations && live == UI_TTF_COUNT);
    /* Only the test teardown frees these. Firmware retains all successful decodes. */
    for (unsigned id = 0; id < UI_TTF_COUNT; id++) heap_caps_free(s_font_data[id]);
    CHECK(live == 0 && live_bytes == 0);
    printf("test_cjk_font_data: %u checks passed\n", checks);
    return 0;
}
