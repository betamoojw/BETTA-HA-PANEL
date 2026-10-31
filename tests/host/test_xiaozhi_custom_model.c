/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi/xiaozhi_custom_model_store.h"
#include <assert.h>
#include "xiaozhi/xiaozhi_custom_wake.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(xiaozhi_wake_phrase_valid("ni hao guan jia"));
    assert(!xiaozhi_wake_phrase_valid("ni hao"));
    assert(!xiaozhi_wake_phrase_valid("ni  hao guan jia"));
    assert(!xiaozhi_wake_phrase_valid(" ni hao guan jia"));
    assert(!xiaozhi_wake_phrase_valid("ni hao guan jia "));
    assert(!xiaozhi_wake_phrase_valid("ni3 hao3 guan jia"));
    assert(!xiaozhi_wake_phrase_valid("Ni hao guan jia"));
    assert(!xiaozhi_wake_phrase_valid("ni hao toolongword"));
    assert(!xiaozhi_wake_phrase_valid("ni ni ni ni ni ni ni ni ni ni ni ni ni"));
    const char *sha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    xiaozhi_custom_model_test_paths(argv[1], 3, sha);
    xiaozhi_custom_model_store_init(); assert(!xiaozhi_custom_model_available());
    assert(!xiaozhi_custom_model_append(1, "a", 1));
    assert(!xiaozhi_custom_model_append(0, "a", 32769));
    assert(!xiaozhi_custom_model_append(3, "a", 1));
    assert(xiaozhi_custom_model_append(0, "a", 1));
    assert(xiaozhi_custom_model_pending_bytes() == 1);
    assert(!xiaozhi_custom_model_append(2, "c", 1));
    assert(!xiaozhi_custom_model_available());
    /* A restart can abandon only the incomplete private resource. */
    xiaozhi_custom_model_store_init(); assert(xiaozhi_custom_model_received() == 0);
    assert(!xiaozhi_custom_model_append(0, "abd", 3));
    assert(!xiaozhi_custom_model_available() && !xiaozhi_custom_model_pending_bytes());
    assert(xiaozhi_custom_model_append(0, "ab", 2));
    assert(xiaozhi_custom_model_append(2, "c", 1));
    assert(xiaozhi_custom_model_available());
    assert(!xiaozhi_custom_model_append(0, "xxx", 3));
    xiaozhi_custom_model_store_init(); assert(xiaozhi_custom_model_available());
    char value[4] = {0}; FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fread(value, 1, 3, f) == 3); fclose(f); assert(!strcmp(value, "abc"));
    /* A corrupt existing resource is never exposed or overwritten. */
    f = fopen(argv[1], "wb"); assert(f); fputs("old", f); fclose(f);
    xiaozhi_custom_model_store_init(); assert(!xiaozhi_custom_model_available());
    assert(!xiaozhi_custom_model_append(0, "abc", 3));
    f = fopen(argv[1], "rb"); assert(f); assert(fread(value, 1, 3, f) == 3); fclose(f);
    assert(!strcmp(value, "old"));
    puts("Pinned model resource: bounds, interruption, checksum, persistence and preservation passed");
}
