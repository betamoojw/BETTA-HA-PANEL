/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "net/net_mgr.h"
#include "net/eth_mgr.h"
#include "net/wifi_mgr.h"
#include "sdkconfig.h"

esp_netif_t *net_mgr_active_netif(void)
{
    esp_netif_t *eth = eth_mgr_netif();
    if (eth) return eth;
    esp_netif_t *wifi = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t info;
    return wifi_mgr_is_connected() && wifi && esp_netif_is_netif_up(wifi) &&
        esp_netif_get_ip_info(wifi, &info) == ESP_OK && info.ip.addr ? wifi : NULL;
}
bool net_mgr_is_connected(void) { return net_mgr_active_netif() != NULL; }
bool net_mgr_connection_changed(net_mgr_snapshot_t *previous)
{
    esp_netif_t *active = net_mgr_active_netif();
    esp_netif_ip_info_t info = {0};
    if (active) (void)esp_netif_get_ip_info(active, &info);
    bool changed = previous->netif && (previous->netif != active || previous->address != info.ip.addr);
    *previous = (net_mgr_snapshot_t){active, info.ip.addr};
    return changed;
}
bool net_mgr_uses_ethernet(void) { return eth_mgr_has_ip(); }
esp_err_t net_mgr_get_ip(char *out, size_t size)
{
    if (!out || !size) return ESP_ERR_INVALID_ARG;
    out[0] = '\0';
    esp_netif_t *netif = net_mgr_active_netif();
    esp_netif_ip_info_t info;
    if (!netif || esp_netif_get_ip_info(netif, &info) != ESP_OK || !info.ip.addr) return ESP_ERR_NOT_FOUND;
    return esp_ip4addr_ntoa(&info.ip, out, size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}
bool net_mgr_get_gateway(uint32_t *address)
{
    esp_netif_t *netif = net_mgr_active_netif();
    esp_netif_ip_info_t info;
    if (!address || !netif || esp_netif_get_ip_info(netif, &info) != ESP_OK || !info.gw.addr) return false;
    *address = info.gw.addr;
    return true;
}
void net_mgr_add_json(cJSON *root)
{
    cJSON *network = cJSON_AddObjectToObject(root, "network");
    if (!network) return;
    char ip[16] = {0};
    bool connected = net_mgr_get_ip(ip, sizeof(ip)) == ESP_OK;
    cJSON_AddBoolToObject(network, "connected", connected);
    cJSON_AddStringToObject(network, "interface", connected ? (net_mgr_uses_ethernet() ? "ethernet" : "wifi") : "none");
    cJSON_AddStringToObject(network, "ip", ip);
    cJSON_AddBoolToObject(network, "wifi_connected", wifi_mgr_is_connected());
    cJSON *eth = cJSON_AddObjectToObject(network, "ethernet");
    if (!eth) return;
#if CONFIG_APP_ETHERNET_IP101
    cJSON_AddBoolToObject(eth, "enabled", true);
#else
    cJSON_AddBoolToObject(eth, "enabled", false);
#endif
    cJSON_AddBoolToObject(eth, "initialized", eth_mgr_is_active());
    cJSON_AddBoolToObject(eth, "link_up", eth_mgr_link_up());
    cJSON_AddBoolToObject(eth, "has_ip", eth_mgr_has_ip());
    cJSON_AddStringToObject(eth, "error", esp_err_to_name(eth_mgr_last_error()));
}
