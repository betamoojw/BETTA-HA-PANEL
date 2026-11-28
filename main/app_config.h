/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Board reference geometry, firmware limits, and compile-time defaults.
 *
 * Runtime settings can override the corresponding defaults. Use the applied
 * display geometry rather than these reference dimensions after rotation.
 */
#pragma once

#include "sdkconfig.h"

/** @def APP_NAME
 * @brief Variant-specific application name used for boot and status reporting.
 */
/** @def APP_BOARD_DESCRIPTION
 * @brief Human-readable board identification reported to the editor.
 */
/** @def APP_SCREEN_WIDTH
 * @brief Reference logical screen width in pixels, before runtime rotation.
 */
/** @def APP_SCREEN_HEIGHT
 * @brief Reference logical screen height in pixels, before runtime rotation.
 */
/** @def APP_CONTENT_BOX_WIDTH
 * @brief Reference dashboard width in pixels.
 */
/** @def APP_CONTENT_BOX_HEIGHT
 * @brief Reference dashboard height in pixels, excluding the two bars.
 */
/* ---- Panel-variant-dependent geometry & identity ----------------------
 * Selected via CONFIG_APP_PANEL_VARIANT_* (see main/Kconfig.projbuild).
 * Defaults fall back to the 4" 720x720 baseline so an older sdkconfig
 * (pre-variant) still builds the smart86 target unchanged. */
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
/* Existing sdkconfig values override defaults. Reject the old IRAM profile
 * that prevented allocation of the internal HTTP task on this board. */
#  if (defined(CONFIG_ESP_WIFI_IRAM_OPT) && CONFIG_ESP_WIFI_IRAM_OPT) || \
      (defined(CONFIG_ESP_WIFI_RX_IRAM_OPT) && CONFIG_ESP_WIFI_RX_IRAM_OPT)
#    error \
        "panels3 requires Wi-Fi IRAM optimizations disabled; regenerate build/panels3/sdkconfig from defaults (docs/development/build.md)"
#  endif
#  define APP_NAME               "betta-ha-panel-s3"
#  define APP_BOARD_DESCRIPTION  "Guition ESP32-S3-4848S040"
#  define APP_SCREEN_WIDTH       480
#  define APP_SCREEN_HEIGHT      480
#  define APP_CONTENT_BOX_WIDTH  480
#  define APP_CONTENT_BOX_HEIGHT 360
#elif defined(CONFIG_APP_PANEL_VARIANT_10INCH_1280)
#  define APP_NAME               "betta-ha-panel-10.1"
#  define APP_BOARD_DESCRIPTION  "Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 (LCD-X)"
#  define APP_SCREEN_WIDTH       1280
#  define APP_SCREEN_HEIGHT      800
#  define APP_CONTENT_BOX_WIDTH  1280
#  define APP_CONTENT_BOX_HEIGHT 680
#elif defined(CONFIG_APP_PANEL_VARIANT_TAB5_1280)
/* M5Stack Tab5: 5-inch 1280x720 reference landscape geometry. */
#  define APP_NAME               "betta-ha-panel-tab5"
#  define APP_BOARD_DESCRIPTION  "M5Stack Tab5"
#  define APP_SCREEN_WIDTH       1280
#  define APP_SCREEN_HEIGHT      720
#  define APP_CONTENT_BOX_WIDTH  1280
#  define APP_CONTENT_BOX_HEIGHT 600
#elif defined(CONFIG_APP_PANEL_VARIANT_10INCH_JC)
/* Guition JC8012P4A1: native portrait 800×1280, LVGL rotation 90° → landscape */
#  define APP_NAME "betta-ha-panel-jc"
#  if CONFIG_APP_PANELJC_PANEL_V2
#    define APP_BOARD_DESCRIPTION "Guition JC8012P4A1 (\"V2\" panel, rear case date code ~2628+)"
#  else
#    define APP_BOARD_DESCRIPTION "Guition JC8012P4A1 (\"V1\"/original panel, rear case date code up to ~2627)"
#  endif
#  define APP_SCREEN_WIDTH       1280
#  define APP_SCREEN_HEIGHT      800
#  define APP_CONTENT_BOX_WIDTH  1280
#  define APP_CONTENT_BOX_HEIGHT 680
#else /* CONFIG_APP_PANEL_VARIANT_4INCH_720 (default) */
#  define APP_NAME               "betta-ha-panel"
#  define APP_BOARD_DESCRIPTION  "Waveshare ESP32-P4-WIFI6-Touch-LCD-4B"
#  define APP_SCREEN_WIDTH       720
#  define APP_SCREEN_HEIGHT      720
#  define APP_CONTENT_BOX_WIDTH  720
#  define APP_CONTENT_BOX_HEIGHT 600
#endif
#define APP_CONTENT_BOX_X                0  /**< Dashboard origin along the logical x axis, in pixels. */
#define APP_CONTENT_BOX_Y                60 /**< Dashboard origin below the top bar, in pixels. */
#define APP_NAV_BUTTON_COUNT             5  /**< Legacy fixed button count; current navigation capacity depends on width. */
#define APP_LVGL_ANTIALIASING            0  /**< Compile-time default for LVGL display antialiasing. */
#define APP_UI_TEST_WEATHER_ICON_OVERLAY 0  /**< Enable the weather icon diagnostic overlay in test builds. */

