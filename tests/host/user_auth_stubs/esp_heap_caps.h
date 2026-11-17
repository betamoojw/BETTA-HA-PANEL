/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1U
#define MALLOC_CAP_8BIT 2U
void *heap_caps_malloc(size_t size, unsigned caps);
void heap_caps_free(void *ptr);
