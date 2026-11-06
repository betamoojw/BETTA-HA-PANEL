/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include "xiaozhi/xiaozhi_session.h"

static void realtime_overlap_and_confirmed_turn(void)
{
    xiaozhi_session_t s;
    xiaozhi_session_reset(&s, true);
    /* The initial question has no reply to supersede. */
    assert(!xiaozhi_session_user_turn(&s, false));
    assert(!s.drop_audio);
    xiaozhi_session_tts_start(&s);
    /* Capture/VAD is not a session-policy input. Without a confirmed user
     * turn, an ongoing reply stays playable for any number of audio frames. */
    for (int frame = 0; frame < 100; ++frame) {
        assert(xiaozhi_session_accept_audio(&s, true));
        assert(s.tts && !s.drop_audio);
    }
    assert(xiaozhi_session_accept_audio(&s, false));
    assert(xiaozhi_session_user_turn(&s, true));
    assert(!s.tts && s.drop_audio);
    assert(!xiaozhi_session_accept_audio(&s, true));
    assert(!xiaozhi_session_accept_audio(&s, false));
    /* Repeated STT and delayed old STOP cannot release the stale-audio gate. */
    assert(!xiaozhi_session_user_turn(&s, true));
    xiaozhi_session_tts_stop(&s);
    assert(!xiaozhi_session_accept_audio(&s, true));
    assert(!xiaozhi_session_user_turn(&s, true));
    xiaozhi_session_tts_stop(&s);
    assert(!xiaozhi_session_accept_audio(&s, false));
    /* The next reply is accepted and can be superseded once, even before its
     * first PCM packet has reached the playback worker. */
    xiaozhi_session_tts_start(&s);
    assert(xiaozhi_session_accept_audio(&s, true));
    assert(xiaozhi_session_user_turn(&s, false));
    assert(!xiaozhi_session_user_turn(&s, false));
}

static void ordinary_stop_and_playback_tail(void)
{
    xiaozhi_session_t s;
    xiaozhi_session_reset(&s, true);
    xiaozhi_session_tts_start(&s);
    /* A normal cloud STOP does not discard the queued end of its response. */
    xiaozhi_session_tts_stop(&s);
    assert(!s.tts && !s.drop_audio);
    assert(xiaozhi_session_accept_audio(&s, true));
    /* A confirmed new question may still supersede that audible tail. */
    assert(xiaozhi_session_user_turn(&s, true));
    assert(!xiaozhi_session_accept_audio(&s, true));
    assert(!xiaozhi_session_user_turn(&s, true));
    xiaozhi_session_tts_start(&s);
    xiaozhi_session_tts_stop(&s);
    /* Once the tail has drained, the next ordinary question needs no flush. */
    assert(!xiaozhi_session_user_turn(&s, false));
    assert(xiaozhi_session_accept_audio(&s, true));
}

static void manual_and_reconnect(void)
{
    xiaozhi_session_t s;
    xiaozhi_session_reset(&s, false);
    xiaozhi_session_tts_start(&s);
    assert(!xiaozhi_session_accept_audio(&s, true));
    assert(xiaozhi_session_accept_audio(&s, false));
    assert(!xiaozhi_session_user_turn(&s, true));
    assert(s.tts && !s.drop_audio);
    xiaozhi_session_tts_stop(&s);
    assert(!xiaozhi_session_user_turn(&s, true));
    assert(xiaozhi_session_accept_audio(&s, false));
    /* Disable/reconnect clears transient reply state and applies the mode. */
    xiaozhi_session_reset(&s, true);
    xiaozhi_session_tts_start(&s);
    assert(xiaozhi_session_user_turn(&s, true));
    xiaozhi_session_reset(&s, true);
    assert(s.realtime && !s.tts && !s.drop_audio);
    assert(xiaozhi_session_accept_audio(&s, true));
    xiaozhi_session_reset(&s, false);
    assert(!s.realtime && !s.tts && !s.drop_audio);
    assert(!xiaozhi_session_accept_audio(&s, true));
}

int main(void)
{
    realtime_overlap_and_confirmed_turn();
    ordinary_stop_and_playback_tail();
    manual_and_reconnect();
    puts("Xiaozhi session: continuous overlap, confirmed user turn, stale stop, tail, manual and reconnect passed");
    return 0;
}