#define APP_DISPLAY_ACTIVE_BRIGHTNESS_PERCENT 100             /**< Default backlight level while active. */
#define APP_DISPLAY_DIM_BRIGHTNESS_PERCENT    50              /**< Default backlight level after inactivity. */
#define APP_DISPLAY_DIM_TIMEOUT_MS            (5 * 60 * 1000) /**< Default inactivity delay in milliseconds. */
#define APP_DISPLAY_NIGHT_BRIGHTNESS_PERCENT  1               /**< Default backlight level during the night schedule. */

#define APP_EVENT_QUEUE_LENGTH  96 /**< Number of preallocated application event slots. */
#define APP_EVENT_QUEUE_WAIT_MS 50 /**< Reference event wait budget in milliseconds; the queue API takes ticks. */

#define APP_LAYOUT_PATH "/littlefs/layout.json" /**< Canonical persisted dashboard layout. */
/** @brief Maximum stored layout JSON in bytes, excluding its trailing NUL.
 *
 * The layout buffer and parse tree live in PSRAM. The limit accommodates the
 * 224-widget total with typical titles; the editor's transient aliases are separate.
 */
#define APP_LAYOUT_MAX_JSON_LEN 32768
#define APP_LAYOUT_MAX_ERRORS   16 /**< Maximum validation messages retained for a rejected layout. */

#define APP_SETTINGS_PATH         "/littlefs/settings.json" /**< Persisted runtime settings document. */
#define APP_SETTINGS_MAX_JSON_LEN 4096                      /**< Maximum serialized settings bytes, excluding the trailing NUL. */

/** @name Settings string capacities
 * Capacities are bytes including the NUL terminator.
 * @{
 */
#define APP_WIFI_SSID_MAX_LEN         33  /**< Wi-Fi network name buffer. */
#define APP_WIFI_PASSWORD_MAX_LEN     65  /**< Wi-Fi password buffer. */
#define APP_WIFI_COUNTRY_CODE_MAX_LEN 3   /**< Two-letter regulatory country code buffer. */
#define APP_WIFI_BSSID_MAX_LEN        18  /**< Colon-separated access point MAC address buffer. */
#define APP_HA_WS_URL_MAX_LEN         256 /**< Home Assistant WebSocket endpoint buffer. */
#define APP_HA_ACCESS_TOKEN_MAX_LEN   512 /**< Home Assistant access token buffer. */
#define APP_MQTT_BROKER_HOST_MAX_LEN  128 /**< Broker host buffer, separate from the port. */
#define APP_MQTT_USERNAME_MAX_LEN     64  /**< MQTT user name buffer. */
#define APP_MQTT_PASSWORD_MAX_LEN     128 /**< MQTT password buffer. */
#define APP_MQTT_TOPIC_PREFIX_MAX_LEN 32  /**< Topic prefix buffer before per-device suffixes are appended. */
#define APP_MQTT_DEVICE_NAME_MAX_LEN  48  /**< MQTT device name buffer. */
#define APP_NTP_SERVER_MAX_LEN        128 /**< SNTP server name buffer. */
#define APP_TIME_TZ_MAX_LEN           128 /**< Timezone identifier or rules buffer. */
#define APP_UI_LANGUAGE_MAX_LEN       16  /**< Normalized UI language code buffer. */
/** @} */

