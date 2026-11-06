/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>

/* Worker-owned policy; microphone and playback can run at the same time.
 * Only a user turn confirmed by the server can supersede an active reply. */
typedef struct {
    bool realtime;
    bool tts;
    bool drop_audio;
} xiaozhi_session_t;

static inline void xiaozhi_session_reset(xiaozhi_session_t *s, bool realtime)
{
    *s = (xiaozhi_session_t){.realtime = realtime};
}

static inline void xiaozhi_session_tts_start(xiaozhi_session_t *s)
{
    s->tts = true;
    s->drop_audio = false;
}

static inline void xiaozhi_session_tts_stop(xiaozhi_session_t *s)
{
    s->tts = false;
    /* An old stop can follow a new user turn. Only a new start releases the gate. */
}

static inline bool xiaozhi_session_accept_audio(const xiaozhi_session_t *s,
                                               bool listening)
{
    return !s->drop_audio && (s->realtime || !listening);
}

static inline bool xiaozhi_session_user_turn(xiaozhi_session_t *s, bool playback)
{
    /* The caller must supply a valid, nonempty server STT event. Local VAD is
     * an activity indicator: residual echo must never abort a reply by itself. */
    if (!s->realtime || s->drop_audio || !(s->tts || playback)) return false;
    s->drop_audio = true;
    s->tts = false;
    return true;
}
