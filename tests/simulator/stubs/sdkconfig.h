/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
#pragma once
/* The selected panel variant is passed as a compiler definition by CMake. */
#ifdef _WIN32
#include <string.h>
/* MinGW's CRT lacks the BSD string helper provided by ESP-IDF. */
static inline size_t betta_sim_strlcpy(char *dest, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t copy = len < size - 1 ? len : size - 1;
        memcpy(dest, src, copy);
        dest[copy] = '\0';
    }
    return len;
}
#define strlcpy betta_sim_strlcpy
#endif
