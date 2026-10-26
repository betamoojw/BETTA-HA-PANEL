/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 *
 * Tool names and audience semantics follow betamoojw/xiaozhi-esp32
 * c435f99a88066029ec2dbd2d758c8cc9d051565b. This is an ESP-IDF C
 * implementation integrated with BETTA's storage, UI event and OTA paths.
 */
#include "xiaozhi_mcp_tools.h"
#include "xiaozhi_mcp_ha.h"

#include "sdkconfig.h"

#if CONFIG_APP_PANEL_VARIANT_10INCH_1280

#include <stdio.h>
#include <string.h>

#include "api/api_ota.h"
#include "app_config.h"
#include "app_events.h"
#include "component_versions.h"
#include "drivers/display_init.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/ha_client.h"
#include "net/wifi_mgr.h"
#include "settings/runtime_settings.h"
#include "ui/display_geometry.h"
#include "ui/theme/theme_palette.h"
#include "ui/theme/theme_store.h"
#include "util/reboot_log_esp.h"
#include "xiaozhi.h"

static const char *string_argument(const cJSON *arguments, const char *name)
{
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(arguments, name));
}

static int integer_argument(const cJSON *arguments, const char *name)
{
    return cJSON_GetObjectItemCaseSensitive(arguments, name)->valueint;
}

static cJSON *error_result(char *error, size_t error_size, const char *message)
{
    snprintf(error, error_size, "%s", message);
    return NULL;
}

static void set_active_brightness(runtime_settings_t *settings, void *context)
{
    settings->hardware_active_brightness_percent = *(const int *)context;
}

static cJSON *set_brightness(const cJSON *arguments, char *error, size_t error_size)
{
    int brightness = integer_argument(arguments, "brightness");
    runtime_settings_t settings;
    esp_err_t err = runtime_settings_update(set_active_brightness, &brightness, &settings);
    if (err == ESP_OK) {
        display_configure_brightness(settings.hardware_active_brightness_percent,
            settings.hardware_dim_brightness_percent, settings.hardware_dim_timeout_seconds);
        err = display_set_brightness_percent(brightness);
    }
    if (err != ESP_OK) return error_result(error, error_size, esp_err_to_name(err));
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "brightness", brightness);
    cJSON_AddBoolToObject(result, "saved", true);
    return result;
}

static cJSON *set_theme(const cJSON *arguments, char *error, size_t error_size)
{
    const char *id = string_argument(arguments, "theme");
    theme_entry_t selected;
    esp_err_t err = theme_palette_resolve_by_id(id, &selected);
    if (err == ESP_OK) err = theme_store_set_active_id(id);
    if (err == ESP_OK) {
        app_event_t event = {.type = EV_LAYOUT_UPDATED};
        if (!app_events_publish(&event, pdMS_TO_TICKS(20))) err = ESP_ERR_TIMEOUT;
    }
    if (err != ESP_OK) return error_result(error, error_size,
        err == ESP_ERR_NOT_FOUND ? "Unknown theme" : esp_err_to_name(err));
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "theme", id);
    cJSON_AddBoolToObject(result, "saved", true);
    return result;
}

static cJSON *device_status(void)
{
    xiaozhi_status_t assistant;
    xiaozhi_status(&assistant);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "board", APP_BOARD_DESCRIPTION);
    cJSON_AddBoolToObject(result, "wifi_connected", wifi_mgr_is_connected());
    cJSON_AddBoolToObject(result, "home_assistant_connected", ha_client_is_connected());
    cJSON_AddNumberToObject(result, "brightness", display_get_brightness_percent());
    cJSON_AddStringToObject(result, "theme", theme_palette_active_id());
    cJSON_AddNumberToObject(result, "speaker_volume", assistant.volume);
    cJSON_AddStringToObject(result, "assistant_state", assistant.state);
    return result;
}

