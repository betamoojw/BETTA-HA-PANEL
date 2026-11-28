/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Operational IPv4 route selection, independent of provisioning Wi-Fi.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"
#include "cJSON.h"

/** @return Borrowed interface with a usable IPv4 address, preferring Ethernet, or NULL. */
esp_netif_t *net_mgr_active_netif(void);

/** @brief Caller-owned snapshot of the selected interface and IPv4 address. */
typedef struct
{
    esp_netif_t *netif;
    uint32_t address;
} net_mgr_snapshot_t;

/** @brief Replace the supplied snapshot with the current route.
 * @param[in,out] previous Zero-initialize before the first call.
 * @return True when a previously selected interface or its address changed, including link loss.
 */
bool net_mgr_connection_changed(net_mgr_snapshot_t *previous);
/** @return True when an upstream interface has a usable IPv4 address, not proof of Internet access. */
bool net_mgr_is_connected(void);
/** @return True when Ethernet is usable and preferred over Wi-Fi. */
bool net_mgr_uses_ethernet(void);
/** @brief Write the selected interface's IPv4 address as dotted-decimal text.
 * @retval ESP_ERR_NOT_FOUND No usable IPv4 address was available.
 * @retval ESP_ERR_INVALID_SIZE The destination was too small.
 */
esp_err_t net_mgr_get_ip(char *out, size_t size);
/** @return True when the active route has a gateway; writes network byte order on success only. */
bool net_mgr_get_gateway(uint32_t *address);
/** @brief Append current route and per-interface diagnostics to caller-owned JSON. */
void net_mgr_add_json(cJSON *root);
