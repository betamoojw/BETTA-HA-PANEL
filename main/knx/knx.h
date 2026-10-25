/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include "cJSON.h"
#include "esp_http_server.h"
#ifdef __cplusplus
extern "C" {
#endif
void knx_init(void);
/* Returned JSON belongs to the caller. No API calls LVGL or manages Wi-Fi. */
cJSON *knx_status(void);
char *knx_configuration(void);
bool knx_configure(const char *json, char *error, size_t error_size);
bool knx_enable(bool enabled, char *error, size_t error_size);
cJSON *knx_objects(int offset, int limit);
cJSON *knx_object(const char *id);
/* Queued, bounded, asynchronous; rechecked against the current permissions. */
bool knx_command(const char *id, const char *address, const char *value,
                 bool write, char *error, size_t error_size);
esp_err_t knx_api_register(httpd_handle_t server);
#ifdef __cplusplus
}
#endif
