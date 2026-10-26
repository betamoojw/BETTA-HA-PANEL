#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode;
    uint8_t channel;
    uint8_t bssid[6];
} wifi_mgr_sta_ap_info_t;
bool wifi_mgr_is_connected(void);
bool wifi_mgr_is_setup_ap_active(void);
esp_err_t wifi_mgr_get_sta_ap_info(wifi_mgr_sta_ap_info_t *out);
