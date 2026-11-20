/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "settings/user_auth.h"
#include <stddef.h>
#include <string.h>

static bool path_is(const char *uri, const char *path)
{
    const size_t n = strlen(path);
    return strncmp(uri, path, n) == 0 && (uri[n] == '\0' || uri[n] == '?');
}

user_role_t user_auth_required_role(const char *uri, const char *method)
{
    if (!uri || !method) return USER_ROLE_ADMIN;
    if (strcmp(method, "GET") == 0) {
        /* Public static assets contain no device data. Only registered routes reach this policy. */
        if (strncmp(uri, "/api/", 5) != 0 || strcmp(uri, "/api/version") == 0 ||
            path_is(uri, "/api/auth/session") || path_is(uri, "/api/license/status")) return USER_ROLE_NONE;
        const char *viewer[] = {"/api/license", "/api/layout", "/api/state", "/api/entities", "/api/settings",
            "/api/i18n/languages", "/api/i18n/effective", "/api/themes", "/api/themes/active", "/api/themes/get",
            "/api/screenshot.jpg", "/api/screenshot.png", "/api/screenshot.bmp", "/api/time"};
        for (unsigned i = 0; i < sizeof(viewer) / sizeof(*viewer); ++i)
            if (path_is(uri, viewer[i])) return USER_ROLE_VIEWER;
        const char *installer[] = {"/api/wifi/scan", "/api/ha/light_entities", "/api/ha/energy",
            "/api/ha/evcc", "/api/ha/printers", "/api/display/test", "/api/knx", "/api/knx/objects", "/api/camera"};
        for (unsigned i = 0; i < sizeof(installer) / sizeof(*installer); ++i)
            if (path_is(uri, installer[i])) return USER_ROLE_INSTALLER;
    }
    if (strcmp(method, "POST") == 0) {
        if (path_is(uri, "/api/auth/login") || path_is(uri, "/api/auth/logout")) return USER_ROLE_NONE;
        if (path_is(uri, "/api/auth/password") || path_is(uri, "/api/display/remote/session")) return USER_ROLE_VIEWER;
        if (path_is(uri, "/api/navigate") || path_is(uri, "/api/ui/tap") ||
            path_is(uri, "/api/notifications")) return USER_ROLE_OPERATOR;
        const char *installer[] = {"/api/display/test", "/api/sound/test", "/api/camera/test"};
        for (unsigned i = 0; i < sizeof(installer) / sizeof(*installer); ++i)
            if (path_is(uri, installer[i])) return USER_ROLE_INSTALLER;
    }
    if (strcmp(method, "PUT") == 0) {
        if (path_is(uri, "/api/layout") || path_is(uri, "/api/themes/active") ||
            path_is(uri, "/api/themes/custom") || path_is(uri, "/api/i18n/custom")) return USER_ROLE_INSTALLER;
    }
    if (strcmp(method, "DELETE") == 0 && (path_is(uri, "/api/themes/custom") ||
        path_is(uri, "/api/ha/light_entities"))) return USER_ROLE_INSTALLER;
    /* Accounts, credentials, OTA, reset, logs, dumps and unclassified extensions require an admin. */
    return USER_ROLE_ADMIN;
}
