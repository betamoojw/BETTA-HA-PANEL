/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define XIAOZHI_WAKE_PHRASE_SIZE 96
#define XIAOZHI_WAKE_DEFAULT_PHRASE "ni hao xiao zhi"
/* Canonical lowercase pinyin, 3-12 syllables; semantic validation is by MultiNet. */
static inline bool xiaozhi_wake_phrase_valid(const char *s)
{
    if (!s) return false;
    unsigned len = 0, syllables = 1, letters = 0;
    for (; s[len]; ++len) {
        if (len >= XIAOZHI_WAKE_PHRASE_SIZE - 1) return false;
        if (s[len] == ' ') {
            if (!letters) return false;
            letters = 0; ++syllables;
        } else if (s[len] >= 'a' && s[len] <= 'z') {
            if (++letters > 6) return false;
        } else return false;
    }
    return letters && syllables >= 3 && syllables <= 12;
}
bool xiaozhi_custom_wake_init(const char *phrase);
int xiaozhi_custom_wake_chunk(void);
bool xiaozhi_custom_wake_detect(int16_t *mono);
void xiaozhi_custom_wake_free(void);
