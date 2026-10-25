/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
const char *xiaozhi_emotion_glyph(const char *emotion);
bool xiaozhi_dialog_expired(uint64_t now_ms, uint64_t activity_ms, unsigned timeout_sec);