#define APP_UI_DEFAULT_LANGUAGE "en" /**< Built-in fallback catalog language. */

#define APP_I18N_DIR          "/littlefs/i18n" /**< Directory for custom language overrides. */
#define APP_I18N_MAX_JSON_LEN 32768            /**< Maximum custom language JSON payload in bytes. */

#define APP_THEME_DIR         "/littlefs/themes"           /**< Directory for persisted custom palettes. */
#define APP_THEME_ACTIVE_PATH "/littlefs/themes/active.id" /**< File containing the selected theme ID. */

/** @brief Opt-in for the paneljc ES8311 output path.
 *
 * The tested unit was silent at every volume on 2026-10-04. With this disabled,
 * the driver reports audio unavailable and the editor hides the Sound settings.
 */
#define APP_PANELJC_AUDIO 0

/** @brief Configurable items left of the clock; narrow displays may render fewer. */
#define APP_TOPBAR_LEFT_SLOTS 3

#define APP_SETUP_AP_SSID_PREFIX     "BETTA-Setup" /**< Prefix for the generated provisioning network name. */
#define APP_SETUP_AP_PASSWORD_LEN    19            /**< Buffer for the generated per-start WPA2 passphrase, including the NUL. */
#define APP_SETUP_AP_CHANNEL         1             /**< Default radio channel for the provisioning network. */
#define APP_SETUP_AP_MAX_CONNECTIONS 4             /**< Maximum simultaneous provisioning network clients. */

/** @def APP_MAX_PAGES
 * @brief Static page capacity selected by reference screen width.
 *
 * The editor's limit follows the current logical width. At higher counts the
 * navigation bar uses compact tabs so every page remains reachable.
 */
#if APP_SCREEN_WIDTH >= 1000
#  define APP_MAX_PAGES 12
#elif APP_SCREEN_WIDTH >= 700
#  define APP_MAX_PAGES 8
#else
#  define APP_MAX_PAGES 5
#endif
#define APP_MAX_WIDGETS_PER_PAGE 32 /**< Per-page bound shared by firmware and editor validation. */
/** @brief Per-panel widget policy cap, also bounded by its page capacity. */
#define APP_MAX_WIDGETS_TOTAL ((APP_MAX_PAGES * APP_MAX_WIDGETS_PER_PAGE) > 224 ? 224 : (APP_MAX_PAGES * APP_MAX_WIDGETS_PER_PAGE))

/** @name Layout and cached entity strings
 * Capacities are bytes including the NUL terminator.
 * @{
 */
#define APP_MAX_ENTITY_ID_LEN 96 /**< Cached entity identifier and binding buffer. */
#define APP_MAX_WIDGET_ID_LEN 32 /**< Widget identifier buffer, also used by UI tap events. */
#define APP_MAX_PAGE_ID_LEN   32 /**< Page identifier buffer, also used by navigation events. */
#define APP_MAX_STATE_LEN     64 /**< Cached entity state text buffer. */
#define APP_MAX_NAME_LEN      64 /**< Shared display name buffer. */
#define APP_MAX_UNIT_LEN      24 /**< Entity unit-of-measurement buffer. */
#define APP_MAX_ICON_LEN      64 /**< Icon identifier buffer. */
#define APP_MAX_UI_OPTION_LEN 24 /**< Buffer for a persisted UI option. */
#define APP_MAX_COLOR_STR_LEN 16 /**< Buffer for a serialized color string. */
/** @} */

/** @name Notification payload limits
 * String capacities include the NUL terminator; the wire payload limit does not.
 * @{
 */
