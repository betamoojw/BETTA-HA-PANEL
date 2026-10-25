/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi/xiaozhi_font_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *sha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    xiaozhi_font_test_paths(argv[1], 3, sha);
    xiaozhi_font_store_init(); assert(!xiaozhi_font_available());
    assert(!xiaozhi_font_append(1, "a", 1));
    assert(!xiaozhi_font_append(0, "a", 32769));
    assert(!xiaozhi_font_append(3, "a", 1));
    assert(xiaozhi_font_append(0, "a", 1));
    assert(xiaozhi_font_pending_bytes() == 1);
    assert(!xiaozhi_font_append(2, "c", 1));
    assert(!xiaozhi_font_available());
    /* A restart can abandon only the incomplete private resource. */
    xiaozhi_font_store_init(); assert(xiaozhi_font_received() == 0);
    assert(!xiaozhi_font_append(0, "abd", 3));
    assert(!xiaozhi_font_available() && !xiaozhi_font_pending_bytes());
    assert(xiaozhi_font_append(0, "ab", 2));
    assert(xiaozhi_font_append(2, "c", 1));
    assert(xiaozhi_font_available());
    assert(!xiaozhi_font_append(0, "xxx", 3));
    xiaozhi_font_store_init(); assert(xiaozhi_font_available());
    char value[4] = {0}; FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fread(value, 1, 3, f) == 3); fclose(f); assert(!strcmp(value, "abc"));
    /* A corrupt existing resource is never exposed or overwritten. */
    f = fopen(argv[1], "wb"); assert(f); fputs("old", f); fclose(f);
    xiaozhi_font_store_init(); assert(!xiaozhi_font_available());
    assert(!xiaozhi_font_append(0, "abc", 3));
    f = fopen(argv[1], "rb"); assert(f); assert(fread(value, 1, 3, f) == 3); fclose(f);
    assert(!strcmp(value, "old"));
    puts("Pinned font resource: bounds, interruption, checksum, persistence and preservation passed");
}
