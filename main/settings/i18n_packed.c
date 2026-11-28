/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Expand and parse a bounded built-in translation catalog in PSRAM.
 */
#include "settings/i18n_packed.h"
#include <stdbool.h>
#include <string.h>
#include "app_config.h"
#include "esp_heap_caps.h"
#include "zlib.h"

static voidpf catalog_alloc(voidpf opaque, uInt count, uInt size)
{
    (void) opaque;
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void catalog_free(voidpf opaque, voidpf memory)
{
    (void) opaque;
    heap_caps_free(memory);
}

cJSON *i18n_packed_parse(const unsigned char *data, size_t size, size_t json_size)
{
    if (!data || !size || size > APP_I18N_MAX_JSON_LEN || !json_size || json_size > APP_I18N_MAX_JSON_LEN)
    {
        return NULL;
    }
    char *json = heap_caps_malloc(json_size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!json)
    {
        return NULL;
    }
    z_stream stream = {.zalloc = catalog_alloc, .zfree = catalog_free};
    cJSON *root = NULL;
    if (inflateInit(&stream) == Z_OK)
    {
        stream.next_in = (Bytef *) data;
        stream.avail_in = (uInt) size;
        stream.next_out = (Bytef *) json;
        stream.avail_out = (uInt) json_size;
        int status = inflate(&stream, Z_FINISH);
        bool complete = status == Z_STREAM_END && stream.total_out == json_size && stream.avail_in == 0;
        (void) inflateEnd(&stream);
        if (complete && !memchr(json, '\0', json_size))
        {
            json[json_size] = '\0';
            root = cJSON_ParseWithOpts(json, NULL, true);
            if (!cJSON_IsObject(root))
            {
                cJSON_Delete(root);
                root = NULL;
            }
        }
    }
    heap_caps_free(json);
    return root;
}