#define APP_NOTIFICATION_MAX_JSON_LEN         2048 /**< Maximum incoming JSON bytes. */
#define APP_NOTIFICATION_MAX_TITLE_LEN        64   /**< Popup heading buffer. */
#define APP_NOTIFICATION_MAX_MESSAGE_LEN      768  /**< Popup body buffer, independent of the heading. */
#define APP_NOTIFICATION_MAX_ID_LEN           64   /**< Optional message identifier buffer. */
#define APP_NOTIFICATION_MAX_ACTION_LABEL_LEN 24   /**< Optional action button text buffer. */
#define APP_NOTIFICATION_MAX_TIMEOUT_SEC      3600 /**< Maximum auto-dismiss interval in seconds; zero disables auto-dismiss. */
/** @} */

/** @def APP_HA_MAX_ENTITIES
 * @brief Entity registry capacity; smaller on S3 to leave PSRAM for display and network workloads.
 */
/** @def APP_HA_MAX_STATES
 * @brief Cached state capacity for the selected board profile.
 */
/* All ESP32-P4 variants (10" and 4") keep the cache in PSRAM (about 8 MB for
 * 4096 entities and states, of 24+ MB free); raise the ceiling for large HA installs. */
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
/* Leave room in 8 MB PSRAM for RGB buffers, TLS, fonts and images. */
#  define APP_HA_MAX_ENTITIES 512
#  define APP_HA_MAX_STATES   512
#else
#  define APP_HA_MAX_ENTITIES 4096
#  define APP_HA_MAX_STATES   4096
#endif
/** @brief Cached attribute JSON capacity in bytes, including the NUL terminator.
 *
 * Must fit weather entity attributes incl. the compact forecast array plus temperature/humidity/units.
 * The P4 boards have plenty of PSRAM: leave room for the weather detail attributes (pressure, wind,
 * visibility) next to the forecast.
 */
#define APP_HA_ATTRS_MAX_LEN             1280
#define APP_HA_WEATHER_DETAILS           (APP_HA_ATTRS_MAX_LEN >= 1280) /**< Include weather detail fields when the cache can hold them. */
#define APP_HA_LIGHT_DISCOVERY_MAX_ITEMS 640   /**< Discovery result capacity, including room cards across areas. */
#define APP_HA_LIGHT_DISCOVERY_MAX_AREAS 96    /**< Area registry capacity for discovery. */
#define APP_HA_LIGHT_DISCOVERY_MAX_DEVICES 256 /**< Device registry capacity for discovery. */
#define APP_HA_DISCOVERY_ID_MAX_LEN        48  /**< Buffer for discovery registry IDs, including the NUL. */
#define APP_HA_DISCOVERY_DOMAIN_MAX_LEN    24  /**< Domain filter buffer including the NUL; fits alarm_control_panel. */
#define APP_HA_DISCOVERY_SEARCH_MAX_LEN    64  /**< Search filter buffer including the NUL. */
/** @brief Enable raw registry discovery over WebSocket independently of the template picker. */
#define APP_HA_LIGHT_DISCOVERY_REGISTRY_ENABLED 0
/** @brief Enable paged discovery rendered by the Home Assistant template endpoint. */
#define APP_HA_LIGHT_DISCOVERY_TEMPLATE_ENABLED 1
/** @def APP_HA_LIGHT_DISCOVERY_PAGE_SIZE
 * @brief Number of entries requested per template page, reduced on S3.
 */
/** @def APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES
 * @brief Maximum response payload bytes accepted for one discovery page.
 */
/** @def APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_FREE_BYTES
 * @brief Free internal bytes needed before ordinary template discovery may start.
 */
/** @def APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_LARGEST_BYTES
 * @brief Minimum contiguous internal block in bytes for ordinary discovery.
 */
/** @def APP_HA_DISCOVERY_USER_MIN_INTERNAL_FREE_BYTES
 * @brief Free internal bytes required for prioritized discovery, subject to the S3 plain-HTTP policy.
 */
/** @def APP_HA_DISCOVERY_USER_MIN_INTERNAL_LARGEST_BYTES
 * @brief Minimum contiguous internal block in bytes for prioritized discovery.
 */
