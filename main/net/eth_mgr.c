/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723; adapted from cptkirki/BETTA-HA-PANEL 3dc138a.
 */
#include "net/eth_mgr.h"
#include "sdkconfig.h"

#if CONFIG_APP_ETHERNET_IP101
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_netif_glue.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR >= 6
#include "esp_eth_phy_ip101.h"
#else
#include "esp_eth_phy.h"
#endif
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define ETH_LINK_BIT BIT0
#define ETH_IP_BIT BIT1
static const char *TAG = "eth_mgr";
static esp_eth_handle_t s_driver;
static esp_netif_t *s_netif;
static EventGroupHandle_t s_events;
static esp_err_t s_error = ESP_ERR_INVALID_STATE;
static bool s_started;

static void eth_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == ETH_EVENT && data && *(esp_eth_handle_t *)data == s_driver) {
        if (id == ETHERNET_EVENT_CONNECTED) {
            xEventGroupSetBits(s_events, ETH_LINK_BIT);
            ESP_LOGI(TAG, "IP101 link up; waiting for DHCP");
        } else if (id == ETHERNET_EVENT_DISCONNECTED || id == ETHERNET_EVENT_STOP) {
            /* esp_netif can retain the old lease after link-down. Never expose it as usable. */
            xEventGroupClearBits(s_events, ETH_LINK_BIT | ETH_IP_BIT);
            ESP_LOGI(TAG, "Ethernet down; Wi-Fi fallback available when connected");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP && data) {
        const ip_event_got_ip_t *event = data;
        if (event->esp_netif == s_netif && event->ip_info.ip.addr &&
            (xEventGroupGetBits(s_events) & ETH_LINK_BIT)) {
            xEventGroupSetBits(s_events, ETH_IP_BIT);
            ESP_LOGI(TAG, "Ethernet ready: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    } else if (base == IP_EVENT && id == IP_EVENT_ETH_LOST_IP && data &&
               ((const ip_event_got_ip_t *)data)->esp_netif == s_netif) {
        xEventGroupClearBits(s_events, ETH_IP_BIT);
    }
}

esp_err_t eth_mgr_init(void)
{
    if (s_started) return ESP_OK;
    s_events = xEventGroupCreate();
    if (!s_events) return s_error = ESP_ERR_NO_MEM;
    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;
    esp_eth_netif_glue_handle_t glue = NULL;
    esp_event_handler_instance_t eth_handler = NULL, ip_handler = NULL;
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    /* LCD-X expansion wiring: RMII RX 28/29/30, TX 34/35/49,
     * MDC 31, MDIO 52, external 50 MHz clock 50, PHY reset 51. */
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = 1;
    phy_cfg.reset_gpio_num = 51;
    mac = esp_eth_mac_new_esp32(&emac, &mac_cfg);
    phy = esp_eth_phy_new_ip101(&phy_cfg);
    esp_err_t err = ESP_ERR_NO_MEM;
    if (!mac || !phy) goto fail;
    esp_eth_config_t driver_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&driver_cfg, &s_driver);
    if (err != ESP_OK) goto fail;
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.route_prio = 200; /* Wi-Fi STA is 100; do not pin the default netif. */
    esp_netif_config_t netif_cfg = {.base = &base, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    s_netif = esp_netif_new(&netif_cfg);
    err = ESP_ERR_NO_MEM;
    if (!s_netif) goto fail;
    glue = esp_eth_new_netif_glue(s_driver);
    if (!glue) goto fail;
    err = esp_netif_attach(s_netif, glue);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event, NULL, &eth_handler);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, eth_event, NULL, &ip_handler);
    if (err != ESP_OK) goto fail;
    err = esp_eth_start(s_driver);
    if (err != ESP_OK) goto fail;
    s_started = true;
    s_error = ESP_OK;
    ESP_LOGI(TAG, "IP101 initialized; Ethernet preferred, Wi-Fi retained for fallback");
    return ESP_OK;
fail:
    if (ip_handler) esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_handler);
    if (eth_handler) esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_handler);
    if (glue) esp_eth_del_netif_glue(glue);
    if (s_netif) esp_netif_destroy(s_netif);
    s_netif = NULL;
    if (s_driver) esp_eth_driver_uninstall(s_driver);
    s_driver = NULL;
    if (phy) phy->del(phy);
    if (mac) mac->del(mac);
    vEventGroupDelete(s_events);
    s_events = NULL;
    ESP_LOGW(TAG, "IP101 unavailable (%s); continuing with Wi-Fi", esp_err_to_name(err));
    return s_error = err;
}

bool eth_mgr_is_active(void) { return s_started; }
bool eth_mgr_link_up(void)
{
    return s_started && (xEventGroupGetBits(s_events) & ETH_LINK_BIT);
}
bool eth_mgr_has_ip(void)
{
    esp_netif_ip_info_t info;
    return s_started && (xEventGroupGetBits(s_events) & (ETH_LINK_BIT | ETH_IP_BIT)) == (ETH_LINK_BIT | ETH_IP_BIT) &&
        esp_netif_is_netif_up(s_netif) && esp_netif_get_ip_info(s_netif, &info) == ESP_OK && info.ip.addr;
}
esp_err_t eth_mgr_wait_for_ip(uint32_t timeout_ms)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    xEventGroupWaitBits(s_events, ETH_LINK_BIT | ETH_IP_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return eth_mgr_has_ip() ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_netif_t *eth_mgr_netif(void) { return eth_mgr_has_ip() ? s_netif : NULL; }
esp_err_t eth_mgr_last_error(void) { return s_error; }
#else
esp_err_t eth_mgr_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t eth_mgr_wait_for_ip(uint32_t timeout_ms) { (void)timeout_ms; return ESP_ERR_NOT_SUPPORTED; }
bool eth_mgr_is_active(void) { return false; }
bool eth_mgr_link_up(void) { return false; }
bool eth_mgr_has_ip(void) { return false; }
esp_netif_t *eth_mgr_netif(void) { return NULL; }
esp_err_t eth_mgr_last_error(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif
