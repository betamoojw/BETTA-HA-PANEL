/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Account/session allocation policy for memory-constrained boards.
 */
#pragma once
#include <stdlib.h>
#ifdef ESP_PLATFORM
#  include "sdkconfig.h"
#endif
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
#  include "esp_heap_caps.h"
#endif

/** @brief Allocate account storage without consuming the S3 internal network reserve.
 * @return Caller-owned allocation, or NULL; on S3 there is no internal-RAM fallback.
 */
static inline void *user_auth_memory_alloc(size_t bytes)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(bytes);
#endif
}

/** @brief Release storage using the allocator paired with user_auth_memory_alloc(). */
static inline void user_auth_memory_free(void *ptr)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    heap_caps_free(ptr);
#else
    free(ptr);
#endif
}
