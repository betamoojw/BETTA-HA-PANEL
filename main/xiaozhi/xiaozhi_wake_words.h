/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Stable stored IDs for wake-word model names and labels.
 */
#pragma once
#include <stddef.h>
/** @brief Stable NVS/API IDs; never reorder existing entries. */
#define XIAOZHI_WAKE_WORD_COUNT 3

/** @return Borrowed model name, or NULL for custom and out-of-range IDs. */
static inline const char *xiaozhi_wake_word_model(int id)
{
    static const char *const names[] = {"wn9_nihaoxiaozhi", "wn9_hiesp", "wn9_jarvis_tts"};
    return id >= 0 && id < XIAOZHI_WAKE_WORD_COUNT ? names[id] : NULL;
}

/** @return Borrowed preset label, or NULL for custom and out-of-range IDs. */
static inline const char *xiaozhi_wake_word_label(int id)
{
    static const char *const labels[] = {"Ni hao Xiao Zhi", "Hi ESP", "Jarvis"};
    return id >= 0 && id < XIAOZHI_WAKE_WORD_COUNT ? labels[id] : NULL;
}
