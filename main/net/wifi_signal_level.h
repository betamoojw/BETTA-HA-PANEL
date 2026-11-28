/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief UI signal levels and RSSI hysteresis.
 */
#pragma once

/* Signal strength shown by the top bar's Wi-Fi symbol: 1 weak, 2 medium, 3 strong; 0 not known yet. */
#define WIFI_SIGNAL_UNKNOWN 0
#define WIFI_SIGNAL_WEAK    1
#define WIFI_SIGNAL_MEDIUM  2
#define WIFI_SIGNAL_STRONG  3

/** @brief Convert a station reading using the displayed level for hysteresis.
 *
 * Nominal thresholds are -60 dBm for strong and -70 dBm for medium. An
 * established level changes only outside its 3 dB hysteresis band.
 *
 * @param rssi_dbm Current reading in dBm.
 * @param previous_level Prior displayed level, or WIFI_SIGNAL_UNKNOWN for a fresh reading.
 * @return Weak, medium, or strong; the caller handles missing readings.
 */
int wifi_signal_level(int rssi_dbm, int previous_level);
