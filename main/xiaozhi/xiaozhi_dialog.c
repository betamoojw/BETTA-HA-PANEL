/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_dialog.h"
#include <string.h>
const char *xiaozhi_emotion_glyph(const char *emotion)
{
    /* Protocol names match Xiaozhi's llm.emotion; Noto Emoji is already bundled. */
    static const struct { const char *name, *glyph; } map[] = {
        {"neutral", "😐"}, {"happy", "🙂"}, {"laughing", "😆"}, {"funny", "😂"},
        {"sad", "😔"}, {"angry", "😠"}, {"crying", "😭"}, {"loving", "😍"},
        {"embarrassed", "😳"}, {"surprised", "😮"}, {"shocked", "😱"},
        {"thinking", "🤔"}, {"winking", "😉"}, {"cool", "😎"}, {"relaxed", "😌"},
        {"delicious", "😋"}, {"kissy", "😘"}, {"confident", "😏"},
        {"sleepy", "😴"}, {"silly", "😜"}, {"confused", "😕"}
    };
    for (unsigned i = 0; i < sizeof(map)/sizeof(map[0]); ++i)
        if (emotion && !strcmp(emotion, map[i].name)) return map[i].glyph;
    return map[0].glyph;
}
bool xiaozhi_dialog_expired(uint64_t now_ms, uint64_t activity_ms, unsigned timeout_sec)
{
    return timeout_sec >= 10 && timeout_sec <= 300 && now_ms >= activity_ms &&
        now_ms - activity_ms >= (uint64_t)timeout_sec * 1000;
}
