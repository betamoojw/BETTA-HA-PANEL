/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *heap_caps_malloc(size_t, unsigned);
void *heap_caps_calloc(size_t, size_t, unsigned);
void *heap_caps_aligned_alloc(size_t, size_t, unsigned);
