/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * POST /api/camera/test: force presence sampling for a short while even with the display
 * active, so Settings > Display > Camera can show a live "presence detected" status while the
 * sensitivity is being tried out (ambient_camera.c normally only samples once the display is
 * already dimmed, to wake it). No body. Unsupported variants return an error.
 */
#include "esp_http_server.h"

#include "drivers/ambient_camera.h"

#define API_CAMERA_TEST_DURATION_MS 30000

esp_err_t api_camera_test_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    ambient_camera_reading_t reading = {0};
    (void)ambient_camera_get(&reading);
    if (!ambient_camera_available() || (!reading.ready && reading.failed)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"Camera unavailable\"}");
    }
    ambient_camera_start_test(API_CAMERA_TEST_DURATION_MS);
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}
