/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */

/** @file
 * @brief Bridge to the KNXnet/IP component's shared receive listener.
 */
#pragma once
#include "esp_knx_ip/esp_knx_ip.h"

#ifdef __cplusplus
extern "C"
{
#endif
/** @brief Create a routing handle with an immutable group listener installed before start. */
esp_err_t betta_knx_route_create(const esp_knx_ip_config_t *config, esp_knx_ip_telegram_callback_t listener, esp_knx_ip_handle_t *out);
#ifdef __cplusplus
}
#endif
