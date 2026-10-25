#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
extern bool test_storage_failure;
extern uint8_t test_enabled;
inline esp_err_t nvs_open(const char*,int,nvs_handle_t* h) { *h=1; return ESP_OK; }
inline esp_err_t nvs_get_u8(nvs_handle_t,const char*,uint8_t* v) { *v=test_enabled; return ESP_OK; }
inline esp_err_t nvs_set_u8(nvs_handle_t,const char*,uint8_t v) { if(test_storage_failure) return ESP_FAIL; test_enabled=v; return ESP_OK; }
inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
inline void nvs_close(nvs_handle_t) {}
