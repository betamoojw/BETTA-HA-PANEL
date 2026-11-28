/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Pure entity and service policies for optional room-card fields and controls.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @brief Optional reading or direct control to show on the room card. */
typedef enum
{
    ROOM_EXTRA_DEFAULT = 0,  /**< No optional extra. */
    ROOM_EXTRA_HUMIDITY,     /**< Humidity reading. */
    ROOM_EXTRA_COVER,        /**< Cover control. */
    ROOM_EXTRA_MEDIA_PLAYER, /**< Media player control. */
} room_extra_mode_t;

/** @brief Parse a stored mode name; NULL and empty mean the default mode.
 * @return False for an unrecognized name, setting a supplied output to the default.
 */
bool room_extra_mode_parse(const char *text, room_extra_mode_t *out);
/** @return The static persisted mode name, or `default` for an unknown mode. */
const char *room_extra_mode_name(room_extra_mode_t mode);

/** @return Required Home Assistant domain, or NULL when the mode has no domain requirement. */
const char *room_extra_mode_domain(room_extra_mode_t mode);
/** @brief Check the required domain prefix and a nonempty object ID, not full ID syntax. */
bool room_extra_entity_fits(room_extra_mode_t mode, const char *entity_id);

/** @brief Action offered by a room-card control. */
typedef enum
{
    ROOM_ACTION_NONE = 0,    /**< No usable action for the entity state or capabilities. */
    ROOM_ACTION_COVER_OPEN,  /**< Open the cover. */
    ROOM_ACTION_COVER_CLOSE, /**< Close the cover. */
    ROOM_ACTION_COVER_STOP,  /**< Stop cover movement. */
    ROOM_ACTION_MEDIA_PLAY,  /**< Start playback. */
    ROOM_ACTION_MEDIA_PAUSE, /**< Pause playback. */
} room_action_t;

#define ROOM_COVER_FEAT_STOP  8U     /* CoverEntityFeature.STOP */
#define ROOM_MEDIA_FEAT_PAUSE 1U     /* MediaPlayerEntityFeature.PAUSE */
#define ROOM_MEDIA_FEAT_PLAY  16384U /* MediaPlayerEntityFeature.PLAY */

/** @return Open for a closed cover, close for an open one, or Stop for movement when supported.
 *
 * Unknown or unavailable states do nothing. Unknown features allow Stop.
 */
room_action_t room_extra_cover_action(const char *state, uint32_t features, bool features_known);

/** @return The up action: open, or stop if already opening and Stop is supported.
 *
 * A cover reported open can go up only with a known position below 100%.
 */
room_action_t room_extra_cover_up(const char *state, int position, uint32_t features, bool features_known);
/** @return The down action: close, or stop if already closing and Stop is supported.
 *
 * A closed or unavailable cover cannot go down. Position is not needed.
 */
room_action_t room_extra_cover_down(const char *state, int position, uint32_t features, bool features_known);

/** @return True when the group has a usable member and at least one is on. */
bool room_group_is_on(int usable, int on);
/** @return `turn_off` when any member is on, `turn_on` otherwise, or NULL with no usable member. */
const char *room_group_toggle_service(int usable, int on);
/** @brief Keep the first move action; otherwise prefer Stop over no group action. */
room_action_t room_cover_group_merge(room_action_t group, room_action_t next);

/** @return Pause while playing, or Play when paused, idle, on, or in standby, if supported.
 *
 * Unknown features allow both actions; an unknown or unavailable state allows neither.
 */
room_action_t room_extra_media_action(const char *state, uint32_t features, bool features_known);

/** @return The static service domain for an action, or NULL if none. */
const char *room_action_domain(room_action_t action);
/** @return The static service name for an action, or NULL if none. */
const char *room_action_service(room_action_t action);
