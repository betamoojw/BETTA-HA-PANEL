/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "camera/camera_config.h"

#include "cJSON.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void camera_config_defaults(camera_settings_t *out)
{
    memset(out, 0, sizeof(*out));
    out->local.jpeg_quality = 55;
    out->local.image =
        (camera_image_config_t) { .contrast = 128, .saturation = 128, .wb_red = 100, .wb_blue = 100, .sharpen = 100, .denoise = 100 };
    out->local.motion = (camera_motion_config_t) { .threshold = 8, .cooldown_ms = 1000, .start_delay_ms = 2000, .ignore_lighting = true };
}

static bool number(const cJSON *o, const char *key, int *dst, int lo, int hi)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!v)
        return true;
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < lo || v->valuedouble > hi ||
        floor(v->valuedouble) != v->valuedouble)
        return false;
    *dst = v->valueint;
    return true;
}
static bool boolean(const cJSON *o, const char *key, bool *dst)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!v)
        return true;
    if (!cJSON_IsBool(v))
        return false;
    *dst = cJSON_IsTrue(v);
    return true;
}
static bool string(const cJSON *o, const char *key, char *dst, size_t cap)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!v)
        return true;
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap)
        return false;
    for (const unsigned char *p = (const unsigned char *) v->valuestring; *p; p++)
        if (*p < 32 || *p == 127)
            return false;
    snprintf(dst, cap, "%s", v->valuestring);
    return true;
}
bool camera_source_id_valid(const char *id)
{
    if (!id || !*id || strlen(id) >= CAMERA_SOURCE_ID_LEN || !strcmp(id, "local"))
        return false;
    for (; *id; id++)
        if (!((*id >= 'a' && *id <= 'z') || (*id >= '0' && *id <= '9') || *id == '_' || *id == '-'))
            return false;
    return true;
}
bool camera_source_url_valid(const char *url)
{
    if (!url)
        return false;
    const char *host = !strncmp(url, "http://", 7) ? url + 7 : !strncmp(url, "https://", 8) ? url + 8 : NULL;
    if (!host || !*host || *host == '/' || *host == '?' || *host == '#')
        return false;
    for (const unsigned char *p = (const unsigned char *) url; *p; p++)
        if (*p <= 32 || *p == 127 || *p == '\\' || *p == '#')
            return false;
    /* Credentials have separate fields; never accept URL userinfo. */
    const char *end = strpbrk(host, "/?");
    const char *at = strchr(host, '@');
    return !at || (end && at > end);
}
static bool image_parse(const cJSON *o, camera_image_config_t *c)
{
    return !o || (cJSON_IsObject(o) && number(o, "brightness", &c->brightness, -128, 127) && number(o, "contrast", &c->contrast, 0, 255) &&
                  number(o, "saturation", &c->saturation, 0, 255) && number(o, "hue", &c->hue, 0, 360) &&
                  number(o, "wb_red", &c->wb_red, 25, 400) && number(o, "wb_blue", &c->wb_blue, 25, 400) &&
                  number(o, "sharpen", &c->sharpen, 25, 300) && number(o, "denoise", &c->denoise, 25, 200) &&
                  number(o, "tone_shadows", &c->tone_shadows, -100, 100) && number(o, "tone_highlights", &c->tone_highlights, -100, 100));
}
static bool motion_parse(const cJSON *o, camera_motion_config_t *c)
{
    if (!o)
        return true;
    if (!cJSON_IsObject(o) || !boolean(o, "enabled", &c->enabled) || !boolean(o, "ignore_lighting", &c->ignore_lighting) ||
        !number(o, "threshold", &c->threshold, 1, 64) || !number(o, "min_area", &c->min_area, 0, 100) ||
        !number(o, "min_duration_ms", &c->min_duration_ms, 0, 1000) || !number(o, "cooldown_ms", &c->cooldown_ms, 0, 30000) ||
        !number(o, "start_delay_ms", &c->start_delay_ms, 0, 10000))
        return false;
    const cJSON *zones = cJSON_GetObjectItemCaseSensitive(o, "zones");
    if (!zones)
        return true;
    if (!cJSON_IsArray(zones) || cJSON_GetArraySize(zones) > CAMERA_MAX_ZONES)
        return false;
    c->zone_count = 0;
    memset(c->zones, 0, sizeof(c->zones));
    const cJSON *z;
    cJSON_ArrayForEach(z, zones)
    {
        camera_zone_t *dst = &c->zones[c->zone_count++];
        if (!cJSON_IsObject(z) || !number(z, "x", &dst->x, 0, 99) || !number(z, "y", &dst->y, 0, 99) || !number(z, "w", &dst->w, 1, 100) ||
            !number(z, "h", &dst->h, 1, 100) || dst->w < 1 || dst->h < 1 || dst->x + dst->w > 100 || dst->y + dst->h > 100)
            return false;
    }
    return true;
}
static bool sources_parse(const cJSON *array, camera_settings_t *out)
{
    if (!array)
        return true;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) > CAMERA_MAX_SOURCES)
        return false;
    camera_source_t next[CAMERA_MAX_SOURCES] = { 0 };
    int count = 0;
    const cJSON *o;
    cJSON_ArrayForEach(o, array)
    {
        camera_source_t *s = &next[count];
        s->refresh_ms = 10000;
        if (!cJSON_IsObject(o) || !string(o, "id", s->id, sizeof(s->id)) || !camera_source_id_valid(s->id))
            return false;
        for (int i = 0; i < count; i++)
            if (!strcmp(next[i].id, s->id))
                return false;
        for (int i = 0; i < out->source_count; i++)
            if (!strcmp(out->sources[i].id, s->id))
                *s = out->sources[i];
        const cJSON *url = cJSON_GetObjectItemCaseSensitive(o, "snapshot_url");
        const cJSON *user = cJSON_GetObjectItemCaseSensitive(o, "username");
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(o, "source");
        if ((cJSON_IsString(url) && strcmp(url->valuestring, s->snapshot_url)) ||
            (cJSON_IsString(user) && strcmp(user->valuestring, s->username)) ||
            (cJSON_IsString(kind) && strcmp(kind->valuestring, s->source)))
            s->password[0] = 0;
        if (!string(o, "name", s->name, sizeof(s->name)) || !string(o, "source", s->source, sizeof(s->source)) ||
            !string(o, "entity_id", s->entity_id, sizeof(s->entity_id)) ||
            !string(o, "snapshot_url", s->snapshot_url, sizeof(s->snapshot_url)) ||
            !string(o, "username", s->username, sizeof(s->username)) || !string(o, "password", s->password, sizeof(s->password)) ||
            !boolean(o, "enabled", &s->enabled) || !number(o, "refresh_ms", &s->refresh_ms, 1000, 3600000))
            return false;
        if (!strcmp(s->source, "http"))
        {
            if (!camera_source_url_valid(s->snapshot_url))
                return false;
        }
        else if (!strcmp(s->source, "ha"))
        {
            if (strncmp(s->entity_id, "camera.", 7) || !s->entity_id[7])
                return false;
            for (const char *p = s->entity_id + 7; *p; p++)
                if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_'))
                    return false;
        }
        else
            return false;
        count++;
    }
    memcpy(out->sources, next, sizeof(next));
    out->source_count = count;
    return true;
}
bool camera_config_parse(const cJSON *root, camera_settings_t *out)
{
    if (!cJSON_IsObject(root) || !out)
        return false;
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, "local");
    camera_config_t *c = &out->local;
    if (o &&
        (!cJSON_IsObject(o) || !boolean(o, "enabled", &c->enabled) || !boolean(o, "stream_enabled", &c->stream_enabled) ||
         !boolean(o, "hflip", &c->hflip) || !boolean(o, "vflip", &c->vflip) || !number(o, "resolution", &c->resolution, 0, 1) ||
         !number(o, "jpeg_quality", &c->jpeg_quality, 10, 95) || !image_parse(cJSON_GetObjectItemCaseSensitive(o, "image"), &c->image) ||
         !motion_parse(cJSON_GetObjectItemCaseSensitive(o, "motion"), &c->motion)))
        return false;
    return sources_parse(cJSON_GetObjectItemCaseSensitive(root, "sources"), out);
}
cJSON *camera_config_json(const camera_settings_t *config, bool secrets)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    cJSON *local = cJSON_AddObjectToObject(root, "local"), *image = cJSON_AddObjectToObject(local, "image"),
          *motion = cJSON_AddObjectToObject(local, "motion"), *zones = cJSON_AddArrayToObject(motion, "zones"),
          *sources = cJSON_AddArrayToObject(root, "sources");
    if (!local || !image || !motion || !zones || !sources)
        goto fail;
