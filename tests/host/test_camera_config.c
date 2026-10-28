/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "cJSON.h"
#include "camera/camera_config.h"
#include "camera/camera_image.h"
#include "camera/camera_motion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(c)                                                     \
    do                                                               \
    {                                                                \
        checks++;                                                    \
        if (!(c))                                                    \
        {                                                            \
            fprintf(stderr, "Camera config %d: %s\n", __LINE__, #c); \
            exit(1);                                                 \
        }                                                            \
    } while (0)
static bool parse(camera_settings_t *s, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    camera_settings_t candidate = *s;
    bool ok = camera_config_parse(root, &candidate);
    cJSON_Delete(root);
    if (ok)
        *s = candidate;
    return ok;
}
int main(void)
{
    camera_settings_t s;
    camera_config_defaults(&s);
    CHECK(!s.local.enabled && !s.local.stream_enabled && !s.local.motion.enabled && s.source_count == 0);
    CHECK(s.local.jpeg_quality == 55 && s.local.image.contrast == 128 && s.local.motion.threshold == 8);
    const char *bad[] = { "[]",
                          "{\"local\":true}",
                          "{\"local\":{\"enabled\":1}}",
                          "{\"local\":{\"jpeg_quality\":96}}",
                          "{\"local\":{\"jpeg_quality\":50.5}}",
                          "{\"local\":{\"image\":{\"wb_red\":401}}}",
                          "{\"local\":{\"motion\":{\"zones\":[{\"x\":99,\"y\":0,\"w\":2,\"h\":20}]}}}",
                          "{\"local\":{\"motion\":{\"zones\":[{}]}}}",
                          "{\"sources\":[{\"id\":\"local\"}]}",
                          "{\"sources\":[{\"id\":\"bad id\"}]}",
                          "{\"sources\":[{\"id\":\"a\",\"source\":\"http\",\"snapshot_url\":\"http://u:p@example/a\"}]}" };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        camera_settings_t before = s;
        CHECK(!parse(&s, bad[i]));
        CHECK(!memcmp(&s, &before, sizeof(s)));
    }
    CHECK(parse(&s,
                "{\"local\":{\"enabled\":true,\"stream_enabled\":true,\"image\":{\"brightness\":12},\"motion\":{\"enabled\":true,\"zones\":"
                "[{\"x\":0,\"y\":0,\"w\":50,\"h\":100}]}},\"sources\":[{\"id\":\"door\",\"source\":\"http\",\"snapshot_url\":\"https://"
                "camera.example/picture\",\"username\":\"user\",\"password\":\"synthetic-secret\",\"enabled\":true}]}"));
    CHECK(parse(&s, "{\"local\":{\"enabled\":false}}"));
    CHECK(!s.local.enabled && s.local.stream_enabled && s.local.image.brightness == 12);
    cJSON *json = camera_config_json(&s, false);
    char *text = cJSON_PrintUnformatted(json);
    CHECK(!strstr(text, "synthetic-secret") && strstr(text, "password_set"));
    CHECK(parse(&s, text));
    CHECK(!strcmp(s.sources[0].password, "synthetic-secret"));
    free(text);
    cJSON_Delete(json);
    json = camera_config_json(&s, true);
    camera_settings_t restored;
    camera_config_defaults(&restored);
    CHECK(camera_config_parse(json, &restored));
    CHECK(!memcmp(&s, &restored, sizeof(s)));
    cJSON_Delete(json);
    CHECK(parse(&s, "{\"sources\":[{\"id\":\"door\",\"snapshot_url\":\"https://other.example/picture\"}]}"));
    CHECK(!s.sources[0].password[0]);
    CHECK(!camera_source_url_valid("file:///a"));
    CHECK(!camera_source_url_valid("http://"));
    CHECK(!camera_source_url_valid("http://a/\r\nb"));
    CHECK(!camera_source_url_valid("http://a\\b/x"));
    CHECK(camera_source_url_valid("http://192.0.2.1:8080/image.jpg?q=1"));
    CHECK(camera_source_url_valid("https://[2001:db8::1]/picture"));
    CHECK(!parse(&s, "{\"sources\":[{\"id\":\"a\",\"source\":\"ha\",\"entity_id\":\"camera.a?token=x\"}]}"));
    CHECK(!parse(&s,
                 "{\"sources\":[{\"id\":\"a\",\"source\":\"ha\",\"entity_id\":\"camera.a\"},{\"id\":\"a\",\"source\":\"ha\",\"entity_id\":"
                 "\"camera.b\"}]}"));

    camera_motion_state_t state = { 0 };
    camera_config_defaults(&s);
    camera_motion_config_t *m = &s.local.motion;
    m->start_delay_ms = 100;
    m->min_duration_ms = 100;
    m->cooldown_ms = 1000;
    uint8_t grid[256] = { 0 };
    camera_motion_reset(&state, 0);
    CHECK(!camera_motion_sample(&state, m, grid, 0));
    memset(grid, 100, 64);
    CHECK(!camera_motion_sample(&state, m, grid, 200));
    memset(grid, 0, 64);
    CHECK(camera_motion_sample(&state, m, grid, 400));
    CHECK(state.status.trigger_count == 1);
    memset(grid, 100, 64);
    CHECK(!camera_motion_sample(&state, m, grid, 600));
    memset(grid, 0, 64);
    CHECK(camera_motion_sample(&state, m, grid, 1500));
    CHECK(state.status.trigger_count == 2);
    memset(grid, 100, sizeof(grid));
    CHECK(!camera_motion_sample(&state, m, grid, 2000));
    CHECK(state.status.ignored_lighting);
    camera_motion_reset(&state, 3000);
    m->start_delay_ms = 0;
    m->min_duration_ms = 0;
    m->cooldown_ms = 0;
    m->zone_count = 2;
    m->zones[0] = (camera_zone_t) { 0, 0, 50, 100 };
    m->zones[1] = m->zones[0];
    memset(grid, 0, sizeof(grid));
    CHECK(!camera_motion_sample(&state, m, grid, 3000));
    for (int y = 0; y < 16; y++)
        for (int x = 8; x < 16; x++)
            grid[y * 16 + x] = 100;
    CHECK(!camera_motion_sample(&state, m, grid, 3100));
    CHECK(state.status.changed_pct == 0);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 8; x++)
            grid[y * 16 + x] = 100;
    CHECK(camera_motion_sample(&state, m, grid, 3200));
    CHECK(state.status.changed_pct == 100);
    CHECK(state.status.zone_changed_pct[0] == 100 && state.status.zone_changed_pct[1] == 100);
    CHECK(!camera_motion_sample(&state, m, grid, 9000));
    CHECK(!state.status.valid);

    camera_config_defaults(&s);
    uint16_t src[16] = { 0xf800, 0x07e0, 0x001f, 0xffff, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, dst[16];
    CHECK(camera_image_render(src, 4, 4, dst, sizeof(dst), &s.local, 0));
    CHECK(!memcmp(src, dst, sizeof(src)));
    s.local.hflip = true;
    CHECK(camera_image_render(src, 4, 4, dst, sizeof(dst), &s.local, 0));
    CHECK(dst[0] == src[3] && dst[15] == src[12]);
    s.local.vflip = true;
    CHECK(camera_image_render(src, 4, 4, dst, sizeof(dst), &s.local, 0));
    CHECK(dst[0] == src[15]);
    s.local.resolution = 1;
    CHECK(!camera_image_render(src, 4, 4, dst, 7, &s.local, 0));
    CHECK(camera_image_render(src, 4, 4, dst, 8, &s.local, 0));
    CHECK(dst[0] == src[10]);
    s.local.image.brightness = 127;
    CHECK(camera_image_render(src, 4, 4, dst, sizeof(dst), &s.local, 0));
    CHECK(dst[0] != src[10]);
    camera_config_defaults(&s);
    const uint16_t rectangle[8] = {1,2,3,4,5,6,7,8};
    const uint16_t poses[4][8] = {{1,2,3,4,5,6,7,8}, {5,1,6,2,7,3,8,4},
                                 {8,7,6,5,4,3,2,1}, {4,8,3,7,2,6,1,5}};
    for (int turn=0; turn<4; turn++) {
        int width=0,height=0;
        CHECK(camera_image_size(4,2,0,turn*90,&width,&height));
        CHECK(width==(turn%2?2:4) && height==(turn%2?4:2));
        for (int flip=0;flip<4;flip++) {
            s.local.hflip=flip&1;s.local.vflip=flip&2;
            CHECK(camera_image_render(rectangle,4,2,dst,sizeof(dst),&s.local,turn*90));
            for (int i=0;i<8;i++) {
                int pos=poses[turn][i]-1,x=pos%4,y=pos/4;
                CHECK(dst[i]==rectangle[(flip&2?1-y:y)*4+(flip&1?3-x:x)]);
            }
        }
        uint16_t large[32];for(int i=0;i<32;i++)large[i]=(uint16_t)(i+1);
        s.local.hflip=false;s.local.vflip=false;s.local.resolution=1;
        CHECK(camera_image_render(large,8,4,dst,16,&s.local,turn*90));
        for(int i=0;i<8;i++){int pos=poses[turn][i]-1;CHECK(dst[i]==large[(pos/4)*16+(pos%4)*2]);}
        s.local.resolution=0;
    }
    int width,height;
    CHECK(!camera_image_size(800,640,0,45,&width,&height));
    CHECK(!camera_image_render(rectangle,4,2,dst,15,&s.local,90));
    uint16_t adjusted[16],rotated[16],direct[16];
    s.local.image.brightness=25;s.local.image.sharpen=150;
    CHECK(camera_image_render(src,4,4,adjusted,sizeof(adjusted),&s.local,0));
    for(int turn=0;turn<4;turn++) {
        CHECK(camera_image_render(src,4,4,direct,sizeof(direct),&s.local,turn*90));
        camera_settings_t neutral;camera_config_defaults(&neutral);
        CHECK(camera_image_render(adjusted,4,4,rotated,sizeof(rotated),&neutral.local,turn*90));
        CHECK(!memcmp(direct,rotated,sizeof(direct)));
    }
    printf("Camera configuration, motion and image: %u checks passed\n", checks);
}
