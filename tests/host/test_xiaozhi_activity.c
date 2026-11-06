/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include "xiaozhi/xiaozhi_activity.h"
#include "xiaozhi/xiaozhi_farewell.h"

static int64_t now;
static bool sound_at_deadline;
int64_t esp_timer_get_time(void)
{
    if (sound_at_deadline) {
        sound_at_deadline = false;
        xiaozhi_activity_note(xiaozhi_activity_generation(), now);
    }
    return now;
}

static void inactivity(void)
{
    now = 1000000; xiaozhi_activity_begin(1);
    assert(xiaozhi_activity_remaining_ms(1) == 30000);
    now += 29999000;
    assert(!xiaozhi_activity_expire(1) && xiaozhi_activity_remaining_ms(1) == 1);
    now += 1000;
    assert(xiaozhi_activity_expire(1));
    assert(!xiaozhi_activity_expire(1) && !xiaozhi_activity_remaining_ms(1));
    xiaozhi_activity_note(1, now + 1000000); /* A completed read cannot revive a closed dialog. */
    assert(!xiaozhi_activity_remaining_ms(1));

    xiaozhi_activity_begin(2); now += 25000000;
    assert(xiaozhi_activity_remaining_ms(2) == 5000);
    xiaozhi_activity_note(2, now);
    assert(xiaozhi_activity_remaining_ms(2) == 30000);
    now += 29999000; assert(!xiaozhi_activity_expire(2));
    now += 1000; sound_at_deadline = true;
    assert(!xiaozhi_activity_expire(2)); /* New sound wins before the atomic claim. */
    assert(xiaozhi_activity_remaining_ms(2) == 30000);

    for (unsigned i = 0; i < 1200; ++i) { /* More than a minute of actual playback. */
        now += 60000; xiaozhi_activity_note(2, now);
        assert(!xiaozhi_activity_expire(2));
    }
    xiaozhi_activity_end(2);
    xiaozhi_activity_begin(3);
    now += 29000000;
    xiaozhi_activity_note(2, now); xiaozhi_activity_end(2);
    assert(xiaozhi_activity_remaining_ms(3) == 1000);
    assert(!xiaozhi_activity_expire(2));
    now += 1000000; assert(xiaozhi_activity_expire(3));
    xiaozhi_activity_begin(UINT32_MAX); xiaozhi_activity_end(UINT32_MAX);
    xiaozhi_activity_begin(0); now += 1000;
    xiaozhi_activity_note(UINT32_MAX, now); xiaozhi_activity_end(UINT32_MAX);
    assert(xiaozhi_activity_remaining_ms(0) == 29999);
}

static void silence_floor(void)
{
    int16_t pcm[960] = {0};
    assert(!xiaozhi_activity_has_sound(pcm, 960));
    for (unsigned i = 0; i < 960; ++i) pcm[i] = 1000;
    assert(!xiaozhi_activity_has_sound(pcm, 960)); /* DC is not sound. */
    for (unsigned i = 0; i < 960; ++i) pcm[i] = (i & 1) ? 16 : -16;
    assert(!xiaozhi_activity_has_sound(pcm, 960));
    for (unsigned i = 0; i < 960; ++i) pcm[i] = (i & 1) ? 320 : -320;
    assert(xiaozhi_activity_has_sound(pcm, 960));
    assert(!xiaozhi_activity_has_sound(NULL, 960));
    assert(!xiaozhi_activity_has_sound(pcm, 0));
    assert(!xiaozhi_activity_has_sound(pcm, 1921));
}

static void playback_tail(void)
{
    xiaozhi_activity_begin(10);
    now += 5000000; xiaozhi_activity_note_playback(10, now);
    assert(xiaozhi_activity_remaining_ms(10) == 30000);
    now += 30000000;
    assert(!xiaozhi_activity_expire(10));
    assert(xiaozhi_activity_remaining_ms(10) == XIAOZHI_OUTPUT_TAIL_MS);
    now += (XIAOZHI_OUTPUT_TAIL_MS - 1) * 1000;
    assert(!xiaozhi_activity_expire(10) && xiaozhi_activity_remaining_ms(10) == 1);
    now += 1000; assert(xiaozhi_activity_expire(10));
    xiaozhi_activity_begin(11); now += 25000000;
    xiaozhi_activity_note_playback(10, now);
    assert(xiaozhi_activity_remaining_ms(11) == 5000);
}

static void farewells(void)
{
    const char *positive[] = {"bye", " BYE BYE! ", "Bye, bye.", "goodbye", "Good bye!", "See you.",
        "SEE YOU LATER", "okay, bye bye", "Thanks, goodbye", "再见", "再見！", "拜拜。", "拜 拜", "好的，再见。",
        "那就这样，拜拜。", "那就這樣，再見", "下次见", "回頭見", "TSCHÜSS!", "Auf Wiedersehen", "ADIOS", "Adiós", "Hasta luego", "Au revoir", "Do widzenia", "Pa pa"};
    const char *negative[] = {NULL, "", " ", "What does bye bye mean?", "What does bye bye mean", "Say goodbye", "Don't say goodbye",
        "Do not say bye bye", "Please translate see you", "I heard bye bye yesterday", "bye bye means goodbye", "bye bye and turn on the light",
        "not goodbye", "bye?", "\"bye bye\"", "'see you'", "不要说再见", "不要說再見", "再见是什么意思", "请翻译拜拜", "他说再见", "“再见”", "「拜拜」", "拜拜灯光", "好再见是什么意思", "好的，请说再见", "see your room", "goodbyes", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80"};
    for (unsigned i = 0; i < sizeof(positive) / sizeof(positive[0]); ++i) {
        if (!xiaozhi_farewell(positive[i])) fprintf(stderr, "Rejected farewell: %s\n", positive[i]);
        assert(xiaozhi_farewell(positive[i]));
    }
    for (unsigned i = 0; i < sizeof(negative) / sizeof(negative[0]); ++i) assert(!xiaozhi_farewell(negative[i]));
}

int main(void)
{
    inactivity(); silence_floor(); playback_tail(); farewells();
    puts("AI Assistant: farewell intent, exact silence boundary, new speech, long playback and stale generations passed");
}
