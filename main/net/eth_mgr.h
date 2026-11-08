/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723; adapted from cptkirki/BETTA-HA-PANEL 3dc138a.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"

/* Boot-task initialization; failure leaves Wi-Fi available. */
esp_err_t eth_mgr_init(void);
esp_err_t eth_mgr_wait_for_ip(uint32_t timeout_ms);
bool eth_mgr_is_active(void);
bool eth_mgr_link_up(void);
bool eth_mgr_has_ip(void);
esp_err_t eth_mgr_last_error(void);
esp_netif_t *eth_mgr_netif(void);
