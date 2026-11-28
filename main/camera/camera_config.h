/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Camera settings schema and JSON conversion contracts.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/** @brief Opaque JSON tree type; include cJSON.h to inspect or free a tree. */
typedef struct cJSON cJSON;

#define CAMERA_MAX_ZONES       4    /**< Maximum number of rectangles in the motion policy. */
#define CAMERA_MAX_SOURCES     4    /**< Maximum number of saved sources, excluding the built-in camera. */
#define CAMERA_CONFIG_MAX_JSON 8192 /**< Maximum serialized settings size in bytes, excluding the terminating NUL. */
#define CAMERA_SOURCE_ID_LEN   24   /**< Source ID storage capacity, including the terminating NUL. */

/** @brief Rectangle in percentages of the mirrored, unrotated camera frame.
 *
 * The right and bottom edges may reach, but not exceed, 100 percent.
 */
typedef struct
{
    int x, /**< Left edge from the frame origin, 0-99 percent. */
        y, /**< Top edge from the frame origin, 0-99 percent. */
        w, /**< Width, 1-100 percent of the frame width. */
        h; /**< Height, 1-100 percent of the frame height. */
} camera_zone_t;

/** @brief Advanced motion policy applied by the capture driver. */
typedef struct
{
    bool enabled,                          /**< Select advanced rather than basic motion sensing. */
        ignore_lighting;                   /**< Suppress changes across most of the grid in the same direction. */
    int threshold,                         /**< Minimum average absolute luma change across selected cells, 1-64. */
        min_area,                          /**< Minimum percentage of selected cells that must cross the change cutoff. */
        min_duration_ms,                   /**< How long motion must remain active before a trigger, in milliseconds. */
        cooldown_ms,                       /**< Minimum time between emitted triggers, in milliseconds. */
        start_delay_ms;                    /**< Grace period after a policy reset, in milliseconds. */
    int zone_count;                        /**< Number of defined rectangles; zero uses the whole grid. */
    camera_zone_t zones[CAMERA_MAX_ZONES]; /**< Only the first `zone_count` rectangles are used. */
} camera_motion_config_t;

/** @brief Software image adjustments applied to picture output, not the sensor or motion input.
 *
 * Sharpening and denoising contribute in opposite directions to the spatial filter.
 */
typedef struct
{
    int brightness,      /**< Signed intensity offset, -128 to 127. */
        contrast,        /**< Contrast scale around mid-gray; 128 is neutral. */
        saturation,      /**< Chroma scale; 128 is neutral. */
        hue,             /**< Chroma rotation in degrees; 0 and 360 are equivalent. */
        wb_red,          /**< Red-channel white-balance gain in percent. */
        wb_blue;         /**< Blue-channel white-balance gain in percent. */
    int sharpen,         /**< Sharpening setting, with 100 neutral. */
        denoise,         /**< Smoothing setting, with 100 neutral. */
        tone_shadows,    /**< Signed correction weighted toward darker intensities; zero is neutral. */
        tone_highlights; /**< Signed correction weighted toward brighter intensities; zero is neutral. */
} camera_image_config_t;

/** @brief Built-in camera settings with separate opt-ins for pictures and MJPEG streaming.
 *
 * These output switches do not enable or disable ambient sensing.
 */
typedef struct
{
    bool enabled,                  /**< Opt in to built-in picture output. */
        stream_enabled,            /**< Allow MJPEG when built-in picture output is also enabled. */
        hflip,                     /**< Mirror along the horizontal sensor axis before rotation. */
        vflip;                     /**< Mirror along the vertical sensor axis before rotation. */
    int resolution;                /**< 0: native 800x640; 1: half-size 400x320 before rotation on LCD-X. */
    int jpeg_quality;              /**< Encoder quality from 10 through 95. */
    camera_image_config_t image;   /**< Adjustments for rendered pictures only. */
    camera_motion_config_t motion; /**< Policy for the separate luma-based motion path. */
} camera_config_t;

/** @brief Saved snapshot source keyed by a stable ID; this record can contain credentials.
 *
 * Use redacted serialization for editor reads and backups rather than exposing
 * the record directly.
 */
typedef struct
{
    char id[CAMERA_SOURCE_ID_LEN], /**< Stable layout reference, independent of list order or name. */
        name[64],                  /**< Display label, not a lookup key. */
        source[8],                 /**< `ha` for a Home Assistant entity or `http` for a direct HTTP(S) URL. */
        entity_id[128];            /**< `camera.*` entity for the HA proxy; unused by a direct source. */
    char snapshot_url[384],        /**< Direct JPEG endpoint; unused by the HA proxy. */
        username[64],              /**< Optional Basic authentication user for the direct source. */
        password[96];              /**< Basic authentication secret, omitted from redacted JSON. */
    bool enabled;                  /**< Saved opt-in; disabled records remain in the settings list. */
    int refresh_ms;                /**< Configured polling interval in milliseconds, from 1000 through 3600000. */
} camera_source_t;

/** @brief Complete local settings and bounded saved source list. */
typedef struct
{
    camera_config_t local;                       /**< Settings for the built-in camera, separate from saved sources. */
    int source_count;                            /**< Number of saved records, including disabled ones. */
    camera_source_t sources[CAMERA_MAX_SOURCES]; /**< Only the first `source_count` entries are valid. */
} camera_settings_t;

/** @brief Initialize defaults with picture output and streaming disabled and no saved sources. */
void camera_config_defaults(camera_settings_t *out);

/** @brief Apply a partial local settings update and replace the source list if supplied.
 *
 * An omitted password is retained by stable ID only when the URL, username, and source type remain unchanged.
 * Parse into a candidate copy: validation can fail after modifying some fields.
 *
 * @param root Borrowed JSON object with optional local and sources fields.
 * @param[in,out] out Candidate initialized from existing settings or defaults.
 * @return True only when the supplied settings pass validation.
 */
bool camera_config_parse(const cJSON *root, camera_settings_t *out);

/** @brief Serialize camera settings for persistence or export.
 * @param config Borrowed settings to serialize.
 * @param secrets True for local persistence; false omits passwords and emits password_set instead.
 * @return Caller-owned JSON to release with cJSON_Delete(), or NULL on allocation failure.
 */
cJSON *camera_config_json(const camera_settings_t *config, bool secrets);

/** @return True for an ID of 1-23 letters a-z, digits, underscores, or hyphens; local is reserved. */
bool camera_source_id_valid(const char *id);

/** @return True for a supported HTTP(S) URL shape without userinfo or a fragment. */
bool camera_source_url_valid(const char *url);
