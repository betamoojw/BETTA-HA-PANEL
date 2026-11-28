/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Shared helpers for emotion display and dialog expiration.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/** @return Borrowed UTF-8 glyph for a protocol emotion; unknown or NULL names select neutral. */
const char *xiaozhi_emotion_glyph(const char *emotion);

/** @brief Compare two timestamps on the same millisecond clock.
 * @return True only for a valid 10-300 second timeout elapsed without a backward time step.
 */
bool xiaozhi_dialog_expired(uint64_t now_ms, uint64_t activity_ms, unsigned timeout_sec);
