/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Persist and report the device license without gating panel functionality.
 */
#include "settings/license_manager.h"
#include "settings/license_crypto.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs.h"
#include "sdkconfig.h"
#include <stdatomic.h>
#include <string.h>

#ifndef CONFIG_APP_LICENSE_PUBLIC_KEY_HEX
#  define CONFIG_APP_LICENSE_PUBLIC_KEY_HEX ""
#endif
static char s_device_id[65];
static atomic_int s_key_state = LICENSE_ERROR;

/** @return Remaining informational trial seconds from boot, independent of import or removal. */
static uint32_t remaining(void)
{
    int64_t elapsed = esp_timer_get_time() / 1000000;
    if (elapsed < 0 || elapsed >= LICENSE_TRIAL_SECONDS)
    {
        return 0;
    }
    return LICENSE_TRIAL_SECONDS - (uint32_t) elapsed;
}

void license_manager_init(void)
{
    uint8_t mac[6];
    atomic_store(&s_key_state, LICENSE_ERROR);
    s_device_id[0] = 0;
    if (esp_efuse_mac_get_default(mac) != ESP_OK || !license_device_id(mac, s_device_id))
    {
        return;
    }
    if (!license_public_key_valid(CONFIG_APP_LICENSE_PUBLIC_KEY_HEX))
    {
        if (!CONFIG_APP_LICENSE_PUBLIC_KEY_HEX[0])
        {
            atomic_store(&s_key_state, LICENSE_UNCONFIGURED);
        }
        return;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("betta_license", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        atomic_store(&s_key_state, LICENSE_MISSING);
        return;
    }
    if (err != ESP_OK)
    {
        return;
    }
    char key[LICENSE_FILE_LENGTH];
    size_t length = sizeof(key);
    err = nvs_get_blob(handle, "device_key", key, &length);
    nvs_close(handle);
    license_key_state_t state = LICENSE_ERROR;
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        state = LICENSE_MISSING;
    }
    else if (err == ESP_ERR_NVS_INVALID_LENGTH)
    {
        state = LICENSE_INVALID;
    }
    else if (err == ESP_OK)
    {
        int verified = license_key_verify(CONFIG_APP_LICENSE_PUBLIC_KEY_HEX, s_device_id, key, length);
        state = verified > 0 ? LICENSE_VALID : verified == 0 ? LICENSE_INVALID : LICENSE_ERROR;
    }
    atomic_store(&s_key_state, state);
}

void license_manager_status(license_status_t *out)
{
    memset(out, 0, sizeof(*out));
    memcpy(out->device_id, s_device_id, sizeof(s_device_id));
    out->key_state = atomic_load(&s_key_state);
    out->trial_seconds = out->key_state == LICENSE_VALID ? 0 : remaining();
}

esp_err_t license_manager_import(const void *key, size_t length)
{
    if (!s_device_id[0] || !license_public_key_valid(CONFIG_APP_LICENSE_PUBLIC_KEY_HEX))
    {
        return ESP_ERR_INVALID_STATE;
    }
    int verified = license_key_verify(CONFIG_APP_LICENSE_PUBLIC_KEY_HEX, s_device_id, key, length);
    if (verified <= 0)
    {
        return verified == 0 ? ESP_ERR_INVALID_ARG : ESP_FAIL;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("betta_license", NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_blob(handle, "device_key", key, LICENSE_FILE_LENGTH);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    /* A failed write/commit has uncertain durability; never report activation. */
    atomic_store(&s_key_state, err == ESP_OK ? LICENSE_VALID : LICENSE_ERROR);
    return err;
}

esp_err_t license_manager_remove(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("betta_license", NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_erase_key(handle, "device_key");
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        err = ESP_OK;
    }
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    atomic_store(&s_key_state,
                 err == ESP_OK ? (license_public_key_valid(CONFIG_APP_LICENSE_PUBLIC_KEY_HEX) ? LICENSE_MISSING
                                  : CONFIG_APP_LICENSE_PUBLIC_KEY_HEX[0]                      ? LICENSE_ERROR
                                                                                              : LICENSE_UNCONFIGURED)
                               : LICENSE_ERROR);
    return err;
}
