/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_netif.h"
#include "net/wifi_mgr.h"
#include "xiaozhi/xiaozhi_mcp_tools.h"

static bool connected = true, setup_ap, no_netif, fail_ap, fail_ip, fail_identity;
static bool drop_during_ap, zero_ip, fail_dns, ipv6_dns;
static unsigned ap_calls;
static esp_netif_t station;
static esp_ip4_addr_t address(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    uint8_t bytes[] = {a,b,c,d}; esp_ip4_addr_t ip;
    memcpy(&ip.addr, bytes, sizeof(bytes)); return ip;
}
bool wifi_mgr_is_connected(void) { return connected; }
bool wifi_mgr_is_setup_ap_active(void) { return setup_ap; }
esp_err_t wifi_mgr_get_sta_ap_info(wifi_mgr_sta_ap_info_t *out)
{
    ap_calls++;
    if (fail_ap) return ESP_ERR_TIMEOUT;
    strcpy(out->ssid, "Test \"Wi-Fi\" \\ AP");
    out->rssi = -63; out->channel = 11;
    const uint8_t bssid[] = {0xA0,0xB1,0xC2,0xD3,0xE4,0xF5};
    memcpy(out->bssid, bssid, 6);
    if (drop_during_ap) connected = false;
    return ESP_OK;
}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key)
{
    assert(!strcmp(key,"WIFI_STA_DEF")); return no_netif ? NULL : &station;
}
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip)
{
    assert(netif == &station);
    if (fail_ip) return ESP_FAIL;
    ip->ip = zero_ip ? address(0,0,0,0) : address(192,168,71,120);
    ip->netmask = address(255,255,255,0); ip->gw = address(192,168,71,1);
    return ESP_OK;
}
esp_err_t esp_netif_get_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns)
{
    assert(netif == &station);
    if (fail_dns) return ESP_FAIL;
    dns->ip.type = ipv6_dns ? ESP_IPADDR_TYPE_V6 : ESP_IPADDR_TYPE_V4;
    dns->ip.u_addr.ip4 = type == ESP_NETIF_DNS_MAIN ? address(192,168,71,1) : address(1,1,1,1);
    return ESP_OK;
}
esp_err_t esp_netif_get_mac(esp_netif_t *netif, uint8_t mac[])
{
    assert(netif == &station);
    if (fail_identity) return ESP_FAIL;
    const uint8_t bytes[] = {2,0x11,0x22,0x33,0x44,0x55}; memcpy(mac,bytes,6);
    return ESP_OK;
}
esp_err_t esp_netif_get_hostname(esp_netif_t *netif, const char **hostname)
{
    assert(netif == &station);
    if (fail_identity) return ESP_FAIL;
    *hostname = "betta-test"; return ESP_OK;
}
static const cJSON *field(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root,key); assert(item); return item;
}
static void expect_text(const cJSON *root, const char *key, const char *value)
{
    const char *actual = cJSON_GetStringValue(field(root,key)); assert(actual && !strcmp(actual,value));
}
static void expect_no_lease(const cJSON *root)
{
    assert(cJSON_IsNull(field(root,"ip_address")) && cJSON_IsNull(field(root,"gateway")));
    assert(cJSON_IsNull(field(root,"netmask")) && cJSON_GetArraySize(field(root,"dns_servers")) == 0);
}
static int allocations_left;
static void *limited_malloc(size_t size) { return allocations_left-- > 0 ? malloc(size) : NULL; }