#define NUM(o, s, k)                                 \
    do                                               \
    {                                                \
        if (!cJSON_AddNumberToObject(o, #k, (s)->k)) \
            goto fail;                               \
    } while (0)
#define BOOL(o, s, k)                              \
    do                                             \
    {                                              \
        if (!cJSON_AddBoolToObject(o, #k, (s)->k)) \
            goto fail;                             \
    } while (0)
#define STR(o, s, k)                                 \
    do                                               \
    {                                                \
        if (!cJSON_AddStringToObject(o, #k, (s)->k)) \
            goto fail;                               \
    } while (0)
    const camera_config_t *c = &config->local;
    BOOL(local, c, enabled);
    BOOL(local, c, stream_enabled);
    BOOL(local, c, hflip);
    BOOL(local, c, vflip);
    NUM(local, c, resolution);
    NUM(local, c, jpeg_quality);
    const camera_image_config_t *im = &c->image;
    NUM(image, im, brightness);
    NUM(image, im, contrast);
    NUM(image, im, saturation);
    NUM(image, im, hue);
    NUM(image, im, wb_red);
    NUM(image, im, wb_blue);
    NUM(image, im, sharpen);
    NUM(image, im, denoise);
    NUM(image, im, tone_shadows);
    NUM(image, im, tone_highlights);
    const camera_motion_config_t *m = &c->motion;
    BOOL(motion, m, enabled);
    BOOL(motion, m, ignore_lighting);
    NUM(motion, m, threshold);
    NUM(motion, m, min_area);
    NUM(motion, m, min_duration_ms);
    NUM(motion, m, cooldown_ms);
    NUM(motion, m, start_delay_ms);
    for (int i = 0; i < m->zone_count; i++)
    {
        cJSON *z = cJSON_CreateObject();
        if (!z || !cJSON_AddItemToArray(zones, z))
        {
            cJSON_Delete(z);
            goto fail;
        }
        const camera_zone_t *v = &m->zones[i];
        NUM(z, v, x);
        NUM(z, v, y);
        NUM(z, v, w);
        NUM(z, v, h);
    }
    for (int i = 0; i < config->source_count; i++)
    {
        cJSON *o = cJSON_CreateObject();
        if (!o || !cJSON_AddItemToArray(sources, o))
        {
            cJSON_Delete(o);
            goto fail;
        }
        const camera_source_t *s = &config->sources[i];
        STR(o, s, id);
        STR(o, s, name);
        STR(o, s, source);
        STR(o, s, entity_id);
        STR(o, s, snapshot_url);
        STR(o, s, username);
        if (secrets)
        {
            STR(o, s, password);
        }
        else if (!cJSON_AddBoolToObject(o, "password_set", s->password[0] != 0))
            goto fail;
        BOOL(o, s, enabled);
        NUM(o, s, refresh_ms);
    }
    return root;
fail:
    cJSON_Delete(root);
    return NULL;
#undef NUM
#undef BOOL
#undef STR
}
