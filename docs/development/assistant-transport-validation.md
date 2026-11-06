<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# AI Assistant transport validation

## COM24 observation, 2026-11-06

The owner reported interrupted speaker replies and **Connection interrupted; try
again** after installing the flip-clock screensaver feature on the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1 (`panel10`). Read-only HTTP diagnostics identified
source `0930affe-dirty`, built November 6 at 19:38:02, ELF prefix `945522baf`.
This is the previously flashed image, not the repair described below.

- The assistant reported seven connection errors and no normal peer closes.
  The latest ending was `send_error`, without a close code, TLS error, socket
  error or failed HTTP handshake.
- At 20:47:17 and 20:55:17 the log recorded a 500 ms WebSocket lock timeout
  immediately before `send_error`. A further lock timeout at 20:51:10 preceded
  `local_error`. Four other recorded endings were also `local_error`.
- The captured boot reported no Wi-Fi disconnects or reconnects, valid OTA
  state and no retained crash reports. These observations rule out neither all
  service faults nor faults from earlier boots.
- The current settings selected the flip screensaver and 12-hour format.
  Only HTTP GET requests were used; serial was not opened. No settings, pages,
  history, credentials or flash contents were written during investigation.

Private diagnostics remain in the ignored local directory
`.local/validation/assistant-audio-interruption-2026-11-06/`.
They include version/tasks, logs, reboot history, assistant status and layout/settings.
Do not publish raw transcripts or device configuration.

## Failure mechanism

The client shares one TLS lock across receive and send. The old application
treated every negative send result as a failed connection, including a temporary
failure to obtain that lock before any bytes were written. Teardown then stopped
speaker playback and surfaced the error popup. The observed lock-timeout/send-error
pairs establish this failure path without a Wi-Fi disconnection.

Review also found immediate failures when incoming or decoded-playback queues
were full. Cloud replies can arrive faster than real-time playback, so a brief
burst could terminate a healthy session. Host fault injection reproduces this
queue behavior; the older device logs do not distinguish it from other local
audio/protocol errors. It is a confirmed code defect, not a proven explanation
for every recorded `local_error`.

