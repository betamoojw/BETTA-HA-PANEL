/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Host stub for tests/host/test_screenshot.c.
 */
#pragma once
#include <stddef.h>
void heap_caps_free(void *ptr);
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *heap_caps_aligned_alloc(size_t alignment, size_t size, unsigned caps);
void *heap_caps_malloc(size_t size, unsigned caps);
size_t heap_caps_get_free_size(unsigned caps);
size_t heap_caps_get_largest_free_block(unsigned caps);
