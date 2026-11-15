/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
unsigned test_i18n_allocations, test_i18n_live, test_i18n_fail_at;
void *heap_caps_malloc(size_t size, uint32_t caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (++test_i18n_allocations == test_i18n_fail_at) return NULL;
    void *memory = malloc(size);
    if (memory) test_i18n_live++;
    return memory;
}
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps)
{
    void *memory = heap_caps_malloc(count * size, caps);
    if (memory) memset(memory, 0, count * size);
    return memory;
}
void heap_caps_free(void *memory)
{
    if (memory) { assert(test_i18n_live > 0); test_i18n_live--; free(memory); }
}