The flip-clock feature did not change assistant transport or codec code. The
timing of the report does not establish a rendering defect. Keep the shared TLS
lock: a previous separate-lock image had a verified TLS crash, documented in
[Components and patches](components.md#websocket-tls-ownership).

## Repair and regression scope

The worker retains a copied, bounded FIFO of unsent packets. Only an explicitly
reported pre-write lock timeout is retryable. A 20 ms lock attempt yields to
receive processing; transport writes retain 500 ms and queued packets expire
after five seconds without deadline extension. Partial writes are never retried.
Control messages remain ordered; microphone capture retains its existing bounded
queue. Teardown frees all outgoing payloads.

The receive callback checks stop every 50 ms while waiting for an incoming slot,
with a fixed five-second stall deadline. Decoded playback waits at most 150 ms,
allowing a maximum 120 ms frame to drain. The worker tries
outgoing packets immediately after freeing an incoming slot, before waiting for
playback, so that backpressure does not indefinitely hold the TLS lock against uploads.
Persistent stalls still fail. No timeout preference, pairing, clock or display
setting changes are part of this repair.

Regression coverage uses the production C functions and fetched WebSocket source:

- Busy-lock retries preserve payload copies and FIFO order, count only actual
  transmissions, stop at a fixed deadline, and clear on peer/local closure.
- Real/partial writes are not replayed; normal CLOSE and late-error precedence,
  subsequent wakes, finite queue capacity and allocation failures remain covered.
- Playback burst tests retain every queued frame in order when a consumer drains
  within the wait, and still fail when it does not drain.
- Worker ordering, audio inactivity, farewell, manual commands and stale-event
  cancellation are tested. Deliberately restored failure paths must fail the tests.
- Component patch tests cover pristine source, migration from the previous patch,
  repeat application and rejection of unknown source.

## Initial repair local results

The initial application repair passed five Python-driven C regression tests
(`test_xiaozhi_*.py`), including 13 connection scenarios, nine dialogue scenarios,
ten deliberately broken controls and the outbox allocation/deadline/FIFO tests.
The WebSocket suite passed four tests, including 12 transport scenarios and four
original/unsafe negative controls. The standalone duplex C test passed burst
backpressure, frame order, AEC routing, cancellation and teardown checks.

ESP-IDF 6.1 `scripts/build.ps1 panel10 reconfigure build`, followed by an incremental
build after the final worker-ordering edit, completed successfully. The resulting
v0.9.1 image is based on `34e40009` with the uncommitted repair:

| Artifact | SHA256 |
| --- | --- |
| App | `a0d64f1a94351af54f78274fdcbf1cc0ee21143881a89a49bdbc9d08c4470837` |
| ELF | `4f836d9de645edeeb4dee8726f3ea20bb2f2b43d0094a1fea14b54a1e60a5cfc` |

The app occupies `0x6595d0` bytes of the `0x680000` slot, leaving `0x26a30`
(158,256 bytes, about 2%). Existing dependency Kconfig notices remain; the board
profile and TLS lock setting are unchanged. The initial device trial below used this image.

Final read-only snapshots matched the original layout and saved settings,
excluding the live Wi-Fi signal reading and local clock time. Flip style and
12-hour format remained selected. Source/document size checks, whitespace checks
and the strict locked documentation build passed.

## Initial repair trial, COM24

The owner authorized an app-only update on 2026-11-06. The P4 stub identified the
same v3.2 device, validated the installed table and active `ota_1` selection, and
verified the prior app before preserving its image. App write/readback passed;
metadata/NVS (128 KiB), coredump (256 KiB) and all LittleFS bytes (18.625 MiB)
matched before boot. Running source `34e40009-dirty`, build time `21:17:16`, ELF
prefix `4f836d9de`, valid OTA, HA and network time were verified. Eight pages,
35 widgets, saved settings, camera/KNX configuration and theme were unchanged;
the selected flip clock retained 12-hour format. No graph widgets were in the
layout, so graph preservation evidence is the unchanged complete storage region.

This image **failed** the longer voice trial. The owner reported continued sound
interruptions/connection guidance. At 21:34:19 the new diagnostic identified
`Incoming queue did not drain within 250 ms`, followed by `local_error`. The first
observed session received 169 audio frames before ending. Later status counted a
second local error. No crash was retained. Do not treat host success as hardware
acceptance or this image as an accepted voice baseline.

Task diagnostics placed the session, capture and playback tasks on core 1 along
with priority-20 LVGL. One captured load sample was 10%/60% on cores 0/1. This shows
uneven placement, not a measured causal latency trace. The follow-up repair pins
voice tasks to core 0, replaces the short receive wait with cancellable waits
bounded by five seconds, and logs unusually slow send/encode/decode/playback work.
Tests now inject 750 ms of consumer delay, a persistent five-second stall and
cancelled full queues, with a negative control restoring the failed short deadline.

Private device evidence is under
`.local/validation/assistant-audio-fix-flash-2026-11-06/`.

## Follow-up repair

The updated session and duplex tests passed, including the delayed-consumer,
bounded-stall and cancellation cases, eleven connection/dialogue negative controls,
and explicit core-0 task-creation checks. ESP-IDF 6.1 rebuilt `panel10` successfully.
The image retains v0.9.1/source `34e40009-dirty`, with build stamp
`Nov  6 2026 21:43:28` and the additional uncommitted repair:

| Artifact | SHA256 |
| --- | --- |
| App | `31e990938a6fe2ba6914f5fd7f24726b97418b7c43ff6838c5c57a924cfc6ba6` |
| ELF | `1f4fea4a9f302997aaf74666c791e4641400fa72c34c69196bb51ba4c4d0e084` |

It uses 6,658,176 bytes, leaving 157,568 bytes in the unchanged application slot.
Follow-up device evidence is under
`.local/validation/assistant-audio-fix2-flash-2026-11-06/`.
The second app-only COM24 write and readback passed. Protected metadata/NVS,
coredump and the complete LittleFS region again matched before boot. Running
build `21:43:28`, ELF prefix `1f4fea4a9`, valid OTA, HA and clock synchronization
were verified. The task snapshots confirmed session/capture/playback on core 0
and LVGL on core 1; one active-voice load sample was 43%/39% on cores 0/1.

The owner confirmed continuous replies, successful interruption and reawakening,
and no connection-interrupted guidance. The bounded observations covered four
wake-origin sessions and several replies, including a roughly 32-second
speaking/listening interval. Final counters were 1,509 received audio frames,
3,084 uploaded frames, zero connection errors and zero capture drops. Farewell
closed sessions normally and later wakes started new sessions. The `barge_ins`
counter stayed zero; interruption acceptance here is the owner's physical
confirmation, not a measured count of that internal event.

No retained crash or unexpected restart was observed. During the captured voice
workload the internal-heap low-water was 58,192 bytes; capture/playback stack
low-water remained above 6,000/3,300 bytes in the sampled sessions. This is one
bounded device trial, not an endurance or acoustic echo-cancellation measurement.

Final layout and saved settings matched the task's original snapshots, excluding
live time, Wi-Fi signal and connection state. All eight pages and 35 widgets,
theme, camera/KNX configuration, assistant preferences and pairing were retained.
The clock remained flip style, 12-hour format, 60-second start and 300-second off.
Two idle screen snapshots showed a centered clock with Friday/date and PM,
advancing from 09:59:04 to 09:59:10; settings were not changed to trigger it.
The owner also confirmed smooth, centered flip-clock display, clear weekday/date,
retained 12-hour format and successful physical touch wake. This completes the
requested voice and clock trial on this COM24 panel; the older failed image is
retained only as investigation evidence.

## Pre-commit review, 2026-11-06

Review covered TLS lock retries, fixed deadlines, receive/playback waits,
core placement and teardown ownership. Two additional boundary defects were
fixed with deterministic C regressions and failing negative controls:

- A positive short transport write previously started a new continuation frame
  despite the current frame's incomplete payload. The component now aborts on
  any byte-count mismatch, including a short later fragment. Write failures
  cannot alias the retryable pre-write lock result. Complete fragmented sends
  still succeed. This is a code-level finding, not a fault observed in the
  accepted COM24 trial.
- The stop flag previously followed the audio-task join. A receive callback
  waiting on a full queue could reach its deadline during intentional teardown.
  Teardown now publishes cancellation before joining audio tasks; the regression
  cancels in one 50 ms wait, clears pending output and permits the next wake.
  Genuine errors delivered during stop retain their diagnostics.

The local review rechecked the archived app/ELF hashes, running build stamp,
valid OTA, protected-region checksums and original/final layout and settings.
All 201 recorded voice samples had zero connection errors and capture drops.
Core-0 voice/core-1 LVGL placement and the post-touch active display were verified
from saved snapshots. The owner's voice and clock confirmations remain the
physical acceptance evidence. No device API, serial access, flash, restart or
settings change was performed during this review.

The historical `repair-validation.json` recorded only the then-unstaged document,
so it is not a source-file manifest for the accepted firmware. The image hashes,
boot record and runtime stamp above identify that image. The review's separate
manifest covers both staged and unstaged changed source files.

The two review fixes are **not installed on COM24**. The accepted `21:43:28`
image remains the hardware baseline; local regression/build results below do
not extend hardware acceptance to the added boundary fixes.

Review validation passed (all commands exited 0, no C suite skipped):

- Nine Python-driven C test methods: 16 transport, 14 connection and nine dialogue
  scenarios, outbox checks and 19 negative controls across the suites.
- Standalone duplex C tests: bounded backpressure, frame order, capture/playback
  core assignment, cancellation, failed joins and resource cleanup.
- ESP-IDF 6.1 `scripts/build.ps1 panel10 reconfigure build`; sdkconfig remained
  byte-identical. App size remains 6,658,176 bytes, with 157,568 bytes free.
  Existing Kconfig notices and the 2%-free application-slot warning remain.
- Component patch check, source/document size limits, whitespace checks and
  strict locked documentation build; affected pages checked over HTTP and
  file URLs at desktop/mobile widths and enlarged page scale.

The review ELF retains runtime stamp `Nov  6 2026 21:43:28` and source
`34e40009-dirty`, matching the hardware baseline's labels. Its app-descriptor
compile stamp is `09:44:54`; neither timestamp identifies this repair uniquely.
Use the distinct app/ELF hashes below to identify the unflashed review artifact:

| Artifact | SHA256 |
| --- | --- |
| Review app, not flashed | `300497ce05f40e1c5b0c3ea721320d5025fb9aba668e35deced2dc5928caa407` |
| Review ELF | `d2ffb042422b0a37ed45ff8defa87030fdbac4d994e30036e2d4e4d4043a1638` |

Private logs, evidence reconciliation and the complete review source manifest
are in `.local/validation/assistant-audio-fix-review-2026-11-06/`.
