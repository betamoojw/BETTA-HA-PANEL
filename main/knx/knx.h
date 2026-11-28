/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */

/** @file
 * @brief Shared KNX service API for commissioning and the assistant.
 */
#pragma once
#include "cJSON.h"
#include "esp_http_server.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif
/** @brief Load saved configuration and start the worker without initializing Wi-Fi or LVGL. */
void knx_init(void);

/** @return Status snapshot owned by the caller; release it with cJSON_Delete(). */
cJSON *knx_status(void);

/** @return Copy of the current configuration, or NULL on allocation failure. Release it with free(). */
char *knx_configuration(void);

/**
 * @brief Validate and persist a complete registry before publishing it to the worker.
 * @param json NUL-terminated registry JSON.
 * @param[out] error Optional diagnostic buffer used on failure.
 * @param error_size Capacity of the diagnostic buffer.
 * @return True if the replacement was saved and published.
 */
bool knx_configure(const char *json, char *error, size_t error_size);

/** @brief Persist the routing enable preference. On failure, write to the optional error buffer. */
bool knx_enable(bool enabled, char *error, size_t error_size);

/**
 * @brief Read a page of objects in configuration order.
 * @param offset Starting object index, clamped to the registry bounds.
 * @param limit Page size, clamped to 1 through 20.
 * @return Page with continuation and routing metadata, or NULL on allocation failure.
 *         Release a returned page with cJSON_Delete().
 */
cJSON *knx_objects(int offset, int limit);

/** @return Snapshot for an exact object ID, or NULL if unknown or allocation fails. Release it with cJSON_Delete(). */
cJSON *knx_object(const char *id);

/**
 * @brief Queue a read or write for an object. The worker rechecks permissions before sending.
 * @param id Exact object ID, or NULL to select by address.
 * @param address Group address used only when id is NULL; it must select one permitted object.
 * @param value DPT-formatted text for a write; ignored for a read.
 * @param write True for a write, false for a read.
 * @param[out] error Optional diagnostic buffer used on rejection.
 * @param error_size Capacity of the diagnostic buffer.
 * @return True if queued, not if confirmed by the bus.
 */
bool knx_command(const char *id, const char *address, const char *value, bool write, char *error, size_t error_size);

/** @return ESP_OK if all guarded commissioning routes were registered, or the first registration error. */
esp_err_t knx_api_register(httpd_handle_t server);
#ifdef __cplusplus
}
#endif
