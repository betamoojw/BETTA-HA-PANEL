/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_farewell.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool normalize(const char *text, char out[160])
{
    if (!text) return false;
    size_t used = 0, read = 0;
    bool space = false;
    while (read < 192 && text[read]) {
        const unsigned char *p = (const unsigned char *)text + read;
        uint32_t c = *p; unsigned bytes = 1;
        if (c >= 0x80) {
            if ((c & 0xe0) == 0xc0) { c &= 0x1f; bytes = 2; }
            else if ((c & 0xf0) == 0xe0) { c &= 0x0f; bytes = 3; }
            else if ((c & 0xf8) == 0xf0) { c &= 0x07; bytes = 4; }
            else return false;
            for (unsigned i = 1; i < bytes; ++i) {
                if (!p[i] || (p[i] & 0xc0) != 0x80) return false;
                c = (c << 6) | (p[i] & 0x3f);
            }
            if ((bytes == 2 && c < 0x80) || (bytes == 3 && c < 0x800) ||
                (bytes == 4 && c < 0x10000) || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
        }
        read += bytes;
        /* Quoted examples and questions are deliberately not goodbye commands. */
        if (c == '"' || c == '\'' || c == '?' || c == 0xff1f ||
            (c >= 0x2018 && c <= 0x201f) || (c >= 0x300c && c <= 0x300f)) return false;
        if (c <= 0x20 || c == ',' || c == '.' || c == '!' || c == 0x3000 ||
            c == 0xff0c || c == 0x3001 || c == 0x3002 || c == 0xff01) { space = used != 0; continue; }
        if (space) { if (used + 1 >= 160) return false; out[used++] = ' '; space = false; }
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        else if ((c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xde)) c += 0x20;
        if (used + bytes >= 160) return false;
        if (c < 0x80) out[used++] = (char)c;
        else if (c < 0x800) { out[used++] = (char)(0xc0 | (c >> 6)); out[used++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) {
            out[used++] = (char)(0xe0 | (c >> 12)); out[used++] = (char)(0x80 | ((c >> 6) & 63)); out[used++] = (char)(0x80 | (c & 63));
        } else return false;
    }
    if (text[read]) return false;
    out[used] = 0;
    return used != 0;
}

bool xiaozhi_farewell(const char *text)
{
    char normalized[160];
    if (!normalize(text, normalized)) return false;
    static const char *const phrases[] = {
        "bye", "bye bye", "byebye", "goodbye", "good bye", "see you", "see you later", "see ya",
        "再见", "再見", "拜拜", "拜 拜", "下次见", "下次見", "下次聊", "回头见", "回頭見",
        "tschüss", "tschuess", "auf wiedersehen", "adiós", "adios", "hasta luego", "au revoir", "do widzenia", "pa pa"
    };
    static const char *const prefixes[] = {"", "ok ", "okay ", "alright ", "thanks ", "thank you ",
        "好 ", "好的 ", "谢谢 ", "謝謝 ", "那就这样 ", "那就這樣 ", "好", "好的", "谢谢", "謝謝", "那就这样", "那就這樣"};
    /* Exact short-utterance matching, not a substring: negations, instructions,
     * translations and quoted examples retain their ordinary conversation. */
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        size_t n = strlen(prefixes[i]);
        if (strncmp(normalized, prefixes[i], n)) continue;
        for (size_t j = 0; j < sizeof(phrases) / sizeof(phrases[0]); ++j)
            if (!strcmp(normalized + n, phrases[j])) return true;
    }
    return false;
}
