/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Worker-owned audio acceptance and turn policy for assistant sessions.
 */
#pragma once
#include <stdbool.h>

/** @brief Session state allowing microphone capture and playback to overlap.
 * Only a user turn confirmed by the server can supersede an active reply. */
typedef struct
{
    bool realtime;
    bool tts;
    bool drop_audio;
} xiaozhi_session_t;

static inline void xiaozhi_session_reset(xiaozhi_session_t *s, bool realtime)
{
    *s = (xiaozhi_session_t) {.realtime = realtime};
}

/** @brief Accept the new reply and clear suppression of the previous reply's audio. */
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

/** @return True if reply audio is not suppressed and does not conflict with half-duplex capture. */
static inline bool xiaozhi_session_accept_audio(const xiaozhi_session_t *s, bool listening)
{
    return !s->drop_audio && (s->realtime || !listening);
}

/** @brief Supersede an active reply in full-duplex mode.
 * @pre The caller has received a confirmed, nonempty server speech transcript, not merely local VAD.
 * @return True when the caller must flush the superseded reply.
 */
static inline bool xiaozhi_session_user_turn(xiaozhi_session_t *s, bool playback)
{
    /* The caller must supply a valid, nonempty server STT event. Local VAD is
     * an activity indicator: residual echo must never abort a reply by itself. */
    if (!s->realtime || s->drop_audio || !(s->tts || playback))
    {
        return false;
    }
    s->drop_audio = true;
    s->tts = false;
    return true;
}
