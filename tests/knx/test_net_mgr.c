/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "net/net_mgr.h"
#include "net/eth_mgr.h"
#include "util/auto_update.h"
static esp_netif_t wifi, ethernet;
static bool wifi_connected, wifi_up, ethernet_ready, ip_error;
static uint32_t wifi_ip = 1, ethernet_ip = 2;
bool wifi_mgr_is_connected(void) { return wifi_connected; }
bool eth_mgr_has_ip(void) { return ethernet_ready; }
bool eth_mgr_link_up(void) { return ethernet_ready; }
bool eth_mgr_is_active(void) { return true; }
esp_err_t eth_mgr_last_error(void) { return ESP_OK; }
esp_netif_t *eth_mgr_netif(void) { return ethernet_ready ? &ethernet : NULL; }
const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_OK"; }
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key)
{ assert(!strcmp(key, "WIFI_STA_DEF")); return &wifi; }
bool esp_netif_is_netif_up(esp_netif_t *netif)
{ assert(netif == &wifi); return wifi_up; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *info)
{
    assert(netif == &wifi || netif == &ethernet);
    memset(info, 0, sizeof(*info));
    if (ip_error) return ESP_FAIL;
    info->ip.addr = netif == &wifi ? wifi_ip : ethernet_ip; info->gw.addr = 254;
    return ESP_OK;
}
char *esp_ip4addr_ntoa(const esp_ip4_addr_t *addr, char *buf, int size)
{ return snprintf(buf, size, "192.0.2.%u", addr->addr) < size ? buf : NULL; }
int main(void)
{
    net_mgr_snapshot_t previous = {0}; char ip[16]; uint32_t gateway;
    assert(!net_mgr_is_connected());
    assert(!net_mgr_connection_changed(&previous));
    wifi_connected = wifi_up = true;
    assert(net_mgr_active_netif() == &wifi);
    assert(!net_mgr_connection_changed(&previous));
    assert(net_mgr_get_ip(ip, sizeof(ip)) == ESP_OK && !strcmp(ip,"192.0.2.1"));
    assert(net_mgr_get_gateway(&gateway) && gateway == 254);
    ethernet_ready = true;
    assert(net_mgr_active_netif() == &ethernet && net_mgr_uses_ethernet());
    assert(net_mgr_connection_changed(&previous));
    assert(!net_mgr_connection_changed(&previous));
    ethernet_ip = 3; assert(net_mgr_connection_changed(&previous));
    ethernet_ready = false; /* eth_mgr revokes readiness even while its DHCP lease is cached. */
    assert(net_mgr_active_netif() == &wifi && net_mgr_connection_changed(&previous));
    wifi_connected = false;
    assert(net_mgr_active_netif() == NULL && net_mgr_connection_changed(&previous));
    assert(net_mgr_get_ip(ip, sizeof(ip)) == ESP_ERR_NOT_FOUND && !ip[0]);
    wifi_connected = true; wifi_up = false; assert(!net_mgr_is_connected());
    wifi_up = true; wifi_ip = 0; assert(!net_mgr_is_connected());
    wifi_ip = 1; ip_error = true; assert(!net_mgr_is_connected());
    ip_error = false; ethernet_ready = true;
    cJSON *root = cJSON_CreateObject(); net_mgr_add_json(root);
    cJSON *network = cJSON_GetObjectItemCaseSensitive(root, "network");
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(network,"interface")),"ethernet"));
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(network,"ip")),"192.0.2.3"));
    cJSON_Delete(root);
    auto_update_input_t update = {.enabled=true, .update_available=true, .sha256_known=true,
        .bootloader_rollback=true, .time_valid=true, .local_minute_of_day=180, .idle_s=600,
        .rssi_dbm=-127, .free_internal=400000, .largest_internal=200000, .free_psram=8000000};
    assert(auto_update_decide(&update)==AUTO_UPDATE_WAIT_WIFI);
    update.ethernet_connected=true; assert(auto_update_decide(&update)==AUTO_UPDATE_GO);
    update.ethernet_connected=false; update.wifi_connected=true;
    assert(auto_update_decide(&update)==AUTO_UPDATE_WAIT_WIFI);
    update.rssi_dbm=-60; assert(auto_update_decide(&update)==AUTO_UPDATE_GO);
    puts("network selection: preference, fallback, address changes, stale leases and JSON passed");
}
