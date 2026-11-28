/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
/** @file
 * @brief Start SNTP and apply the panel timezone and regional date format.
 */
#include "net/time_sync.h"

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "util/log_tags.h"

#include "net/timezone_catalog.h"
#include "util/panel_date.h"
static char s_timezone[APP_TIME_TZ_MAX_LEN];
/** @brief Serialize timezone changes with consumers that need a matching name and wall clock. */
static pthread_mutex_t s_timezone_lock = PTHREAD_MUTEX_INITIALIZER;

const char *time_sync_tz_name(size_t index)
{
    return timezone_catalog_name(index);
}

const char *time_sync_tz_rules(const char *name)
{
    return timezone_catalog_rules(name);
}

bool time_sync_local_clock(time_t now, struct tm *local, char *zone, size_t zone_size)
{
    if (!zone || !zone_size || !local)
    {
        return false;
    }
    zone[0] = '\0';
    if (pthread_mutex_lock(&s_timezone_lock) != 0)
    {
        return false;
    }
    strlcpy(zone, s_timezone, zone_size);
    bool ok = localtime_r(&now, local) != NULL;
    pthread_mutex_unlock(&s_timezone_lock);
    return ok;
}

static esp_err_t apply_timezone(const char *name, const char *rules)
{
    if (strlen(name) >= sizeof(s_timezone))
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (pthread_mutex_lock(&s_timezone_lock) != 0)
    {
        return ESP_FAIL;
    }
    if (setenv("TZ", rules, 1) != 0)
    {
        pthread_mutex_unlock(&s_timezone_lock);
        ESP_LOGW(TAG_TIME, "Failed to set timezone '%s'", name);
        return ESP_FAIL;
    }
    tzset();
    panel_date_set_pattern(timezone_catalog_date_pattern(name));
    memmove(s_timezone, name, strlen(name) + 1);
    pthread_mutex_unlock(&s_timezone_lock);
    ESP_LOGI(TAG_TIME, "Timezone set: %s", name);
    return ESP_OK;
}

esp_err_t time_sync_set_tz(const char *name)
{
    const char *rules = time_sync_tz_rules(name);
    return rules != NULL ? apply_timezone(name, rules) : ESP_ERR_INVALID_ARG;
}

esp_err_t time_sync_set_timezone(const char *tz)
{
    const char *value = (tz != NULL && tz[0] != '\0') ? tz : APP_TIME_TZ;
    const char *rules = time_sync_tz_rules(value);
    /* Old POSIX DST rules can also contain '/', so do not classify by slash. */
    return apply_timezone(value, rules != NULL ? rules : value);
}

esp_err_t time_sync_start(const char *ntp_server)
{
    if (esp_sntp_enabled())
    {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ntp_server != NULL ? ntp_server : "pool.ntp.org");
    esp_sntp_init();
    ESP_LOGI(TAG_TIME, "SNTP started");
    return ESP_OK;
}

bool time_sync_wait_for_sync(uint32_t timeout_ms)
{
    const uint32_t step_ms = 500;
    uint32_t waited = 0;
    while (waited < timeout_ms)
    {
        time_t now = 0;
        struct tm info = {0};
        time(&now);
        localtime_r(&now, &info);
        if (info.tm_year > (2016 - 1900))
        {
            ESP_LOGI(TAG_TIME, "Time synchronized");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        waited += step_ms;
    }
    ESP_LOGW(TAG_TIME, "Time synchronization timeout");
    return false;
}
