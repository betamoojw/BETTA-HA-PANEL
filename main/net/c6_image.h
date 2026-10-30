/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Owns no persistent state. On success the caller releases *data with heap_caps_free. */
esp_err_t c6_image_unpack(const uint8_t *archive, size_t archive_size, uint8_t **data, size_t *size);
