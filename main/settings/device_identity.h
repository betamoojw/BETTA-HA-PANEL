/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Central device identity: the room the panel stands in (settings "device.room")
 * gives the device id ("betta_ha_panel_arbeitszimmer") that MQTT, Home Assistant
 * discovery and the editor header use. Without a room the id stays the
 * MAC-based "panel-XXXX".
 */
/** @file
 * @brief Shared room-based or MAC-based panel identity.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define DEVICE_ROOM_MAX_LEN 32 /**< Room name storage capacity in bytes, including the NUL. */

/** @brief Copy the configured room into module storage; NULL clears it. */
void device_identity_set_room(const char *room);

/** @return True when a nonempty room slug is available; false leaves out unchanged. */
bool device_identity_room_id(char *out, size_t out_len);

/** @brief Write the room-based display name, or the generic panel name if no room is set. */
void device_identity_display_name(char *out, size_t out_len);

/** @brief Write the effective ID, preferring the room slug and otherwise using the station MAC. */
void device_identity_effective_id(char *out, size_t out_len);
