/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct cJSON cJSON;

#define CAMERA_MAX_ZONES       4
#define CAMERA_MAX_SOURCES     4
#define CAMERA_CONFIG_MAX_JSON 8192
#define CAMERA_SOURCE_ID_LEN   24

typedef struct
{
        int x, y, w, h;
} camera_zone_t;
typedef struct
{
        bool enabled, ignore_lighting;
        int threshold, min_area, min_duration_ms, cooldown_ms, start_delay_ms;
        int zone_count;
        camera_zone_t zones[CAMERA_MAX_ZONES];
} camera_motion_config_t;
typedef struct
{
        int brightness, contrast, saturation, hue, wb_red, wb_blue;
        int sharpen, denoise, tone_shadows, tone_highlights;
} camera_image_config_t;
typedef struct
{
        bool enabled, stream_enabled, hflip, vflip;
        int resolution; /* 0: sensor-native 800x640; 1: half 400x320 on LCD-X */
        int jpeg_quality;
        camera_image_config_t image;
        camera_motion_config_t motion;
} camera_config_t;
typedef struct
{
        char id[CAMERA_SOURCE_ID_LEN], name[64], source[8], entity_id[128];
        char snapshot_url[384], username[64], password[96];
        bool enabled;
        int refresh_ms;
} camera_source_t;
typedef struct
{
        camera_config_t local;
        int source_count;
        camera_source_t sources[CAMERA_MAX_SOURCES];
} camera_settings_t;

void camera_config_defaults(camera_settings_t *out);
/* Partial update of local settings; source arrays replace the list. Omitted
 * source passwords are retained by stable id, never by array position. */
bool camera_config_parse(const cJSON *root, camera_settings_t *out);
cJSON *camera_config_json(const camera_settings_t *config, bool secrets);
bool camera_source_id_valid(const char *id);
bool camera_source_url_valid(const char *url);
