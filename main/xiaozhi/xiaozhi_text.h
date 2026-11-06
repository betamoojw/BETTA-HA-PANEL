/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#include <string.h>

/* Preserve complete code points when bounding a valid UTF-8 JSON string.
 * This is display truncation, not validation of arbitrary byte streams. */
static inline void xiaozhi_text_copy(char *destination, size_t capacity, const char *source)
{
    if (!capacity) return;
    size_t length = 0;
    while (length + 1 < capacity && source[length]) ++length;
    if (source[length])
        while (length && ((unsigned char)source[length] & 0xc0) == 0x80) --length;
    memcpy(destination, source, length);
    destination[length] = '\0';
}
