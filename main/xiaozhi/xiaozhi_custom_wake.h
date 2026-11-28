/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Custom wake phrase syntax and local inference lifecycle.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define XIAOZHI_WAKE_PHRASE_SIZE    96
#define XIAOZHI_WAKE_DEFAULT_PHRASE "ni hao xiao zhi"

/** @brief Check canonical lowercase pinyin syntax (3-12 syllables); MultiNet validates the phrase itself. */
static inline bool xiaozhi_wake_phrase_valid(const char *s)
{
    if (!s)
    {
        return false;
    }
    unsigned len = 0, syllables = 1, letters = 0;
    for (; s[len]; ++len)
    {
        if (len >= XIAOZHI_WAKE_PHRASE_SIZE - 1)
        {
            return false;
        }
        if (s[len] == ' ')
        {
            if (!letters)
            {
                return false;
            }
            letters = 0;
            ++syllables;
        }
        else if (s[len] >= 'a' && s[len] <= 'z')
        {
            if (++letters > 6)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return letters && syllables >= 3 && syllables <= 12;
}

/** @brief Load the verified model and configure its single canonical pinyin command.
 * @return True when the requested phrase is ready for local detection.
 */
bool xiaozhi_custom_wake_init(const char *phrase);

/** @return PCM sample count required for one inference call, or zero before initialization. */
int xiaozhi_custom_wake_chunk(void);

/** @brief Run inference on one initialized model-sized block of mono 16 kHz PCM. */
bool xiaozhi_custom_wake_detect(int16_t *mono);

/** @brief Release the detector before freeing its borrowed model data. */
void xiaozhi_custom_wake_free(void);