/** @def APP_HA_HTTP_TX_BUFFER_SIZE
 * @brief HA HTTP transmit buffer in bytes, sized for the board's allocation policy.
 */
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
/* HTTP RX/TX prefer PSRAM. Keep TLS admission at 36 KiB; ordinary HTTP may use
 * the guarded 32 KiB prepared reserve plus 2 KiB for client/header setup.
 * These are admission reserves, not a measured peak or a TLS qualification. */
#  define APP_HA_LIGHT_DISCOVERY_PAGE_SIZE                     12
#  define APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES                   8192U
#  define APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_FREE_BYTES    (40U * 1024U)
#  define APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_LARGEST_BYTES (8U * 1024U)
#  define APP_HA_DISCOVERY_USER_MIN_INTERNAL_FREE_BYTES        (36U * 1024U)
#  define APP_HA_DISCOVERY_USER_MIN_INTERNAL_LARGEST_BYTES     (8U * 1024U)
/* malloc(1024) prefers internal RAM on S3; 2048 moves TX to PSRAM like RX. */
#  define APP_HA_HTTP_TX_BUFFER_SIZE                     2048
#  define APP_HA_DISCOVERY_PREPARED_INTERNAL_FREE_BYTES  (32U * 1024U) /**< Free internal bytes reserved before connecting. */
#  define APP_HA_DISCOVERY_CONNECTED_INTERNAL_FREE_BYTES (24U * 1024U) /**< Free internal bytes reserved once connected. */
#  define APP_HA_DISCOVERY_REQUEST_INTERNAL_BLOCK_BYTES (4U * 1024U) /**< Contiguous block required while preparing or reading a request. */
#  define APP_HA_DISCOVERY_HTTP_SETUP_INTERNAL_BYTES    (2U * 1024U) /**< Additional bytes for plain HTTP client and header setup. */
/** @brief S3 free internal bytes required for prioritized plain-HTTP discovery. */
#  define APP_HA_DISCOVERY_HTTP_MIN_INTERNAL_FREE_BYTES \
      (APP_HA_DISCOVERY_PREPARED_INTERNAL_FREE_BYTES + APP_HA_DISCOVERY_HTTP_SETUP_INTERNAL_BYTES)
#  define APP_HA_HTTP_IDLE_RELEASE_MS 2000 /**< Idle milliseconds before releasing the cached HTTP session on S3. */
#else
/* Preserve the P4 admission thresholds and 40-entity pages. */
#  define APP_HA_LIGHT_DISCOVERY_PAGE_SIZE                     40
#  define APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES                   16384U
#  define APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_FREE_BYTES    (120U * 1024U)
#  define APP_HA_DISCOVERY_TEMPLATE_MIN_INTERNAL_LARGEST_BYTES (48U * 1024U)
#  define APP_HA_DISCOVERY_USER_MIN_INTERNAL_FREE_BYTES        (64U * 1024U)
#  define APP_HA_DISCOVERY_USER_MIN_INTERNAL_LARGEST_BYTES     (24U * 1024U)
#  define APP_HA_HTTP_TX_BUFFER_SIZE                           1024
#endif

#define APP_HA_QUEUE_LENGTH     96    /**< Receive queue capacity for WebSocket messages, not model entries. */
#define APP_HA_TASK_STACK       12288 /**< HA client task stack in bytes. */
#define APP_HA_TASK_PRIO        8     /**< FreeRTOS priority of the HA client task. */
#define APP_HA_COVER_TASK_STACK 8192  /**< Cover fetch worker stack in bytes. */
#define APP_HA_COVER_TASK_PRIO  3     /**< FreeRTOS priority of the cover fetch worker. */

/** @brief Preserve immediate light commands by sending an explicit zero transition. */
#define APP_HA_LIGHT_USE_TRANSITION_ZERO 1

/** @brief Use guarded light.toggle for panel power actions when enabled. */
#define APP_HA_LIGHT_POWER_USE_TOGGLE 0

/** @brief Keep the legacy WebSocket entity subscription path enabled. */
#define APP_HA_USE_WS_ENTITIES_SUBSCRIPTION 1

/** @brief Enable verbose state-routing logs from the WebSocket through the model to the UI. */
#define APP_HA_ROUTE_TRACE_LOG 0

