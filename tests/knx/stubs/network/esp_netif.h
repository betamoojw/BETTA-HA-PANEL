/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct { int dummy; } esp_netif_t;
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip, netmask, gw; } esp_netif_ip_info_t;
typedef struct { union { esp_ip4_addr_t ip4; } u_addr; uint8_t type; } esp_ip_addr_t;
typedef struct { esp_ip_addr_t ip; } esp_netif_dns_info_t;
typedef enum { ESP_NETIF_DNS_MAIN, ESP_NETIF_DNS_BACKUP } esp_netif_dns_type_t;
#define ESP_IPADDR_TYPE_V4 0
#define ESP_IPADDR_TYPE_V6 6
#define IPSTR "%d.%d.%d.%d"
#define IP2STR(ip) ((const uint8_t *)&(ip)->addr)[0], ((const uint8_t *)&(ip)->addr)[1], \
                  ((const uint8_t *)&(ip)->addr)[2], ((const uint8_t *)&(ip)->addr)[3]
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip);
esp_err_t esp_netif_get_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns);
esp_err_t esp_netif_get_mac(esp_netif_t *netif, uint8_t mac[]);
esp_err_t esp_netif_get_hostname(esp_netif_t *netif, const char **hostname);
