/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Pure reboot history model for RTC breadcrumbs and the persistent ring.
 *
 * Following SonosESP's reboot log, the ESP adapter refreshes a breadcrumb
 * without writing flash and combines it with the reset reason once per boot.
 * Do not assume an RTC breadcrumb survives a power cut.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REBOOT_LOG_SLOTS   8
#define REBOOT_CRUMB_MAGIC 0x5EB007C8u
#define REBOOT_RING_MAGIC  0x5EB0A11Bu

/** @brief Which deliberate restart was marked. Stored in NVS: append only, never renumber. */
typedef enum
{
    REBOOT_CAUSE_NONE = 0,       /**< No deliberate restart cause was recorded. */
    REBOOT_CAUSE_SETTINGS,       /**< The editor requested Save + Reboot. */
    REBOOT_CAUSE_OTA,            /**< A firmware update finished. */
    REBOOT_CAUSE_FACTORY_RESET,  /**< The panel performed a factory reset. */
    REBOOT_CAUSE_C6_UPDATE,      /**< The ESP32-C6 coprocessor firmware was updated. */
    REBOOT_CAUSE_NET_WATCHDOG,   /**< The link or transport watchdog exhausted recovery. */
    REBOOT_CAUSE_HTTP_WATCHDOG,  /**< The local HTTP server stopped answering with the network up. */
    REBOOT_CAUSE_UI_WATCHDOG,    /**< The stalled UI caused a deliberate abort with a core dump. */
    REBOOT_CAUSE_MANUAL,         /**< The editor requested a panel restart. */
    REBOOT_CAUSE_WIFI_RECOVERED, /**< The saved network returned while the setup access point was up. */
    REBOOT_CAUSE_COUNT           /**< Exclusive upper bound for cause IDs. */
} reboot_cause_t;

/** @brief Checksum-protected snapshot of the current run in RTC memory. */
typedef struct
{
    uint32_t magic;
    uint32_t uptime_s;          /**< Uptime in seconds at the last refresh. */
    uint32_t min_free_internal; /**< Lowest internal free heap, in bytes. */
    uint32_t largest_internal;  /**< Largest free internal block in bytes at the last refresh. */
    int64_t wall_epoch;         /**< Wall-clock seconds at the last refresh, or zero if unknown. */
    uint8_t cause;              /* a reboot_cause_t, set just before a deliberate restart */
    uint8_t reserved[3];
    uint32_t crc;
} reboot_crumb_t;

/** @brief One history entry for the run that ended, recorded on the next boot. */
typedef struct
{
    uint32_t seq;         /* boot counter of the boot that recorded the entry */
    uint8_t reset_reason; /* esp_reset_reason_t of that boot (how the previous run ended) */
    uint8_t cause;        /* reboot_cause_t, or 0 */
    uint8_t had_crumb;    /* the previous run's breadcrumb was intact */
    uint8_t reserved;
    uint32_t prev_uptime_s; /* how long the previous run lasted (0 when unknown) */
    uint32_t min_free_internal;
    uint32_t largest_internal;
    int64_t prev_wall_epoch; /* wall time of the previous run's last refresh (0 when unknown) */
} reboot_entry_t;

/** @brief Fixed-size persistent history ring with a sequence number incremented on append. */
typedef struct
{
    uint32_t magic;
    uint32_t next_seq;
    uint8_t head;  /* slot the next entry goes to */
    uint8_t count; /* entries kept, at most REBOOT_LOG_SLOTS */
    uint8_t reserved[2];
    reboot_entry_t slot[REBOOT_LOG_SLOTS];
} reboot_ring_t;

/** @brief Set the breadcrumb magic and seal the fields with CRC-32. */
void reboot_crumb_seal(reboot_crumb_t *crumb);
/** @return True when the breadcrumb has valid magic, checksum, and cause. */
bool reboot_crumb_valid(const reboot_crumb_t *crumb);

/** @brief Reinitialize the ring only when its magic or index bounds are invalid. */
void reboot_ring_init_if_invalid(reboot_ring_t *ring);
/** @brief Build an entry from this boot's reset reason and an optional previous breadcrumb.
 *
 * Ignore the breadcrumb after power-on or brownout. A retained restart tag
 * is considered only for a software reset.
 */
void reboot_entry_build(reboot_entry_t *out, uint32_t seq, uint8_t reset_reason, const reboot_crumb_t *crumb);
/** @brief Assign the next sequence number and append, overwriting the oldest entry when full. */
void reboot_ring_push(reboot_ring_t *ring, reboot_entry_t *entry);
/** @return True when an entry was copied; index zero is the newest retained entry. */
bool reboot_ring_get(const reboot_ring_t *ring, int index, reboot_entry_t *out);

/** @return A static short name for the supplied ESP reset reason. */
const char *reboot_reason_name(uint8_t esp_reset_reason);
/** @return A static short name for the supplied deliberate restart cause. */
const char *reboot_cause_name(uint8_t cause);
/** @return A static display label combining the reset reason with any deliberate cause. */
const char *reboot_entry_label(const reboot_entry_t *entry);
/** @return True for a panic, watchdog reset, or deliberate watchdog recovery restart. */
bool reboot_entry_is_failure(const reboot_entry_t *entry);
