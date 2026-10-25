#pragma once
#define ESP_LOGI(...) ((void)0)
inline const char* esp_err_to_name(int) { return "test transport error"; }
