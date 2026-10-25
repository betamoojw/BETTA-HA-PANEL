#pragma once
#include <stddef.h>
#include <stdint.h>
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(n) (n)
#define CONFIG_ESP_KNX_IP_MAX_GROUP_ADDRESSES 128
#define CONFIG_LWIP_MAX_SOCKETS 32
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
