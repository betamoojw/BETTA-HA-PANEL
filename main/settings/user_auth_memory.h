/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
#include "esp_heap_caps.h"
#endif

/* HTTP-task-owned records may live in PSRAM. Never consume the S3 network
 * reserve as a fallback; callers must fail closed and allow a later retry. */
static inline void *user_auth_memory_alloc(size_t bytes)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(bytes);
#endif
}

static inline void user_auth_memory_free(void *ptr)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    heap_caps_free(ptr);
#else
    free(ptr);
#endif
}
