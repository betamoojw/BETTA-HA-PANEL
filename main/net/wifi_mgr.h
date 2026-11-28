/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Wi-Fi configuration, setup AP management and station diagnostics.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "app_config.h"

/** @brief Optional setup AP overrides, consumed during wifi_mgr_start_setup_ap(). */
typedef struct
{
    const char *ssid;         /**< NULL or empty generates a setup network name. */
    const char *password;     /**< NULL or empty generates a passphrase for this AP start. */
    const char *country_code; /**< Optional radio country code; NULL selects the default. */
    uint8_t channel;          /**< Zero uses the configured default channel. */
    uint8_t max_connection;   /**< Zero uses the default limit on simultaneous clients. */
} wifi_mgr_ap_config_t;

/** @brief One scan result copied by value; APs are matched by BSSID, not SSID. */
typedef struct
{
    char ssid[APP_WIFI_SSID_MAX_LEN];
    int8_t rssi; /**< Signal strength in dBm. */
    uint8_t authmode;
    uint8_t channel;
    uint8_t bssid[6];
    bool connected; /**< The station is currently associated with this BSSID. */
} wifi_mgr_scan_result_t;

/** @brief Station connection settings, consumed during wifi_mgr_init(). */
typedef struct
{
    const char *ssid;         /**< Nonempty saved network name. */
    const char *password;     /**< NULL or empty for an open network. */
    const char *country_code; /**< Optional radio country code. */
    const char *bssid;        /**< Optional access point lock; absent or invalid text leaves it unlocked. */
    bool wait_for_ip;         /**< Wait for an IPv4 address before returning. */
    int connect_timeout_ms;   /**< Startup wait in milliseconds; a nonpositive value uses the default. */
    int max_retries;          /**< Station retry budget; zero uses the default and negative is invalid. */
} wifi_mgr_config_t;

/** @brief Snapshot of the associated station AP, independent of DHCP. */
typedef struct
{
    char ssid[APP_WIFI_SSID_MAX_LEN];
    int8_t rssi; /**< Signal strength in dBm. */
    uint8_t authmode;
    uint8_t channel;
    uint8_t bssid[6];
} wifi_mgr_sta_ap_info_t;

/** @brief Recovery diagnostics retained separately from the active reconnect state.
 *
 * Uptime fields are milliseconds since boot; zero means not observed. The
 * persistence layer stores this layout directly, so keep its binary layout stable.
 */
typedef struct
{
    uint8_t last_disconnect_reason;      /**< Wi-Fi driver reason code, not an ESP error code. */
    int64_t last_disconnect_uptime_ms;   /**< Uptime of the most recent disconnect. */
    uint32_t disconnect_count;           /**< Observed disconnects since boot. */
    uint32_t reconnect_attempts;         /**< Recovery connect attempts for the current outage, reset after a successful lease. */
    uint32_t hard_recover_count;         /**< Hosted transport recoveries attempted since boot. */
    int32_t last_hard_recover_err;       /**< Result of the latest hard recovery, stored as an esp_err_t value. */
    int64_t last_hard_recover_uptime_ms; /**< Uptime of the recorded hard recovery. */
} wifi_mgr_diag_t;

/** @brief Start the station and optionally wait for the station IP event.
 * @return ESP_OK on successful startup; without a wait this does not imply a lease.
 */
esp_err_t wifi_mgr_init(const wifi_mgr_config_t *cfg);
/** @return Tracked station connectivity, not whether the setup AP is running. */
bool wifi_mgr_is_connected(void);
/** @brief Request station recovery using the configured reconnect policy. */
esp_err_t wifi_mgr_force_reconnect(void);
/** @brief Recover the Hosted transport, or request station reconnect on native Wi-Fi. */
esp_err_t wifi_mgr_force_transport_recover(void);
/** @brief Start provisioning AP+STA mode; NULL uses defaults including a generated passphrase. */
esp_err_t wifi_mgr_start_setup_ap(const wifi_mgr_ap_config_t *cfg);
/** @brief Leave setup AP mode, preserving the station path where supported. */
esp_err_t wifi_mgr_stop_setup_ap(void);
/** @brief Start the setup-mode retry worker for saved station credentials.
 *
 * Probes pause while a client uses the setup AP. A successful probe schedules
 * a reboot so the normal startup path can leave the provisioning screen.
 */
esp_err_t wifi_mgr_start_recovery(void);
/** @return Whether provisioning AP mode is currently active. */
bool wifi_mgr_is_setup_ap_active(void);
/** @return Borrowed setup SSID text; empty when inactive and mutable on a later AP start. */
const char *wifi_mgr_get_setup_ap_ssid(void);
/** @return Borrowed setup passphrase for the active AP, or an empty string when inactive. */
const char *wifi_mgr_get_setup_ap_password(void);
/** @brief Format the station IPv4 address into a caller buffer; allow 16 bytes including the NUL. */
esp_err_t wifi_mgr_get_sta_ip(char *out, size_t out_len);
/** @return True for a station gateway; writes network byte order only on success. */
bool wifi_mgr_get_sta_gateway(uint32_t *out_addr);
/** @brief Run any deferred C6 update after the host OTA image is marked valid.
 * Running it earlier could restart the host before validation and cause rollback.
 */
void wifi_mgr_run_deferred_c6_update(void);
/** @brief Copy the version actually reported by the running coprocessor, not the bundled image's version.
 * @return False before a successful query or on hardware without a coprocessor.
 */
bool wifi_mgr_get_c6_version(char *out, size_t out_len);
/** @brief Format the setup AP's IPv4 address separately from the station address. */
esp_err_t wifi_mgr_get_ap_ip(char *out, size_t out_len);
/** @brief Copy association metadata using the serialized Wi-Fi API; may wait for its mutex. */
esp_err_t wifi_mgr_get_sta_ap_info(wifi_mgr_sta_ap_info_t *out_info);
/** @brief Try to read RSSI without waiting for the Wi-Fi API mutex.
 * @retval ESP_ERR_TIMEOUT The Wi-Fi API could not be acquired; leave the reading unchanged.
 */
esp_err_t wifi_mgr_try_get_sta_rssi(int8_t *out_rssi_dbm);
/** @brief Read station RSSI in dBm using the same bounded mutex wait as AP info. */
esp_err_t wifi_mgr_get_sta_rssi(int8_t *out_rssi_dbm);
/** @brief Run a bounded active scan and return distinct visible BSSIDs.
 * @param[out] results Caller-owned array.
 * @param max_results Number of available entries in results.
 * @param[out] out_count Number of entries returned; initialized to zero before scanning.
 * @return ESP_OK for a completed scan; timeout or driver errors are propagated.
 */
esp_err_t wifi_mgr_scan(wifi_mgr_scan_result_t *results, size_t max_results, size_t *out_count);
/** @brief Copy live recovery counters without querying the Wi-Fi driver or flash. */
void wifi_mgr_get_diag(wifi_mgr_diag_t *out);
/** @return Static, untranslated disconnect reason label for logs and diagnostics. */
const char *wifi_mgr_reason_to_str(uint8_t reason);
