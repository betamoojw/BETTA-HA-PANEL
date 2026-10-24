/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/display_geometry.h"
#include "layout/layout_orientation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int number(const cJSON *o, const char *key)
{ return cJSON_GetObjectItemCaseSensitive(o, key)->valueint; }
static cJSON *first(cJSON *root)
{ return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(
    cJSON_GetObjectItemCaseSensitive(root, "pages"), 0), "widgets"), 0); }

int main(void)
{
    const int base = display_geometry_default_rotation();
    assert(!display_geometry_set_rotation(45));
    for (int offset = 0; offset < 360; offset += 90) {
        assert(display_geometry_set_rotation((base + offset) % 360));
        bool portrait = APP_SCREEN_WIDTH != APP_SCREEN_HEIGHT && offset % 180 != 0;
        assert(display_geometry_portrait() == portrait);
        assert(APP_VIEW_WIDTH == (portrait ? APP_SCREEN_HEIGHT : APP_SCREEN_WIDTH));
        assert(APP_VIEW_HEIGHT == (portrait ? APP_SCREEN_WIDTH : APP_SCREEN_HEIGHT));
        cJSON *root = cJSON_Parse("{\"pages\":[{\"widgets\":[{\"type\":\"sensor\","
            "\"rect\":{\"x\":10,\"y\":20,\"w\":300,\"h\":180}},"
            "{\"type\":\"empty_tile\",\"rect\":{\"x\":0,\"y\":0,\"w\":400,\"h\":300}}]}]}");
        assert(root && layout_orientation_project(root, portrait));
        assert(number(root,"canvas_w") == APP_VIEW_WIDTH);
        assert(number(root,"canvas_h") >= APP_VIEW_CONTENT_HEIGHT);
        cJSON *widget = first(root), *r = cJSON_GetObjectItemCaseSensitive(widget,"rect");
        assert(number(r,"x") + number(r,"w") <= APP_VIEW_WIDTH);
        if (portrait) {
            cJSON *background = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(
                cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"pages"),0),"widgets"),1);
            cJSON *br = cJSON_GetObjectItemCaseSensitive(background,"rect");
            assert(number(br,"w") == APP_VIEW_WIDTH && number(br,"h") == APP_VIEW_CONTENT_HEIGHT);
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(r,"y"),1400);
        }
        assert(layout_orientation_canonicalize(root));
        r = cJSON_GetObjectItemCaseSensitive(widget,"rect");
        assert(number(r,"x") == 10 && number(r,"y") == 20);
        if (portrait) {
            assert(layout_orientation_project(root,true));
            assert(number(cJSON_GetObjectItemCaseSensitive(widget,"rect"),"y") == 1400);
            assert(number(root,"canvas_h") >= 1580);
        }
        cJSON_Delete(root);
    }
    printf("Orientation geometry %dx%d: four poses and layout round trips passed\n",APP_SCREEN_WIDTH,APP_SCREEN_HEIGHT);
    return 0;
}
