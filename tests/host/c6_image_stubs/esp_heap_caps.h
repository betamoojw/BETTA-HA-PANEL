/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_8BIT 1U
#define MALLOC_CAP_SPIRAM 2U
void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps);
void heap_caps_free(void *memory);
