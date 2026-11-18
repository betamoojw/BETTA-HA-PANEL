/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stddef.h>

#define MALLOC_CAP_SPIRAM 1U
#define MALLOC_CAP_8BIT 2U
#define MALLOC_CAP_INTERNAL 4U

size_t heap_caps_get_free_size(unsigned caps);
size_t heap_caps_get_largest_free_block(unsigned caps);
void *heap_caps_calloc(size_t count, size_t size, unsigned caps);
void *heap_caps_malloc(size_t size, unsigned caps);
void heap_caps_free(void *ptr);
