/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "camera/camera_store.h"

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "util/atomic_file.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMERA_PATH "/littlefs/cameras.json"
static camera_settings_t s_settings;
static SemaphoreHandle_t s_mutex;
static _Atomic uint32_t s_generation;

esp_err_t camera_store_init(void)
{
    if (s_mutex)
        return ESP_OK;
    camera_config_defaults(&s_settings);
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
        return ESP_ERR_NO_MEM;
    FILE *f = fopen(CAMERA_PATH, "rb");
    if (!f)
        return errno == ENOENT ? ESP_OK : ESP_FAIL;
    char *buf = heap_caps_malloc(CAMERA_CONFIG_MAX_JSON + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf)
    {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t n = fread(buf, 1, CAMERA_CONFIG_MAX_JSON + 1, f);
    bool valid = !ferror(f) && n <= CAMERA_CONFIG_MAX_JSON;
    fclose(f);
    cJSON *root = NULL;
    if (valid)
    {
        buf[n] = 0;
        root = cJSON_ParseWithLength(buf, n + 1);
    }
    camera_settings_t *candidate = heap_caps_malloc(sizeof(*candidate), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (candidate)
        *candidate = s_settings;
    valid = root && candidate && camera_config_parse(root, candidate);
    if (valid)
        s_settings = *candidate;
    free(candidate);
    cJSON_Delete(root);
    free(buf);
    /* Invalid/read-failed files are retained; defaults never overwrite them. */
    return valid ? ESP_OK : ESP_FAIL;
}
bool camera_store_get(camera_settings_t *out)
{
    if (!out || !s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return false;
    *out = s_settings;
    xSemaphoreGive(s_mutex);
    return true;
}
bool camera_store_local(camera_config_t *out)
{
    if (!out || !s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return false;
    *out = s_settings.local;
    xSemaphoreGive(s_mutex);
    return true;
}
bool camera_store_source(const char *id, camera_source_t *out)
{
    if (!id || !out || !s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return false;
    bool found = false;
    for (int i = 0; i < s_settings.source_count; i++)
        if (!strcmp(id, s_settings.sources[i].id))
        {
            *out = s_settings.sources[i];
            found = true;
            break;
        }
    xSemaphoreGive(s_mutex);
    return found;
}
esp_err_t camera_store_update(const cJSON *root)
{
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(300)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    camera_settings_t *candidate = heap_caps_malloc(sizeof(*candidate), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    esp_err_t err = ESP_ERR_NO_MEM;
    if (!candidate)
        goto done;
    *candidate = s_settings;
    if (!camera_config_parse(root, candidate))
    {
        err = ESP_ERR_INVALID_ARG;
        goto done;
    }
    cJSON *json = camera_config_json(candidate, true);
    char *text = json ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (!text)
        goto done;
    size_t n = strlen(text);
    err = n > CAMERA_CONFIG_MAX_JSON ? ESP_ERR_INVALID_SIZE : atomic_file_write(CAMERA_PATH, text, n);
    memset(text, 0, n);
    free(text);
    if (err == ESP_OK)
    {
        s_settings = *candidate;
        atomic_fetch_add(&s_generation, 1);
    }
done:
    if (candidate)
    {
        memset(candidate, 0, sizeof(*candidate));
        free(candidate);
    }
    xSemaphoreGive(s_mutex);
    return err;
}
uint32_t camera_store_generation(void)
{
    return atomic_load(&s_generation);
}
bool camera_store_output_allowed(const char *id)
{
    if (!id || !s_mutex || xSemaphoreTake(s_mutex, 0) != pdTRUE)
        return false;
    bool enabled = false;
    if (!strcmp(id, "local"))
        enabled = s_settings.local.enabled;
    else
        for (int i = 0; i < s_settings.source_count; i++)
            if (!strcmp(id, s_settings.sources[i].id))
                enabled = s_settings.sources[i].enabled;
    xSemaphoreGive(s_mutex);
    return enabled;
}
