/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "net/c6_image.h"
#include "util/sha256_soft.h"
#include "zlib.h"

static unsigned checks, allocations, live, fail_at;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "C6 archive line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
void *heap_caps_malloc(size_t size, uint32_t caps)
{
    CHECK(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
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
    if (memory) { CHECK(live > 0); live--; free(memory); }
}
static void reject(const uint8_t *archive, size_t length)
{
    uint8_t *raw = (uint8_t *)1;
    size_t size = 123;
    CHECK(c6_image_unpack(archive, length, &raw, &size) != ESP_OK);
    CHECK(raw == NULL && size == 0 && live == 0);
}
static uint8_t *read_file(const char *name, size_t *size)
{
    FILE *file = fopen(name, "rb"); CHECK(file != NULL);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file); CHECK(length > 0);
    rewind(file);
    uint8_t *data = malloc((size_t)length); CHECK(data != NULL);
    CHECK(fread(data, 1, (size_t)length, file) == (size_t)length);
    CHECK(fclose(file) == 0); *size = (size_t)length;
    return data;
}
int main(int argc, char **argv)
{
    uint8_t original[4096];
    for (size_t i = 0; i < sizeof(original); i++) original[i] = (uint8_t)(i * 73U);
    uint8_t archive[8192] = "BC6Z";
    archive[4] = 0; archive[5] = 16;
    sha256_soft_t hash;
    sha256_soft_init(&hash); sha256_soft_update(&hash, original, sizeof(original));
    sha256_soft_final(&hash, archive + 8);
    uLongf compressed = sizeof(archive) - 41;
    CHECK(compress2(archive + 40, &compressed, original, sizeof(original), 9) == Z_OK);
    const size_t length = compressed + 40;
    uint8_t *raw = NULL; size_t size = 0;
    CHECK(c6_image_unpack(archive, length, &raw, &size) == ESP_OK);
    CHECK(size == sizeof(original) && memcmp(raw, original, size) == 0);
    heap_caps_free(raw); CHECK(live == 0);
    reject(NULL, 0); reject(archive, 40); reject(archive, length - 1); reject(archive, length + 1);
    const size_t mutations[] = {0, 7, 8, 40, length - 1};
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); i++) {
        archive[mutations[i]] ^= 0xFF; reject(archive, length); archive[mutations[i]] ^= 0xFF;
    }
    archive[5] = 0; reject(archive, length); archive[5] = 8; reject(archive, length); archive[5] = 16;
    for (fail_at = 1; fail_at <= 2; fail_at++) {
        allocations = 0;
        CHECK(c6_image_unpack(archive, length, &raw, &size) == ESP_ERR_NO_MEM);
        CHECK(raw == NULL && size == 0 && live == 0);
    }
    fail_at = 0;
    if (argc == 3) {
        size_t packed_size, expected_size;
        uint8_t *packed = read_file(argv[1], &packed_size), *expected = read_file(argv[2], &expected_size);
        CHECK(c6_image_unpack(packed, packed_size, &raw, &size) == ESP_OK);
        CHECK(size == expected_size && memcmp(raw, expected, size) == 0);
        heap_caps_free(raw); free(packed); free(expected);
        CHECK(live == 0);
    }
    printf("C6 archive: %u checks passed (round trip, corruption, bounds, allocation failure and cleanup)\n", checks);
    return 0;
}
