/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723; adapted from cptkirki/BETTA-HA-PANEL 3dc138a.
 */
/** @file
 * @brief Optional Ethernet startup, readiness and interface queries.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"

/** @brief Start the optional Ethernet driver from the boot task; failure leaves Wi-Fi available.
 * @retval ESP_OK The driver is running; DHCP may still be pending.
 * @retval ESP_ERR_NOT_SUPPORTED Ethernet is not enabled for this build.
 */
esp_err_t eth_mgr_init(void);
/** @brief Wait up to timeout_ms for link and a usable IPv4 address; does not stop DHCP on timeout. */
esp_err_t eth_mgr_wait_for_ip(uint32_t timeout_ms);
/** @return True when the Ethernet driver has started, even if the cable is disconnected. */
bool eth_mgr_is_active(void);
/** @return True when the physical link is up, independently of DHCP. */
bool eth_mgr_link_up(void);
/** @return True only when the link and interface are up and the lease is usable. */
bool eth_mgr_has_ip(void);
/** @return Last initialization result, not a live link status. */
esp_err_t eth_mgr_last_error(void);
/** @return Borrowed Ethernet interface only while it has a usable IPv4 address. */
esp_netif_t *eth_mgr_netif(void);
