/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "net/time_sync.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "util/log_tags.h"

#include "net/timezone_catalog.h"
static char s_timezone[APP_TIME_TZ_MAX_LEN];

const char *time_sync_tz_name(size_t index) { return timezone_catalog_name(index); }
const char *time_sync_tz_rules(const char *name) { return timezone_catalog_rules(name); }

const char *time_sync_get_tz(void)
{
    return s_timezone;
}

static esp_err_t apply_timezone(const char *name, const char *rules)
{
    if (strlen(name) >= sizeof(s_timezone)) return ESP_ERR_INVALID_ARG;
    if (setenv("TZ", rules, 1) != 0) {
        ESP_LOGW(TAG_TIME, "Failed to set timezone '%s'", name);
        return ESP_FAIL;
    }
    tzset();
    memmove(s_timezone, name, strlen(name) + 1);
    ESP_LOGI(TAG_TIME, "Timezone set: %s", s_timezone);
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
    if (esp_sntp_enabled()) {
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
    while (waited < timeout_ms) {
        time_t now = 0;
        struct tm info = {0};
        time(&now);
        localtime_r(&now, &info);
        if (info.tm_year > (2016 - 1900)) {
            ESP_LOGI(TAG_TIME, "Time synchronized");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        waited += step_ms;
    }
    ESP_LOGW(TAG_TIME, "Time synchronization timeout");
    return false;
}
