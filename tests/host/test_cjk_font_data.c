/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "ui/fonts/ttf_data.h"

static unsigned allocations, live, fail_at, checks;
void *heap_caps_malloc(size_t size, uint32_t caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (++allocations == fail_at) return NULL;
    void *memory = malloc(size);
    if (memory) live++;
    return memory;
}
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps)
{
    void *memory = heap_caps_malloc(count * size, caps);
    if (memory) memset(memory, 0, count * size);
    return memory;
}
void heap_caps_free(void *memory)
{
    if (memory) { assert(live); live--; free(memory); }
}

/* Exercise the production decoder, including rejection paths before cache publication. */
#include "../../main/ui/cjk_font_data.c"
#define CHECK(expr) do { checks++; if (!(expr)) { fprintf(stderr, "CJK line %d: %s\n", __LINE__, #expr); exit(1); } } while (0)

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
    reject(packed, CJK_FONT_MAX_BYTES + 1, raw);
    reject(packed, packed_size, 0); reject(packed, packed_size, 11);
    reject(packed, packed_size, CJK_FONT_MAX_BYTES + 1);
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

    for (unsigned failure = 1; failure <= successful_allocations; failure++) {
        fail_at = failure; allocations = 0;
        unsigned int size = 99;
        CHECK(ui_cjk_font_data(&size) == NULL);
        CHECK(size == 0 && s_font_data == NULL && live == 0);
    }
    fail_at = 0; allocations = 0;
    unsigned int size = 0;
    const unsigned char *font = ui_cjk_font_data(&size);
    CHECK(font && size == raw && memcmp(font, ttf_cjk_data, raw) == 0);
    CHECK(live == 1);
    unsigned cached_allocations = allocations;
    fail_at = cached_allocations + 1;
    for (unsigned i = 0; i < 96; i++) {
        size = 0;
        CHECK(ui_cjk_font_data(&size) == font && size == raw);
    }
    CHECK(allocations == cached_allocations && live == 1);
    CHECK(memcmp(font, ttf_cjk_data, raw) == 0);
    /* Only the test process teardown frees it; firmware intentionally retains it. */
    heap_caps_free(s_font_data); s_font_data = NULL;
    CHECK(live == 0);
    printf("test_cjk_font_data: %u checks passed\n", checks);
    return 0;
}
