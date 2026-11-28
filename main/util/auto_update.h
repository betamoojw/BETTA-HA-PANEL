/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Pure scheduling policy for the optional automatic firmware update.
 *
 * The scheduler supplies measured conditions and performs any install itself.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 03:00 local time, inclusive, in minutes since midnight. */
#define AUTO_UPDATE_WINDOW_START_MIN 180
/** @brief 04:00 local time, exclusive, in minutes since midnight. */
#define AUTO_UPDATE_WINDOW_END_MIN 240
/** @brief Required idle time since panel or editor activity, in seconds. */
#define AUTO_UPDATE_IDLE_MIN_S 600
/** @brief Minimum Wi-Fi RSSI in dBm when there is no usable wired connection. */
#define AUTO_UPDATE_MIN_RSSI_DBM (-75)
/* TLS download headroom in bytes, in addition to the running dashboard. */
#define AUTO_UPDATE_MIN_FREE_INTERNAL    (120U * 1024U)
#define AUTO_UPDATE_MIN_LARGEST_INTERNAL (60U * 1024U)
#define AUTO_UPDATE_MIN_FREE_PSRAM       (4U * 1024U * 1024U)

/** @brief Scheduler outcomes, including the storage outcome supplied by the caller. */
typedef enum
{
    AUTO_UPDATE_GO = 0,               /**< The measured conditions allow an attempt. */
    AUTO_UPDATE_WAIT_DISABLED,        /**< Automatic installation is disabled. */
    AUTO_UPDATE_WAIT_BUSY,            /**< Another update is in progress. */
    AUTO_UPDATE_WAIT_SAFE_MODE,       /**< This boot is using the built-in safe layout. */
    AUTO_UPDATE_WAIT_NO_UPDATE,       /**< The update check found no newer release. */
    AUTO_UPDATE_WAIT_BLOCKED_VERSION, /**< This release already rolled back. */
    AUTO_UPDATE_WAIT_TRIED_TODAY,     /**< An attempt was already recorded today. */
    AUTO_UPDATE_WAIT_NO_SHA256,       /**< The download has no digest to check. */
    AUTO_UPDATE_WAIT_NO_ROLLBACK,     /**< The bootloader cannot roll back a bad image. */
    AUTO_UPDATE_WAIT_TIME_UNKNOWN,    /**< The clock is not set. */
    AUTO_UPDATE_WAIT_OUTSIDE_WINDOW,  /**< The local time is outside the install window. */
    AUTO_UPDATE_WAIT_START_MINUTE,    /**< The panel's assigned minute has not arrived. */
    AUTO_UPDATE_WAIT_ACTIVITY,        /**< There was recent panel or editor activity. */
    AUTO_UPDATE_WAIT_WIFI,            /**< No usable wired connection or strong enough Wi-Fi link. */
    AUTO_UPDATE_WAIT_STORAGE,         /**< The scheduler could not persist its attempt state. */
    AUTO_UPDATE_WAIT_MEMORY           /**< Not enough free memory for the download. */
} auto_update_decision_t;

/** @brief Snapshot of update, activity, network, and heap conditions.
 *
 * Heap sizes are in bytes; release tags are borrowed for the decision.
 */
typedef struct
{
    bool enabled;             /* the "Install updates automatically" setting */
    bool update_available;    /* the update check found a newer release (it already follows "Include pre-releases") */
    bool sha256_known;        /* that release has a SHA-256 digest */
    bool bootloader_rollback; /* /api/version bootloader.rollback */
    bool tried_today;         /* the last attempt was on today's date */
    bool busy;                /* an OTA install is running */
    bool safe_mode;
    bool time_valid;
    int local_minute_of_day; /**< 0 through 1439 in the panel's time zone. */
    int start_offset_min;    /**< Daily start offset from auto_update_start_offset_min(). */
    int64_t idle_s;          /**< Seconds since the last touch or editor request. */
    bool wifi_connected;
    bool ethernet_connected; /**< A usable wired IP bypasses the Wi-Fi requirement. */
    int rssi_dbm;            /**< Signal strength; only checked for an active Wi-Fi link. */
    size_t free_internal;
    size_t largest_internal;
    size_t free_psram;
    const char *release_tag; /* the release found, "v0.9.0-rc4" */
    const char *blocked_tag; /* a version that was installed and rolled back, "" for none */
} auto_update_input_t;

/** @return The first unmet condition, or AUTO_UPDATE_GO. A NULL snapshot is disabled. */
auto_update_decision_t auto_update_decide(const auto_update_input_t *in);

/** @return A stable status name for logs and `/api/ota/status`, such as `wait_activity`. */
const char *auto_update_decision_name(auto_update_decision_t decision);

/** @return A deterministic minute from 0 through 59, mixed from the device and local day. */
int auto_update_start_offset_min(uint32_t device_salt, int32_t day_number);