int main(void)
{
    cJSON *root = xiaozhi_mcp_network_info(); assert(root);
    assert(cJSON_IsTrue(field(root,"connected")) && cJSON_IsFalse(field(root,"setup_ap_active")));
    expect_text(root,"ip_address","192.168.71.120"); expect_text(root,"netmask","255.255.255.0");
    expect_text(root,"gateway","192.168.71.1"); expect_text(root,"mac_address","02:11:22:33:44:55");
    expect_text(root,"bssid","A0:B1:C2:D3:E4:F5"); expect_text(root,"hostname","betta-test");
    assert(field(root,"rssi_dbm")->valueint == -63 && field(root,"channel")->valueint == 11);
    assert(cJSON_GetArraySize(field(root,"dns_servers")) == 2);
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetArrayItem(field(root,"dns_servers"),1)),"1.1.1.1"));
    /* Round-trip escaping and a bounded, explicit allowlist of output fields. */
    char *text = cJSON_PrintUnformatted(root); assert(text && strlen(text) < 1024);
    cJSON *parsed = cJSON_Parse(text); expect_text(parsed,"ssid","Test \"Wi-Fi\" \\ AP");
    assert(cJSON_GetArraySize(parsed) == 13 && !strstr(text,"password") && !strstr(text,"token"));
    free(text); cJSON_Delete(parsed); cJSON_Delete(root);

    connected = false; setup_ap = true; unsigned prior_calls = ap_calls;
    root = xiaozhi_mcp_network_info(); assert(ap_calls == prior_calls);
    assert(cJSON_IsFalse(field(root,"connected")) && cJSON_IsTrue(field(root,"setup_ap_active")));
    expect_no_lease(root); assert(cJSON_IsNull(field(root,"ssid")) && cJSON_IsNull(field(root,"rssi_dbm")));
    expect_text(root,"mac_address","02:11:22:33:44:55"); cJSON_Delete(root);

    connected = true; setup_ap = false; fail_ap = true;
    root = xiaozhi_mcp_network_info(); expect_text(root,"ip_address","192.168.71.120");
    assert(cJSON_IsNull(field(root,"ssid")) && cJSON_IsNull(field(root,"channel")));
    cJSON_Delete(root); fail_ap = false;
    drop_during_ap = true;
    root = xiaozhi_mcp_network_info(); expect_no_lease(root);
    assert(cJSON_IsFalse(field(root,"connected")) && cJSON_IsNull(field(root,"bssid")));
    cJSON_Delete(root); drop_during_ap = false; connected = true;
    fail_ip = true;
    root = xiaozhi_mcp_network_info(); expect_no_lease(root); cJSON_Delete(root); fail_ip = false;
    zero_ip = true;
    root = xiaozhi_mcp_network_info(); expect_no_lease(root); cJSON_Delete(root); zero_ip = false;
    no_netif = true;
    root = xiaozhi_mcp_network_info(); expect_no_lease(root);
    assert(cJSON_IsNull(field(root,"mac_address")) && cJSON_IsNull(field(root,"hostname")));
    cJSON_Delete(root); no_netif = false;
    fail_identity = true; fail_dns = true;
    root = xiaozhi_mcp_network_info(); expect_text(root,"ip_address","192.168.71.120");
    assert(cJSON_IsNull(field(root,"mac_address")) && cJSON_GetArraySize(field(root,"dns_servers")) == 0);
    cJSON_Delete(root); fail_identity = false; fail_dns = false;
    ipv6_dns = true;
    root = xiaozhi_mcp_network_info(); assert(cJSON_GetArraySize(field(root,"dns_servers")) == 0);
    cJSON_Delete(root); ipv6_dns = false;
    /* Never return a plausible partial snapshot on allocation failure. */
    cJSON_Hooks hooks = {.malloc_fn=limited_malloc, .free_fn=free}; cJSON_InitHooks(&hooks);
    for (int i = 0; i < 70; ++i) {
        allocations_left = i; root = xiaozhi_mcp_network_info();
        if (root) assert(cJSON_GetArraySize(root) == 13 && cJSON_GetArraySize(field(root,"dns_servers")) == 2);
        cJSON_Delete(root);
    }
    cJSON_InitHooks(NULL);
    puts("MCP network snapshot: connected, offline, partial reads, disconnect, escaping and allocation failures passed");
}
