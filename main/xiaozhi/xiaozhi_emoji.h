/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Lookup interface for generated Otto emotion assets.
 */
#pragma once
#include "lvgl.h"
/** @brief Look up a generated Otto image by protocol emotion name.
 * @param emotion NUL-terminated name, or NULL to select neutral.
 * @return Borrowed static image descriptor; the caller must not free it.
 */
const lv_image_dsc_t *xiaozhi_otto_asset(const char *emotion);
