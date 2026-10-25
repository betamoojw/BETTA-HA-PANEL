#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct { int unused; } esp_netif_t;
typedef struct { struct { uint32_t addr; } ip; } esp_netif_ip_info_t;
extern bool test_online;
extern uint32_t test_ip;
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) { static esp_netif_t n{}; return &n; }
inline bool esp_netif_is_netif_up(esp_netif_t*) { return test_online; }
inline esp_err_t esp_netif_get_ip_info(esp_netif_t*, esp_netif_ip_info_t* info) { info->ip.addr=test_ip; return ESP_OK; }