#define APP_UI_TASK_STACK 16384 /**< UI task stack in bytes; observed use was at most 8.6 KB on paneljc (2026-09-30). */
#define APP_UI_TASK_PRIO  4     /**< FreeRTOS priority of the dashboard task. */

#define APP_LVGL_TASK_STACK 20480 /**< LVGL adapter stack in bytes; leave room for popup and glass drawing. */

#define APP_HTTP_PORT 80 /**< Panel editor and API listen port. */
/** @brief Shared HTTP task stack in bytes, including screenshot headroom.
 *
 * On paneljc an editor request left only 1.4 KB free with the old 12 KB stack (2026-09-30).
 */
#define APP_HTTP_TASK_STACK 14336

#define APP_OTA_URL_MAX_LEN  512  /**< OTA URL buffer capacity including the NUL. */
#define APP_OTA_JSON_MAX_LEN 768  /**< Maximum OTA control request JSON bytes. */
#define APP_OTA_CHUNK_SIZE   4096 /**< Number of image bytes read at a time by the OTA stream. */
#define APP_OTA_URL_TOTAL_TIMEOUT_MS \
    (15 * 60 * 1000)            /**< Download deadline in milliseconds, below the automatic-update observer's timeout. */
#define APP_OTA_TASK_STACK 8192 /**< URL install worker stack in bytes. */
#define APP_OTA_TASK_PRIO  5    /**< FreeRTOS priority of the URL install worker. */
/** @brief Release repository queried by the periodic update checker. */
#define APP_OTA_RELEASE_REPO "andre68723/BETTA-HA-PANEL"

/** @def APP_HAVE_HOSTED_C6_FW_IMAGE
 * @brief Set by the build when the optional hosted coprocessor image is embedded.
 */
#ifndef APP_HAVE_HOSTED_C6_FW_IMAGE
#  define APP_HAVE_HOSTED_C6_FW_IMAGE 0
#endif

/** @def APP_WIFI_SSID
 * @brief Compile-time station SSID default; placeholder values are not treated as credentials.
 */
#ifdef CONFIG_APP_WIFI_SSID
#  define APP_WIFI_SSID CONFIG_APP_WIFI_SSID
#else
#  define APP_WIFI_SSID "YOUR_WIFI_SSID"
#endif

/** @def APP_WIFI_PASSWORD
 * @brief Compile-time station password default; placeholder values are ignored.
 */
#ifdef CONFIG_APP_WIFI_PASSWORD
#  define APP_WIFI_PASSWORD CONFIG_APP_WIFI_PASSWORD
#else
#  define APP_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

/** @def APP_WIFI_COUNTRY_CODE
 * @brief Default regulatory country code used when no override is saved.
 */
#ifdef CONFIG_APP_WIFI_COUNTRY_CODE
#  define APP_WIFI_COUNTRY_CODE CONFIG_APP_WIFI_COUNTRY_CODE
#else
#  define APP_WIFI_COUNTRY_CODE "DE"
#endif

/** @def APP_WIFI_WAIT_FOR_IP
 * @brief Enable a bounded startup wait for the station IP when Ethernet has no lease.
 */
#ifdef CONFIG_APP_WIFI_WAIT_FOR_IP
#  define APP_WIFI_WAIT_FOR_IP CONFIG_APP_WIFI_WAIT_FOR_IP
#else
#  define APP_WIFI_WAIT_FOR_IP 1
#endif

/** @def APP_WIFI_CONNECT_TIMEOUT_MS
 * @brief Startup connection timeout in milliseconds.
 */
#ifdef CONFIG_APP_WIFI_CONNECT_TIMEOUT_MS
#  define APP_WIFI_CONNECT_TIMEOUT_MS CONFIG_APP_WIFI_CONNECT_TIMEOUT_MS
#else
#  define APP_WIFI_CONNECT_TIMEOUT_MS 15000
#endif

/** @def APP_WIFI_MAX_RETRIES
 * @brief Retry budget passed to the Wi-Fi manager during startup.
 */
