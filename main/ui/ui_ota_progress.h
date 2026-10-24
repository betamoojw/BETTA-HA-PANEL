/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

void ui_ota_progress_begin(bool downloading, size_t total_bytes);
void ui_ota_progress_set_status(const char *version);
void ui_ota_progress_update(size_t written_bytes, size_t total_bytes);
void ui_ota_progress_success(void);
void ui_ota_progress_error(void);
bool ui_ota_progress_is_active(void);