static cJSON *system_info(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "name", APP_NAME);
    cJSON_AddStringToObject(result, "board", APP_BOARD_DESCRIPTION);
    cJSON_AddStringToObject(result, "version", app && app->version[0] ? app->version : "unknown");
    cJSON_AddStringToObject(result, "git_commit", APP_GIT_COMMIT_SHA);
    cJSON_AddStringToObject(result, "idf_version", app && app->idf_ver[0] ? app->idf_ver : "");
    cJSON_AddNumberToObject(result, "chip_revision", chip.revision);
    cJSON_AddNumberToObject(result, "cpu_cores", chip.cores);
    cJSON_AddNumberToObject(result, "uptime_s", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(result, "free_heap_internal",
        (double)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(result, "free_psram",
        (double)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return result;
}

static void reboot_task(void *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(1000));
    reboot_log_mark(REBOOT_CAUSE_MANUAL);
    esp_restart();
}

bool xiaozhi_mcp_device_tool_supported(const char *name)
{
    static const char *const supported[] = {
        "self.get_device_status",
        "self.audio_speaker.set_volume",
        "self.screen.set_brightness",
        "self.screen.set_theme",
        "self.get_network_info",
        "self.ha.list_entities",
        "self.ha.get_entity",
        "self.ha.control_entity",
        "self.get_system_info",
        "self.reboot",
        "self.upgrade_firmware",
        "self.screen.get_info",
    };
    for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); i++) {
        if (strcmp(name, supported[i]) == 0) return true;
    }
    return false;
}

cJSON *xiaozhi_mcp_device_tool_call(const char *name, const cJSON *arguments,
                                    char *error, size_t error_size)
{
    if (strcmp(name, "self.get_device_status") == 0) return device_status();
    if (!strcmp(name, "self.ha.list_entities") || !strcmp(name, "self.ha.get_entity") ||
        !strcmp(name, "self.ha.control_entity"))
        return xiaozhi_mcp_ha_call(name, arguments, error, error_size);
    if (strcmp(name, "self.get_network_info") == 0) {
        cJSON *result = xiaozhi_mcp_network_info();
        return result ? result : error_result(error, error_size, "Network snapshot allocation failed");
    }
    if (strcmp(name, "self.audio_speaker.set_volume") == 0) {
        int volume = integer_argument(arguments, "volume");
        esp_err_t err = xiaozhi_command("volume", volume);
        if (err != ESP_OK) return error_result(error, error_size, esp_err_to_name(err));
        cJSON *result = cJSON_CreateObject();
        cJSON_AddNumberToObject(result, "volume", volume);
        cJSON_AddBoolToObject(result, "queued", true);
        return result;
    }
    if (strcmp(name, "self.screen.set_brightness") == 0)
        return set_brightness(arguments, error, error_size);
    if (strcmp(name, "self.screen.set_theme") == 0)
        return set_theme(arguments, error, error_size);
    if (strcmp(name, "self.get_system_info") == 0) return system_info();
    if (strcmp(name, "self.reboot") == 0) {
        if (xTaskCreate(reboot_task, "mcp_reboot", 3072, NULL, 5, NULL) != pdPASS)
            return error_result(error, error_size, "Could not start restart task");
        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "restarting", true);
        return result;
    }
    if (strcmp(name, "self.upgrade_firmware") == 0) {
        esp_err_t err = api_ota_start_url(string_argument(arguments, "url"));
        if (err != ESP_OK) return error_result(error, error_size, esp_err_to_name(err));
        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "started", true);
        return result;
    }
    if (strcmp(name, "self.screen.get_info") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddNumberToObject(result, "width", display_geometry_width());
        cJSON_AddNumberToObject(result, "height", display_geometry_height());
        cJSON_AddNumberToObject(result, "rotation", display_geometry_rotation());
        cJSON_AddBoolToObject(result, "monochrome", false);
        return result;
    }
    return error_result(error, error_size, "Unsupported device tool");
}

#else

bool xiaozhi_mcp_device_tool_supported(const char *name)
{
    (void)name;
    return false;
}

cJSON *xiaozhi_mcp_device_tool_call(const char *name, const cJSON *arguments,
                                    char *error, size_t error_size)
{
    (void)name;
    (void)arguments;
    if (error_size > 0) error[0] = '\0';
    return NULL;
}

#endif