#ifdef CONFIG_APP_WIFI_MAX_RETRIES
#  define APP_WIFI_MAX_RETRIES CONFIG_APP_WIFI_MAX_RETRIES
#else
#  define APP_WIFI_MAX_RETRIES 8
#endif

/** @def APP_WIFI_DISABLE_POWER_SAVE
 * @brief Disable station power saving when the board policy requests it.
 */
#ifdef CONFIG_APP_WIFI_DISABLE_POWER_SAVE
#  define APP_WIFI_DISABLE_POWER_SAVE CONFIG_APP_WIFI_DISABLE_POWER_SAVE
#else
#  define APP_WIFI_DISABLE_POWER_SAVE 1
#endif

/** @def APP_HOSTED_AUTO_UPDATE_C6_FW
 * @brief Enable the optional embedded coprocessor firmware updater.
 */
#ifdef CONFIG_APP_HOSTED_AUTO_UPDATE_C6_FW
#  define APP_HOSTED_AUTO_UPDATE_C6_FW CONFIG_APP_HOSTED_AUTO_UPDATE_C6_FW
#else
#  define APP_HOSTED_AUTO_UPDATE_C6_FW 1
#endif

/** @brief Expert override for a bundled C6 image that differs from the host ESP-Hosted stack version. */
#define APP_HOSTED_ALLOW_BUNDLED_C6_VERSION_MISMATCH 0

/** @def APP_HA_WS_URL
 * @brief Compile-time Home Assistant WebSocket URL default; the placeholder is ignored.
 */
#ifdef CONFIG_APP_HA_WS_URL
#  define APP_HA_WS_URL CONFIG_APP_HA_WS_URL
#else
#  define APP_HA_WS_URL "YOUR_HA_WS_URL"
#endif

/** @def APP_HA_ACCESS_TOKEN
 * @brief Compile-time Home Assistant token default; the placeholder is ignored.
 */
#ifdef CONFIG_APP_HA_ACCESS_TOKEN
#  define APP_HA_ACCESS_TOKEN CONFIG_APP_HA_ACCESS_TOKEN
#else
#  define APP_HA_ACCESS_TOKEN "YOUR_HA_ACCESS_TOKEN"
#endif

/** @def APP_HA_FETCH_INITIAL_STATES
 * @brief Enable the initial state fetch after the Home Assistant connection is established.
 */
#ifdef CONFIG_APP_HA_FETCH_INITIAL_STATES
#  define APP_HA_FETCH_INITIAL_STATES CONFIG_APP_HA_FETCH_INITIAL_STATES
#else
#  define APP_HA_FETCH_INITIAL_STATES 1
#endif

/** @def APP_HA_SUBSCRIBE_STATE_CHANGED
 * @brief Enable state-change event subscriptions on the Home Assistant connection.
 */
#ifdef CONFIG_APP_HA_SUBSCRIBE_STATE_CHANGED
#  define APP_HA_SUBSCRIBE_STATE_CHANGED CONFIG_APP_HA_SUBSCRIBE_STATE_CHANGED
#else
#  define APP_HA_SUBSCRIBE_STATE_CHANGED 1
#endif

/** @def APP_HA_PING_INTERVAL_MS
 * @brief Requested WebSocket ping interval in milliseconds, bounded by the client minimum.
 */
#ifdef CONFIG_APP_HA_PING_INTERVAL_MS
#  define APP_HA_PING_INTERVAL_MS CONFIG_APP_HA_PING_INTERVAL_MS
#else
#  define APP_HA_PING_INTERVAL_MS 8000
#endif

/** @def APP_NTP_SERVER
 * @brief Default SNTP server when the runtime setting is not provided.
 */
#ifdef CONFIG_APP_NTP_SERVER
#  define APP_NTP_SERVER CONFIG_APP_NTP_SERVER
#else
#  define APP_NTP_SERVER "pool.ntp.org"
#endif

/** @def APP_TIME_TZ
 * @brief Default local timezone when the runtime setting is not provided.
 */
#ifdef CONFIG_APP_TIME_TZ
#  define APP_TIME_TZ CONFIG_APP_TIME_TZ
#else
#  define APP_TIME_TZ "Europe/Berlin"
#endif
