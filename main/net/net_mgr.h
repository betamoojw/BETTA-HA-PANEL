/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"
#include "cJSON.h"

/* Operational connectivity, independent of the radio used for provisioning. */
esp_netif_t *net_mgr_active_netif(void);
typedef struct { esp_netif_t *netif; uint32_t address; } net_mgr_snapshot_t;
/* Records the current route; true when an existing connection must be reopened. */
bool net_mgr_connection_changed(net_mgr_snapshot_t *previous);
bool net_mgr_is_connected(void);
bool net_mgr_uses_ethernet(void);
esp_err_t net_mgr_get_ip(char *out, size_t size);
bool net_mgr_get_gateway(uint32_t *address);
void net_mgr_add_json(cJSON *root);
