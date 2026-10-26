/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi_mcp_tools.h"

#include <stdio.h>
#include "esp_netif.h"
#include "net/wifi_mgr.h"

static bool add_text(cJSON *object, const char *key, const char *value)
{
    return value ? cJSON_AddStringToObject(object, key, value) != NULL :
                   cJSON_AddNullToObject(object, key) != NULL;
}

static bool add_ip(cJSON *object, const char *key, const esp_ip4_addr_t *address)
{
    char text[16];
    if (!address || !address->addr) return add_text(object, key, NULL);
    snprintf(text, sizeof(text), IPSTR, IP2STR(address));
    return add_text(object, key, text);
}

static void format_mac(const uint8_t mac[6], char text[18])
{
    snprintf(text, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

cJSON *xiaozhi_mcp_network_info(void)
{
    /* The Wi-Fi manager serializes the only C6 RPC, with a bounded lock wait.
     * ESP-NETIF queries below read local interface state. Never scan, reconnect
     * or load saved credentials merely to answer a voice question. */
    bool connected = wifi_mgr_is_connected();
    wifi_mgr_sta_ap_info_t ap = {0};
    bool have_ap = connected && wifi_mgr_get_sta_ap_info(&ap) == ESP_OK;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {0};
    bool have_ip = connected && netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr;
    esp_netif_dns_info_t dns[2] = {0};
    bool have_dns[2] = {false, false};
    for (int i = 0; have_ip && i < 2; ++i) {
        have_dns[i] = esp_netif_get_dns_info(netif, i == 0 ? ESP_NETIF_DNS_MAIN : ESP_NETIF_DNS_BACKUP,
            &dns[i]) == ESP_OK && dns[i].ip.type == ESP_IPADDR_TYPE_V4 && dns[i].ip.u_addr.ip4.addr;
    }
    uint8_t mac[6] = {0};
    char mac_text[18], bssid[18];
    bool have_mac = netif && esp_netif_get_mac(netif, mac) == ESP_OK;
    if (have_mac) format_mac(mac, mac_text);
    const char *hostname = NULL;
    if (!netif || esp_netif_get_hostname(netif, &hostname) != ESP_OK) hostname = NULL;

    /* A disconnect during the RPC must not expose the previous lease/AP as
     * current. Station MAC and hostname remain meaningful while disconnected. */
    connected = connected && wifi_mgr_is_connected();
    have_ap = have_ap && connected;
    have_ip = have_ip && connected;
    if (have_ap) format_mac(ap.bssid, bssid);
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    cJSON *servers = cJSON_AddArrayToObject(result, "dns_servers");
    bool ok = servers && cJSON_AddStringToObject(result, "interface", "wifi") &&
        cJSON_AddBoolToObject(result, "connected", connected) &&
        cJSON_AddBoolToObject(result, "setup_ap_active", wifi_mgr_is_setup_ap_active()) &&
        add_text(result, "ssid", have_ap ? ap.ssid : NULL) &&
        add_text(result, "bssid", have_ap ? bssid : NULL) &&
        add_text(result, "mac_address", have_mac ? mac_text : NULL) &&
        add_text(result, "hostname", hostname && hostname[0] ? hostname : NULL) &&
        add_ip(result, "ip_address", have_ip ? &ip.ip : NULL) &&
        add_ip(result, "netmask", have_ip ? &ip.netmask : NULL) &&
        add_ip(result, "gateway", have_ip ? &ip.gw : NULL);
    ok = ok && (have_ap ? cJSON_AddNumberToObject(result, "rssi_dbm", ap.rssi) :
                         cJSON_AddNullToObject(result, "rssi_dbm"));
    ok = ok && (have_ap ? cJSON_AddNumberToObject(result, "channel", ap.channel) :
                         cJSON_AddNullToObject(result, "channel"));
    for (int i = 0; ok && have_ip && i < 2; ++i) {
        if (!have_dns[i]) continue;
        char text[16];
        snprintf(text, sizeof(text), IPSTR, IP2STR(&dns[i].ip.u_addr.ip4));
        cJSON *entry = cJSON_CreateString(text);
        if (!entry || !cJSON_AddItemToArray(servers, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    if (!ok) { cJSON_Delete(result); return NULL; }
    return result;
}
