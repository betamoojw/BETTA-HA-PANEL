/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "net/c6_image.h"
#include <stdbool.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "util/sha256_soft.h"
#include "zlib.h"

static voidpf image_alloc(voidpf opaque, uInt count, uInt size)
{
    (void)opaque;
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void image_free(voidpf opaque, voidpf memory)
{
    (void)opaque;
    heap_caps_free(memory);
}

esp_err_t c6_image_unpack(const uint8_t *archive, size_t archive_size, uint8_t **data, size_t *size)
{
    if (data == NULL || size == NULL) return ESP_ERR_INVALID_ARG;
    *data = NULL;
    *size = 0;
    if (archive == NULL || archive_size <= 40 || archive_size > 4 * 1024 * 1024 ||
        memcmp(archive, "BC6Z", 4) != 0) return ESP_ERR_INVALID_SIZE;
    const uint32_t raw_size = (uint32_t)archive[4] | ((uint32_t)archive[5] << 8) |
                              ((uint32_t)archive[6] << 16) | ((uint32_t)archive[7] << 24);
    if (raw_size < 288 || raw_size > 4 * 1024 * 1024) return ESP_ERR_INVALID_SIZE;
    uint8_t *raw = heap_caps_malloc(raw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (raw == NULL) return ESP_ERR_NO_MEM;
    z_stream stream = {.zalloc = image_alloc, .zfree = image_free};
    int status = inflateInit(&stream);
    if (status == Z_OK) {
        stream.next_in = (Bytef *)(archive + 40);
        stream.avail_in = (uInt)(archive_size - 40);
        stream.next_out = raw;
        stream.avail_out = raw_size;
        status = inflate(&stream, Z_FINISH);
        const bool complete = status == Z_STREAM_END && stream.total_out == raw_size && stream.avail_in == 0;
        (void)inflateEnd(&stream);
        if (complete) {
            sha256_soft_t hash;
            uint8_t digest[32];
            sha256_soft_init(&hash);
            sha256_soft_update(&hash, raw, raw_size);
            sha256_soft_final(&hash, digest);
            if (memcmp(digest, archive + 8, sizeof(digest)) == 0) {
                *data = raw;
                *size = raw_size;
                return ESP_OK;
            }
        }
    }
    heap_caps_free(raw);
    return status == Z_MEM_ERROR ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_RESPONSE;
}
