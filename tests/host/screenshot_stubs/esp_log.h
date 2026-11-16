/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Host stub for tests/host/test_screenshot.c: just enough of ESP-IDF, HTTP server and LVGL for main/api/api_screenshot.c.
 */
#pragma once
#include <stdio.h>
void test_screenshot_log(const char *fmt, ...) __attribute__((format(gnu_printf, 1, 2)));
#define ESP_LOGI(tag, fmt, ...) do { (void)(tag); test_screenshot_log(fmt, ##__VA_ARGS__); } while (0)
#define ESP_LOGW(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
