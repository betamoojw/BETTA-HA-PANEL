/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
void discovery_test_log(const char *, const char *, ...);
#define ESP_LOGI(...) discovery_test_log(__VA_ARGS__)
