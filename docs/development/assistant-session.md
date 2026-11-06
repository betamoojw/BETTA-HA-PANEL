<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# AI Assistant session policy

For operation, see [AI Assistant](../xiaozhi-ai.md#conversation). The session
worker owns conversation lifetime; the native popup and web editor display its
state. Neither a UI poll nor a text update starts a separate inactivity clock.
`xiaozhi_activity.c` shares generation-tagged audio activity across the session
and audio tasks. `xiaozhi_farewell.c` owns bounded user-farewell classification.

## Audio inactivity

Opening a dialogue starts a fixed 30,000 ms deadline. Effective user speech
after echo cancellation or actual successful, non-silent speaker playback
renews it. Wake-only capture is excluded. Manual capture uses the same speech
qualification instead of counting every captured frame as activity.

Successful, non-muted speaker writes include a bounded 120 ms I2S/DMA tail
allowance before the 30-second interval. This conservatively avoids expiring
before buffered sound finishes; it is not a physical acoustic measurement.
Microphone activity has no extra tail allowance.

Silence, background noise, queued audio, TTS text/status, network heartbeats,
MCP responses and scrolling cannot extend the deadline. A long audible response
continues to renew it. Frequency-bar values are display observations, not input
to speech detection, timeout policy or interruption decisions.

The same audio generation guards observations and expiration. A late completion
from an old capture/write cannot renew a newly opened conversation. Checking
expiry and accepting current audio activity must not leave a race that closes
a conversation renewed at its deadline.

## Farewell and normal closure

Only a confirmed user STT event can request farewell closure. Normalize case,
spacing and ordinary terminal punctuation, then match bounded farewell phrases.
Examples include “bye bye”, “see you”, “再见” and “拜拜”. Do not match an arbitrary
substring: “What does bye bye mean?”, a quoted phrase or “不要说再见” is not an
instruction to close. TTS replies never enter this classifier.

Farewell, silence expiry, explicit Disconnect and a normal service close end
the current conversation. Teardown stops capture/playback, clears visual audio
state and restores the saved local wake configuration. It does not disable the
assistant or erase pairing. A closing dialogue cannot consume the next wake's
events. An explicit manual opening starts a fresh deadline without contacting
the cloud; pressing Talk starts manual capture as before.

Full duplex, server-confirmed speech interruption and the existing AEC remain
independent. A detected local speech interval can renew inactivity but cannot
abort playback merely because residual echo resembles speech.

## Status and views

`GET /api/xiaozhi` preserves its path and existing status fields. The additional
dialogue fields expose the worker's lifetime:

| Field | Meaning |
| --- | --- |
| `dialog_epoch` | Read-only, random nonzero identifier for this device boot |
| `dialog_seq` | Generation of the latest opened dialogue |
| `dialog_active` | Current dialogue is active |
| `dialog_voice_opened` | Wake-origin dialogue; manual controls otherwise |
| `dialog_end_seq` | Generation to which the latest end applies |
| `dialog_end_reason` | Normal end reason or `error` |
| `silence_remaining_ms` | Backend-owned remaining audio inactivity interval |

The native view matches an end to its displayed generation before hiding. Both
views show the gentle last-five-second hint only while applicable; new effective
audio removes it. The web conversation view ends without closing the settings
page. Old HTTP replies must not reintroduce stale text, bars or an ended session.
New HTTP `dialog_turn`, `dismiss`, `start_turn` and `stop_turn` commands carry
top-level `epoch` and `value` fields: `epoch` is the displayed `dialog_epoch`, and
`value` is the displayed `dialog_seq`. The API checks the boot identifier before queuing the
command; the worker checks the dialogue generation before acting. This prevents
an old browser page from controlling a different device boot and a delayed
pointer/key release from stopping a newer turn. `dialog_turn` opens a manual
dialogue only when the supplied generation still matches and no dialogue is
active; a delayed open cannot take over a new wake. The first manual recording
in the web view sends `dialog_turn` before `start_turn`. Legacy `dialog`, `start`,
`stop` and `disconnect` commands remain supported.

Transport faults keep the existing [connection diagnosis](components.md#xiaozhi-connection-endings).
A fault racing a local stop retains its diagnostic path; normal ending must not
hide it or overwrite its retained details. The view keeps recovery guidance for
real failures. Dismissing the current ended error hides its recovery view and
returns to standby without clearing fault diagnostics; old-generation acknowledgements are ignored.
`connection_*` fields can describe normal peer closes before the
first fault; after a fault, only a later fault replaces those details. Normal
endings still update `dialog_end_reason` and peer-close counters. A subsequent
wake establishes a new connection.

## Naming and compatibility

All product-facing labels use **AI Assistant**, including Chinese catalogs.
Existing `xiaozhi` NVS names, API/resource routes, translation keys and source
identifiers remain compatible. Stored wake phrases, cloud credentials and
preference values survive the display-name change. A legacy bounded `timeout`
command remains accepted for compatibility, but cannot change the fixed
30-second runtime interval or rewrite its retained legacy value.

The Xiaozhi service name, `xiaozhi.me`, upstream URLs and license attribution
remain factual. Do not rename real wake-model phrases or rewrite user speech,
cloud responses, historical crash records or accepted firmware evidence.

## Regression boundary

Tests cover positive and negative farewell phrases, duplicate events, TTS
isolation, 29,999/30,000 ms, new audio at expiry, long playback, silent manual
capture and non-audio traffic. Lifecycle checks cover close/reopen, quick wake,
stale-generation callbacks, cross-boot HTTP commands, delayed control releases,
error recovery, clearing spectrum and manual/wake origins. Existing duplex,
AEC, interruption and transport checks remain required.

Host tests, browser/SDL checks and a panel10 build do not establish acoustic or
physical display/touch acceptance. A new device trial needs separate authority;
retain earlier accepted firmware and crash evidence in the meantime.
